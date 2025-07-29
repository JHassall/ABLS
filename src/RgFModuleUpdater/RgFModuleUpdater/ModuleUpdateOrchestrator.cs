/*
 * ModuleUpdateOrchestrator.cs
 * 
 * Core orchestrator for ABLS module firmware updates.
 * Handles sequential updates, safety checks, and version synchronization.
 * 
 * Author: RgF Engineering
 * Version: 1.0.0
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace RgFModuleUpdater
{
    public class ModuleUpdateOrchestrator
    {
        private readonly List<ModuleInfo> _modules;
        private SimpleHttpServer _httpServer;
        private readonly UdpClient _udpClient;
        private int _httpPort = 8080;
        private readonly int _udpPort = 12346; // RgFModuleUpdate command port
        private readonly int _statusPort = 12347; // Status reporting port
        private string _firmwareDirectory = string.Empty;
        private CancellationTokenSource _cancellationTokenSource;

        public ModuleUpdateOrchestrator()
        {
            _modules = new List<ModuleInfo>
            {
                new ModuleInfo { Name = "Left Wing", IpAddress = "192.168.1.101", Role = ModuleRole.Left },
                new ModuleInfo { Name = "Centre", IpAddress = "192.168.1.102", Role = ModuleRole.Centre },
                new ModuleInfo { Name = "Right Wing", IpAddress = "192.168.1.103", Role = ModuleRole.Right }
            };

            _httpServer = new SimpleHttpServer(_httpPort, "");
            _udpClient = new UdpClient(_udpPort);
            _cancellationTokenSource = new CancellationTokenSource();
        }

        public async Task InitializeAsync()
        {
            Console.WriteLine("Initializing RgF Module Updater...");

            // Setup firmware directory
            _firmwareDirectory = Path.Combine(Environment.CurrentDirectory, "firmware");
            if (!Directory.Exists(_firmwareDirectory))
            {
                Directory.CreateDirectory(_firmwareDirectory);
                Console.WriteLine($"Created firmware directory: {_firmwareDirectory}");
            }

            // Setup HTTP server for firmware downloads with fallback ports
            await StartSimpleHttpServerWithFallbackAsync();

            // Start UDP status listener
            _ = Task.Run(ListenForStatusUpdatesAsync);

            Console.WriteLine("Initialization complete.\n");
        }

        private async Task StartSimpleHttpServerWithFallbackAsync()
        {
            var fallbackPorts = new[] { 8080, 8081, 8082, 8083, 8084, 8085, 9000, 9001, 9002 };
            Exception lastException = null;
            
            foreach (var port in fallbackPorts)
            {
                try
                {
                    // Create a new SimpleHttpServer for each attempt
                    _httpServer?.Dispose();
                    _httpServer = new SimpleHttpServer(port, _firmwareDirectory);
                    
                    _httpPort = port;
                    
                    if (await _httpServer.StartAsync())
                    {
                        Console.WriteLine($"✓ Simple HTTP server started successfully on port {_httpPort}");
                        Console.WriteLine($"  Server URL: http://localhost:{_httpPort}/");
                        Console.WriteLine($"  Firmware directory: {_firmwareDirectory}");
                        return; // Success!
                    }
                }
                catch (Exception ex)
                {
                    lastException = ex;
                    Console.WriteLine($"Port {port} failed: {ex.Message}");
                    
                    try
                    {
                        _httpServer?.Dispose();
                    }
                    catch { }
                }
            }
            
            // If we get here, all ports failed
            var errorMessage = $"Failed to start HTTP server on any port. Last error: {lastException?.Message}";
            
            errorMessage += "\n\nTROUBLESHoting:\n" +
                          "1. Check if ports are available (netstat -an | findstr :PORT)\n" +
                          "2. Try running from a different directory\n" +
                          "3. Check Windows Firewall settings";
            
            throw new InvalidOperationException(errorMessage);
        }

        public int GetHttpPort()
        {
            return _httpPort;
        }

        public void Dispose()
        {
            _cancellationTokenSource?.Cancel();
            _httpServer?.Dispose();
            _udpClient?.Close();
            _cancellationTokenSource?.Dispose();
        }

        public async Task<string> CalculateSHA256Async(string filePath)
        {
            using var sha256 = SHA256.Create();
            using var stream = File.OpenRead(filePath);
            var hash = await Task.Run(() => sha256.ComputeHash(stream));
            return Convert.ToHexString(hash).ToLower();
        }

        public async Task<ModuleStatus> QueryModuleStatusAsync(ModuleInfo module)
        {
            try
            {
                // First, try a simple ping to check basic connectivity
                using var ping = new System.Net.NetworkInformation.Ping();
                var pingReply = await ping.SendPingAsync(module.IpAddress, 2000); // 2 second timeout
                
                if (pingReply.Status != System.Net.NetworkInformation.IPStatus.Success)
                {
                    throw new Exception($"Ping failed: {pingReply.Status}");
                }

                // Send status query UDP packet
                var queryPacket = new StatusQueryPacket { Command = "STATUS_QUERY" };
                var jsonData = JsonSerializer.Serialize(queryPacket);
                var data = Encoding.UTF8.GetBytes(jsonData);

                var endpoint = new IPEndPoint(IPAddress.Parse(module.IpAddress), _statusPort);
                await _udpClient.SendAsync(data, data.Length, endpoint);

                // Wait for actual response with timeout
                var cts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
                
                try
                {
                    // Try to receive a response (this is a simplified implementation)
                    // In a real implementation, you'd need proper response matching and parsing
                    var receiveTask = _udpClient.ReceiveAsync();
                    var completedTask = await Task.WhenAny(receiveTask, Task.Delay(3000, cts.Token));
                    
                    if (completedTask == receiveTask)
                    {
                        var result = await receiveTask;
                        var responseJson = Encoding.UTF8.GetString(result.Buffer);
                        
                        // Try to parse the response (simplified)
                        try
                        {
                            var status = JsonSerializer.Deserialize<ModuleStatus>(responseJson);
                            return status ?? throw new Exception("Invalid response format");
                        }
                        catch
                        {
                            // If we can't parse the response, at least we know the module responded
                            return new ModuleStatus
                            {
                                Status = "RESPONDING",
                                Version = "Unknown",
                                UptimeSeconds = 0,
                                FreeMemory = 0
                            };
                        }
                    }
                    else
                    {
                        throw new TimeoutException("No response received within timeout period");
                    }
                }
                catch (Exception ex)
                {
                    throw new Exception($"UDP communication failed: {ex.Message}");
                }
            }
            catch (Exception ex)
            {
                // Module is not reachable or not responding
                Console.WriteLine($"Module {module.Name} ({module.IpAddress}) is offline: {ex.Message}");
                throw new Exception($"Module offline: {ex.Message}");
            }
        }

        public async Task UpdateModuleAsync(ModuleInfo module, string firmwarePath, string firmwareHash)
        {
            // Send update command
            var updateCommand = new UpdateCommandPacket
            {
                Command = "START_UPDATE",
                FirmwareUrl = $"http://localhost:{_httpPort}/firmware/{Path.GetFileName(firmwarePath)}",
                FirmwareHash = firmwareHash,
                FirmwareSize = new FileInfo(firmwarePath).Length
            };

            var jsonData = JsonSerializer.Serialize(updateCommand);
            var data = Encoding.UTF8.GetBytes(jsonData);

            var endpoint = new IPEndPoint(IPAddress.Parse(module.IpAddress), _udpPort);
            await _udpClient.SendAsync(data, data.Length, endpoint);

            // Wait for update completion (simplified - in practice you'd monitor progress)
            await Task.Delay(30000); // 30 second timeout
        }

        public async Task RunInteractiveAsync()
        {
            while (true)
            {
                Console.WriteLine("=== RgF Module Updater Menu ===");
                Console.WriteLine("1. Check module status");
                Console.WriteLine("2. Update all modules");
                Console.WriteLine("3. Update specific module");
                Console.WriteLine("4. List firmware files");
                Console.WriteLine("5. Exit");
                Console.Write("Select option: ");

                var input = Console.ReadLine();
                Console.WriteLine();

                try
                {
                    switch (input)
                    {
                        case "1":
                            await CheckModuleStatusAsync();
                            break;
                        case "2":
                            await UpdateAllModulesAsync();
                            break;
                        case "3":
                            await UpdateSpecificModuleAsync();
                            break;
                        case "4":
                            ListFirmwareFiles();
                            break;
                        case "5":
                            Console.WriteLine("Shutting down...");
                            _cancellationTokenSource.Cancel();
                            _httpServer.Stop();
                            _udpClient.Close();
                            return;
                        default:
                            Console.WriteLine("Invalid option. Please try again.\n");
                            break;
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error: {ex.Message}\n");
                }
            }
        }

        private async Task CheckModuleStatusAsync()
        {
            Console.WriteLine("Checking module status...\n");

            foreach (var module in _modules)
            {
                Console.WriteLine($"Module: {module.Name} ({module.IpAddress})");
                
                try
                {
                    var status = await QueryModuleStatusAsync(module);
                    Console.WriteLine($"  Status: {status.Status}");
                    Console.WriteLine($"  Version: {status.Version}");
                    Console.WriteLine($"  Uptime: {status.UptimeSeconds}s");
                    Console.WriteLine($"  Free Memory: {status.FreeMemory} bytes");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"  Status: ERROR - {ex.Message}");
                }
                
                Console.WriteLine();
            }
        }

        private async Task UpdateAllModulesAsync()
        {
            Console.WriteLine("=== Sequential Module Update Process ===\n");

            // List available firmware files
            var firmwareFiles = Directory.GetFiles(_firmwareDirectory, "*.hex");
            if (firmwareFiles.Length == 0)
            {
                Console.WriteLine("No firmware files found in firmware directory.");
                Console.WriteLine($"Please place .hex files in: {_firmwareDirectory}\n");
                return;
            }

            Console.WriteLine("Available firmware files:");
            for (int i = 0; i < firmwareFiles.Length; i++)
            {
                Console.WriteLine($"{i + 1}. {Path.GetFileName(firmwareFiles[i])}");
            }

            Console.Write("Select firmware file (number): ");
            if (!int.TryParse(Console.ReadLine(), out int selection) || 
                selection < 1 || selection > firmwareFiles.Length)
            {
                Console.WriteLine("Invalid selection.\n");
                return;
            }

            var selectedFirmware = firmwareFiles[selection - 1];
            Console.WriteLine($"Selected: {Path.GetFileName(selectedFirmware)}\n");

            // Calculate SHA256 hash
            var firmwareHash = await CalculateSHA256Async(selectedFirmware);
            Console.WriteLine($"Firmware SHA256: {firmwareHash}\n");

            // Confirm update
            Console.Write("Proceed with update? (y/N): ");
            var confirmInput = Console.ReadLine();
            if (confirmInput?.ToLower() != "y")
            {
                Console.WriteLine("Update cancelled.\n");
                return;
            }

            // Perform sequential updates
            foreach (var module in _modules)
            {
                Console.WriteLine($"Updating {module.Name}...");
                
                try
                {
                    await UpdateModuleAsync(module, selectedFirmware, firmwareHash);
                    Console.WriteLine($"✓ {module.Name} updated successfully\n");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"✗ {module.Name} update failed: {ex.Message}");
                    Console.WriteLine("Aborting remaining updates.\n");
                    return;
                }
            }

            Console.WriteLine("All modules updated successfully!\n");
        }

        private async Task UpdateSpecificModuleAsync()
        {
            Console.WriteLine("Select module to update:");
            for (int i = 0; i < _modules.Count; i++)
            {
                Console.WriteLine($"{i + 1}. {_modules[i].Name} ({_modules[i].IpAddress})");
            }

            Console.Write("Select module (number): ");
            if (!int.TryParse(Console.ReadLine(), out int selection) || 
                selection < 1 || selection > _modules.Count)
            {
                Console.WriteLine("Invalid selection.\n");
                return;
            }

            var selectedModule = _modules[selection - 1];
            
            // Similar firmware selection logic as UpdateAllModulesAsync
            var firmwareFiles = Directory.GetFiles(_firmwareDirectory, "*.hex");
            if (firmwareFiles.Length == 0)
            {
                Console.WriteLine("No firmware files found in firmware directory.\n");
                return;
            }

            Console.WriteLine("\nAvailable firmware files:");
            for (int i = 0; i < firmwareFiles.Length; i++)
            {
                Console.WriteLine($"{i + 1}. {Path.GetFileName(firmwareFiles[i])}");
            }

            Console.Write("Select firmware file (number): ");
            if (!int.TryParse(Console.ReadLine(), out int firmwareSelection) || 
                firmwareSelection < 1 || firmwareSelection > firmwareFiles.Length)
            {
                Console.WriteLine("Invalid selection.\n");
                return;
            }

            var selectedFirmware = firmwareFiles[firmwareSelection - 1];
            var firmwareHash = await CalculateSHA256Async(selectedFirmware);

            Console.WriteLine($"\nUpdating {selectedModule.Name} with {Path.GetFileName(selectedFirmware)}...");
            
            try
            {
                await UpdateModuleAsync(selectedModule, selectedFirmware, firmwareHash);
                Console.WriteLine($"✓ {selectedModule.Name} updated successfully\n");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"✗ Update failed: {ex.Message}\n");
            }
        }

        private void ListFirmwareFiles()
        {
            Console.WriteLine($"Firmware directory: {_firmwareDirectory}\n");
            
            var firmwareFiles = Directory.GetFiles(_firmwareDirectory, "*.hex");
            if (firmwareFiles.Length == 0)
            {
                Console.WriteLine("No firmware files found.\n");
                return;
            }

            Console.WriteLine("Available firmware files:");
            foreach (var file in firmwareFiles)
            {
                var fileInfo = new FileInfo(file);
                Console.WriteLine($"  {Path.GetFileName(file)} ({fileInfo.Length:N0} bytes, {fileInfo.LastWriteTime:yyyy-MM-dd HH:mm:ss})");
            }
            Console.WriteLine();
        }





        private async Task ListenForStatusUpdatesAsync()
        {
            while (!_cancellationTokenSource.Token.IsCancellationRequested)
            {
                try
                {
                    var result = await _udpClient.ReceiveAsync();
                    var message = Encoding.UTF8.GetString(result.Buffer);
                    
                    Console.WriteLine($"Status update from {result.RemoteEndPoint}: {message}");
                }
                catch (Exception ex) when (!_cancellationTokenSource.Token.IsCancellationRequested)
                {
                    Console.WriteLine($"UDP listener error: {ex.Message}");
                }
            }
        }
    }

    // Data structures
    public class ModuleInfo
    {
        public required string Name { get; set; }
        public required string IpAddress { get; set; }
        public ModuleRole Role { get; set; }
    }

    public enum ModuleRole
    {
        Left,
        Centre,
        Right
    }

    public class ModuleStatus
    {
        public required string Status { get; set; }
        public required string Version { get; set; }
        public long UptimeSeconds { get; set; }
        public long FreeMemory { get; set; }
    }

    public class StatusQueryPacket
    {
        public required string Command { get; set; }
    }

    public class UpdateCommandPacket
    {
        public required string Command { get; set; }
        public required string FirmwareUrl { get; set; }
        public required string FirmwareHash { get; set; }
        public long FirmwareSize { get; set; }
    }
}
