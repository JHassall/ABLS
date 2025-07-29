/*
 * SimpleHttpServer.cs
 * 
 * Simple HTTP server implementation using TcpListener
 * Avoids administrator privilege requirements of HttpListener
 * 
 * Author: RgF Engineering
 * Version: 1.0.0
 */

using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace RgFModuleUpdater
{
    public class SimpleHttpServer : IDisposable
    {
        private TcpListener _listener;
        private readonly int _port;
        private readonly string _firmwareDirectory;
        private CancellationTokenSource _cancellationTokenSource;
        private bool _isRunning;

        public SimpleHttpServer(int port, string firmwareDirectory)
        {
            _port = port;
            _firmwareDirectory = firmwareDirectory;
            _cancellationTokenSource = new CancellationTokenSource();
        }

        public async Task<bool> StartAsync()
        {
            try
            {
                _listener = new TcpListener(IPAddress.Loopback, _port);
                _listener.Start();
                _isRunning = true;

                Console.WriteLine($"✓ Simple HTTP server started on port {_port}");
                Console.WriteLine($"  Server URL: http://localhost:{_port}/");
                Console.WriteLine($"  Firmware directory: {_firmwareDirectory}");

                // Start accepting connections
                _ = Task.Run(AcceptConnectionsAsync);

                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Failed to start HTTP server on port {_port}: {ex.Message}");
                return false;
            }
        }

        private async Task AcceptConnectionsAsync()
        {
            while (_isRunning && !_cancellationTokenSource.Token.IsCancellationRequested)
            {
                try
                {
                    var tcpClient = await _listener.AcceptTcpClientAsync();
                    _ = Task.Run(() => HandleClientAsync(tcpClient));
                }
                catch (ObjectDisposedException)
                {
                    // Server is shutting down
                    break;
                }
                catch (Exception ex)
                {
                    if (_isRunning)
                    {
                        Console.WriteLine($"Error accepting connection: {ex.Message}");
                    }
                }
            }
        }

        private async Task HandleClientAsync(TcpClient client)
        {
            try
            {
                using (client)
                using (var stream = client.GetStream())
                using (var reader = new StreamReader(stream))
                using (var writer = new StreamWriter(stream))
                {
                    // Read HTTP request
                    var requestLine = await reader.ReadLineAsync();
                    if (string.IsNullOrEmpty(requestLine))
                        return;

                    var parts = requestLine.Split(' ');
                    if (parts.Length < 2)
                        return;

                    var method = parts[0];
                    var path = parts[1];

                    // Read headers (we don't need them for this simple server)
                    string line;
                    while (!string.IsNullOrEmpty(line = await reader.ReadLineAsync()))
                    {
                        // Skip headers
                    }

                    // Handle GET requests for firmware files
                    if (method == "GET" && path.StartsWith("/firmware/"))
                    {
                        await HandleFirmwareRequestAsync(writer, stream, path);
                    }
                    else
                    {
                        await SendNotFoundResponseAsync(writer);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error handling client: {ex.Message}");
            }
        }

        private async Task HandleFirmwareRequestAsync(StreamWriter writer, NetworkStream stream, string path)
        {
            try
            {
                var fileName = Path.GetFileName(path);
                var filePath = Path.Combine(_firmwareDirectory, fileName);

                if (File.Exists(filePath))
                {
                    var fileBytes = await File.ReadAllBytesAsync(filePath);
                    
                    // Send HTTP response headers
                    await writer.WriteLineAsync("HTTP/1.1 200 OK");
                    await writer.WriteLineAsync("Content-Type: application/octet-stream");
                    await writer.WriteLineAsync($"Content-Length: {fileBytes.Length}");
                    await writer.WriteLineAsync($"Content-Disposition: attachment; filename=\"{fileName}\"");
                    await writer.WriteLineAsync("Connection: close");
                    await writer.WriteLineAsync(); // Empty line to end headers
                    await writer.FlushAsync();

                    // Send file content
                    await stream.WriteAsync(fileBytes, 0, fileBytes.Length);
                    await stream.FlushAsync();

                    Console.WriteLine($"Served firmware file: {fileName} ({fileBytes.Length:N0} bytes)");
                }
                else
                {
                    await SendNotFoundResponseAsync(writer);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error serving firmware file: {ex.Message}");
                await SendErrorResponseAsync(writer, "Internal Server Error");
            }
        }

        private async Task SendNotFoundResponseAsync(StreamWriter writer)
        {
            var response = "HTTP/1.1 404 Not Found\r\n" +
                          "Content-Type: text/plain\r\n" +
                          "Content-Length: 14\r\n" +
                          "Connection: close\r\n\r\n" +
                          "File not found";

            await writer.WriteAsync(response);
            await writer.FlushAsync();
        }

        private async Task SendErrorResponseAsync(StreamWriter writer, string error)
        {
            var response = $"HTTP/1.1 500 Internal Server Error\r\n" +
                          "Content-Type: text/plain\r\n" +
                          $"Content-Length: {error.Length}\r\n" +
                          "Connection: close\r\n\r\n" +
                          error;

            await writer.WriteAsync(response);
            await writer.FlushAsync();
        }

        public void Stop()
        {
            _isRunning = false;
            _cancellationTokenSource?.Cancel();
            _listener?.Stop();
        }

        public void Dispose()
        {
            Stop();
            _cancellationTokenSource?.Dispose();
        }
    }
}
