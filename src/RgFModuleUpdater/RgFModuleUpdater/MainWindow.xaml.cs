/*
 * MainWindow.xaml.cs
 * 
 * WPF UI code-behind for RgF Module Updater
 * Provides user-friendly interface for ABLS firmware updates
 * 
 * Author: RgF Engineering
 * Version: 1.0.0
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;

namespace RgFModuleUpdater
{
    public partial class MainWindow : Window
    {
        private ModuleUpdateOrchestrator _orchestrator;
        private readonly DispatcherTimer _statusTimer;
        private readonly List<ModuleInfo> _modules;
        private bool _isUpdating = false;

        public MainWindow()
        {
            InitializeComponent();
            
            _modules = new List<ModuleInfo>
            {
                new ModuleInfo { Name = "Left Wing", IpAddress = "192.168.1.101", Role = ModuleRole.Left },
                new ModuleInfo { Name = "Centre", IpAddress = "192.168.1.102", Role = ModuleRole.Centre },
                new ModuleInfo { Name = "Right Wing", IpAddress = "192.168.1.103", Role = ModuleRole.Right }
            };

            _statusTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(5)
            };
            _statusTimer.Tick += StatusTimer_Tick;

            Loaded += MainWindow_Loaded;
            Closing += MainWindow_Closing;
        }

        private async void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            LogMessage("=== RgF Module Updater v1.0.0 ===");
            LogMessage("ABLS Firmware Update System");
            LogMessage("Author: RgF Engineering\n");

            try
            {
                _orchestrator = new ModuleUpdateOrchestrator();
                await _orchestrator.InitializeAsync();
                
                UpdateServerStatus(true);
                LogMessage("✓ HTTP firmware server started successfully");
                LogMessage("✓ UDP communication initialized");
                
                RefreshFirmwareList();
                await RefreshModuleStatus();
                
                _statusTimer.Start();
                LogMessage("✓ System ready for firmware updates\n");
            }
            catch (Exception ex)
            {
                LogMessage($"✗ Initialization failed: {ex.Message}");
                UpdateServerStatus(false);
                MessageBox.Show($"Failed to initialize system: {ex.Message}", "Initialization Error", 
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            _statusTimer?.Stop();
            _orchestrator?.Dispose();
        }

        private async void StatusTimer_Tick(object sender, EventArgs e)
        {
            if (!_isUpdating)
            {
                await RefreshModuleStatus();
            }
        }

        private async void RefreshStatusButton_Click(object sender, RoutedEventArgs e)
        {
            await RefreshModuleStatus();
        }

        private async Task RefreshModuleStatus()
        {
            try
            {
                LogMessage("Checking module status...");
                
                var leftStatus = await QueryModuleStatusSafe(_modules[0]);
                var centreStatus = await QueryModuleStatusSafe(_modules[1]);
                var rightStatus = await QueryModuleStatusSafe(_modules[2]);

                UpdateModuleStatusUI("Left", leftStatus, LeftModuleStatus, LeftModuleVersion);
                UpdateModuleStatusUI("Centre", centreStatus, CentreModuleStatus, CentreModuleVersion);
                UpdateModuleStatusUI("Right", rightStatus, RightModuleStatus, RightModuleVersion);
                
                LogMessage("Module status check completed\n");
            }
            catch (Exception ex)
            {
                LogMessage($"Error checking module status: {ex.Message}");
            }
        }

        private async Task<ModuleStatus?> QueryModuleStatusSafe(ModuleInfo module)
        {
            try
            {
                return await _orchestrator.QueryModuleStatusAsync(module);
            }
            catch
            {
                return null;
            }
        }

        private void UpdateModuleStatusUI(string moduleName, ModuleStatus? status, TextBlock statusBlock, TextBlock versionBlock)
        {
            if (status != null)
            {
                statusBlock.Text = $"Status: {status.Status}";
                statusBlock.Foreground = status.Status == "OPERATIONAL" ? Brushes.Green : Brushes.Orange;
                versionBlock.Text = $"Version: {status.Version}";
            }
            else
            {
                statusBlock.Text = "Status: OFFLINE";
                statusBlock.Foreground = Brushes.Red;
                versionBlock.Text = "Version: Unknown";
            }
        }

        private void RefreshFirmwareButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshFirmwareList();
        }

        private void RefreshFirmwareList()
        {
            try
            {
                var firmwareDirectory = Path.Combine(Environment.CurrentDirectory, "firmware");
                if (!Directory.Exists(firmwareDirectory))
                {
                    Directory.CreateDirectory(firmwareDirectory);
                }

                var firmwareFiles = Directory.GetFiles(firmwareDirectory, "*.hex")
                    .Select(Path.GetFileName)
                    .ToList();

                FirmwareComboBox.ItemsSource = firmwareFiles;
                
                if (firmwareFiles.Any())
                {
                    FirmwareComboBox.SelectedIndex = 0;
                    LogMessage($"Found {firmwareFiles.Count} firmware file(s)");
                }
                else
                {
                    LogMessage("No firmware files found. Please place .hex files in the firmware directory.");
                }
            }
            catch (Exception ex)
            {
                LogMessage($"Error refreshing firmware list: {ex.Message}");
            }
        }

        private void FirmwareComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (FirmwareComboBox.SelectedItem != null)
            {
                var selectedFile = FirmwareComboBox.SelectedItem.ToString();
                var filePath = Path.Combine(Environment.CurrentDirectory, "firmware", selectedFile);
                
                if (File.Exists(filePath))
                {
                    var fileInfo = new FileInfo(filePath);
                    LogMessage($"Selected firmware: {selectedFile} ({fileInfo.Length:N0} bytes, {fileInfo.LastWriteTime:yyyy-MM-dd HH:mm:ss})");
                }
            }
        }

        private async void UpdateAllButton_Click(object sender, RoutedEventArgs e)
        {
            if (_isUpdating)
            {
                MessageBox.Show("Update already in progress!", "Update In Progress", 
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (FirmwareComboBox.SelectedItem == null)
            {
                MessageBox.Show("Please select a firmware file first.", "No Firmware Selected", 
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var result = MessageBox.Show(
                $"This will update all three modules with firmware: {FirmwareComboBox.SelectedItem}\n\n" +
                "The update process will:\n" +
                "• Update modules sequentially (Left → Centre → Right)\n" +
                "• Verify firmware integrity using SHA256\n" +
                "• Require each module to reboot successfully\n\n" +
                "Proceed with the update?",
                "Confirm Update All Modules",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question);

            if (result == MessageBoxResult.Yes)
            {
                await PerformUpdateAll();
            }
        }

        private async Task PerformUpdateAll()
        {
            _isUpdating = true;
            UpdateProgressBar.Value = 0;
            ProgressLabel.Text = "Preparing update...";
            
            try
            {
                var selectedFirmware = FirmwareComboBox.SelectedItem.ToString();
                var firmwarePath = Path.Combine(Environment.CurrentDirectory, "firmware", selectedFirmware);
                
                LogMessage($"=== Starting Sequential Module Update ===");
                LogMessage($"Firmware: {selectedFirmware}");
                
                // Calculate SHA256 hash
                UpdateProgressBar.Value = 10;
                ProgressLabel.Text = "Calculating firmware hash...";
                var firmwareHash = await _orchestrator.CalculateSHA256Async(firmwarePath);
                LogMessage($"SHA256: {firmwareHash}");
                
                // Update each module sequentially
                for (int i = 0; i < _modules.Count; i++)
                {
                    var module = _modules[i];
                    var progressBase = 20 + (i * 25);
                    
                    UpdateProgressBar.Value = progressBase;
                    ProgressLabel.Text = $"Updating {module.Name}...";
                    LogMessage($"\n--- Updating {module.Name} ---");
                    
                    try
                    {
                        await _orchestrator.UpdateModuleAsync(module, firmwarePath, firmwareHash);
                        UpdateProgressBar.Value = progressBase + 20;
                        LogMessage($"✓ {module.Name} updated successfully");
                    }
                    catch (Exception ex)
                    {
                        LogMessage($"✗ {module.Name} update failed: {ex.Message}");
                        MessageBox.Show($"Update failed on {module.Name}: {ex.Message}\n\nAborting remaining updates.", 
                            "Update Failed", MessageBoxButton.OK, MessageBoxImage.Error);
                        return;
                    }
                }
                
                UpdateProgressBar.Value = 95;
                ProgressLabel.Text = "Verifying all modules...";
                await Task.Delay(2000); // Give modules time to reboot
                
                await RefreshModuleStatus();
                
                UpdateProgressBar.Value = 100;
                ProgressLabel.Text = "Update completed successfully!";
                LogMessage("\n=== All modules updated successfully! ===");
                
                MessageBox.Show("All modules have been updated successfully!", "Update Complete", 
                    MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                LogMessage($"Update process failed: {ex.Message}");
                MessageBox.Show($"Update process failed: {ex.Message}", "Update Error", 
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                _isUpdating = false;
                if (UpdateProgressBar.Value != 100)
                {
                    UpdateProgressBar.Value = 0;
                    ProgressLabel.Text = "Ready";
                }
            }
        }

        private void UpdateSelectedButton_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Individual module update feature coming soon!", "Feature Not Available", 
                MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void OpenFirmwareFolderButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var firmwareDirectory = Path.Combine(Environment.CurrentDirectory, "firmware");
                if (!Directory.Exists(firmwareDirectory))
                {
                    Directory.CreateDirectory(firmwareDirectory);
                }
                
                Process.Start("explorer.exe", firmwareDirectory);
                LogMessage($"Opened firmware directory: {firmwareDirectory}");
            }
            catch (Exception ex)
            {
                LogMessage($"Error opening firmware directory: {ex.Message}");
                MessageBox.Show($"Could not open firmware directory: {ex.Message}", "Error", 
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ClearLogButton_Click(object sender, RoutedEventArgs e)
        {
            LogTextBox.Clear();
        }

        private void LogMessage(string message)
        {
            Dispatcher.Invoke(() =>
            {
                var timestamp = DateTime.Now.ToString("HH:mm:ss");
                LogTextBox.AppendText($"[{timestamp}] {message}\n");
                LogTextBox.ScrollToEnd();
            });
        }

        private void UpdateServerStatus(bool isOnline)
        {
            Dispatcher.Invoke(() =>
            {
                if (isOnline)
                {
                    ServerStatusIndicator.Fill = Brushes.Green;
                    ServerStatusText.Text = "Server Online";
                    StatusBarText.Text = $"Ready - HTTP Server: Running on port {_orchestrator?.GetHttpPort() ?? 8080}";
                }
                else
                {
                    ServerStatusIndicator.Fill = Brushes.Red;
                    ServerStatusText.Text = "Server Offline";
                    StatusBarText.Text = "Error - HTTP Server: Stopped";
                }
            });
        }
    }
}
