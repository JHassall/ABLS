using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;

namespace ABLSIMUCalibrator
{
    /// <summary>
    /// Professional IMU Calibration Application for ABLS Agricultural Boom Control
    /// Provides user-driven calibration workflow with boom in known positions
    /// </summary>
    public partial class MainWindow : Window
    {
        #region Private Fields
        
        private UdpClient? _udpClient;
        private IPEndPoint? _targetEndPoint;
        private bool _isConnected = false;
        private CancellationTokenSource? _cancellationTokenSource;
        private DispatcherTimer _statusTimer;
        private DispatcherTimer _dataUpdateTimer;
        
        // Calibration state
        private CalibrationState _calibrationState = CalibrationState.NotStarted;
        private Dictionary<string, MultiSensorCalibrationData> _calibrationData = new();
        private Dictionary<string, ModuleStatus> _moduleStatuses = new();
        
        // Real-time multi-sensor data
        private Dictionary<string, MultiSensorRealtimeData> _realtimeData = new();
        
        // Calibration progress tracking
        private int _calibrationStepsCompleted = 0;
        
        #endregion

        #region Constructor and Initialization
        
        public MainWindow()
        {
            InitializeComponent();
            InitializeApplication();
        }
        
        private void InitializeApplication()
        {
            // Initialize module statuses
            _moduleStatuses["Left"] = new ModuleStatus { Name = "Left Wing Module", IsOnline = false };
            _moduleStatuses["Centre"] = new ModuleStatus { Name = "Centre Module", IsOnline = false };
            _moduleStatuses["Right"] = new ModuleStatus { Name = "Right Wing Module", IsOnline = false };
            
            // Initialize calibration data storage
            _calibrationData["Left"] = new MultiSensorCalibrationData();
            _calibrationData["Centre"] = new MultiSensorCalibrationData();
            _calibrationData["Right"] = new MultiSensorCalibrationData();
            
            // Initialize real-time data storage
            _realtimeData["Left"] = new MultiSensorRealtimeData();
            _realtimeData["Centre"] = new MultiSensorRealtimeData();
            _realtimeData["Right"] = new MultiSensorRealtimeData();
            
            // Setup timers
            _statusTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(1)
            };
            _statusTimer.Tick += StatusTimer_Tick;
            _statusTimer.Start();
            
            _dataUpdateTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(100) // 10Hz update rate
            };
            _dataUpdateTimer.Tick += DataUpdateTimer_Tick;
            
            UpdateUI();
        }
        
        #endregion

        #region Event Handlers
        
        private async void BtnConnect_Click(object sender, RoutedEventArgs e)
        {
            if (!_isConnected)
            {
                await ConnectToModules();
            }
            else
            {
                await DisconnectFromModules();
            }
        }
        
        private async void BtnSetLevelReference_Click(object sender, RoutedEventArgs e)
        {
            await SetLevelReference();
        }
        
        private async void BtnCaptureLevelPosition_Click(object sender, RoutedEventArgs e)
        {
            await CaptureCalibrationPosition(CalibrationPosition.Level);
        }
        
        private async void BtnCaptureMaxUpPosition_Click(object sender, RoutedEventArgs e)
        {
            await CaptureCalibrationPosition(CalibrationPosition.MaxUp);
        }
        
        private async void BtnCaptureMaxDownPosition_Click(object sender, RoutedEventArgs e)
        {
            await CaptureCalibrationPosition(CalibrationPosition.MaxDown);
        }
        
        private async void BtnCompleteCalibration_Click(object sender, RoutedEventArgs e)
        {
            await CompleteCalibration();
        }
        
        private async void BtnAbortCalibration_Click(object sender, RoutedEventArgs e)
        {
            await AbortCalibration();
        }
        
        private void StatusTimer_Tick(object? sender, EventArgs e)
        {
            txtTimestamp.Text = DateTime.Now.ToString("HH:mm:ss");
        }
        
        private void DataUpdateTimer_Tick(object? sender, EventArgs e)
        {
            UpdateRealtimeDataDisplay();
        }
        
        #endregion

        #region Network Communication
        
        private async Task ConnectToModules()
        {
            try
            {
                txtStatusBar.Text = "Connecting to ABLS modules...";
                
                // Parse target IP
                if (!IPAddress.TryParse(txtTargetIP.Text, out IPAddress? targetIP))
                {
                    MessageBox.Show("Invalid IP address format.", "Connection Error", 
                                  MessageBoxButton.OK, MessageBoxImage.Error);
                    return;
                }
                
                // Initialize UDP client
                _targetEndPoint = new IPEndPoint(targetIP, 8888); // ABLS command port
                _udpClient = new UdpClient();
                _cancellationTokenSource = new CancellationTokenSource();
                
                // Start listening for responses
                _ = Task.Run(() => ListenForResponses(_cancellationTokenSource.Token));
                
                // Send initial connection request
                await SendCommand("CONNECT_IMU_CALIBRATOR");
                
                // Start data update timer
                _dataUpdateTimer.Start();
                
                _isConnected = true;
                btnConnect.Content = "Disconnect";
                txtStatusBar.Text = "Connected - Checking module status...";
                
                // Request initial status from all modules
                await RequestModuleStatus();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Connection failed: {ex.Message}", "Connection Error", 
                              MessageBoxButton.OK, MessageBoxImage.Error);
                txtStatusBar.Text = "Connection failed";
            }
        }
        
        private async Task DisconnectFromModules()
        {
            try
            {
                _isConnected = false;
                _dataUpdateTimer.Stop();
                
                if (_cancellationTokenSource != null)
                {
                    _cancellationTokenSource.Cancel();
                    _cancellationTokenSource.Dispose();
                    _cancellationTokenSource = null;
                }
                
                if (_udpClient != null)
                {
                    await SendCommand("DISCONNECT_IMU_CALIBRATOR");
                    _udpClient.Close();
                    _udpClient.Dispose();
                    _udpClient = null;
                }
                
                // Reset module statuses
                foreach (var status in _moduleStatuses.Values)
                {
                    status.IsOnline = false;
                }
                
                btnConnect.Content = "Connect";
                txtStatusBar.Text = "Disconnected";
                UpdateUI();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Disconnection error: {ex.Message}", "Error", 
                              MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }
        
        private async Task SendCommand(string command, string? moduleId = null)
        {
            if (_udpClient == null || _targetEndPoint == null) return;
            
            try
            {
                string message = moduleId != null ? $"{command}:{moduleId}" : command;
                byte[] data = Encoding.UTF8.GetBytes(message);
                await _udpClient.SendAsync(data, data.Length, _targetEndPoint);
            }
            catch (Exception ex)
            {
                txtStatusBar.Text = $"Send error: {ex.Message}";
            }
        }
        
        private async Task ListenForResponses(CancellationToken cancellationToken)
        {
            if (_udpClient == null) return;
            
            try
            {
                while (!cancellationToken.IsCancellationRequested)
                {
                    var result = await _udpClient.ReceiveAsync();
                    string response = Encoding.UTF8.GetString(result.Buffer);
                    
                    // Process response on UI thread
                    Dispatcher.Invoke(() => ProcessResponse(response));
                }
            }
            catch (ObjectDisposedException)
            {
                // Expected when disconnecting
            }
            catch (Exception ex)
            {
                Dispatcher.Invoke(() => txtStatusBar.Text = $"Listen error: {ex.Message}");
            }
        }
        
        private void ProcessResponse(string response)
        {
            try
            {
                // Parse response format: "COMMAND:MODULE_ID:DATA"
                string[] parts = response.Split(':', 3);
                if (parts.Length < 2) return;
                
                string command = parts[0];
                string moduleId = parts[1];
                string data = parts.Length > 2 ? parts[2] : "";
                
                switch (command)
                {
                    case "MODULE_STATUS":
                        ProcessModuleStatus(moduleId, data);
                        break;
                    case "IMU_DATA":
                        ProcessIMUData(moduleId, data);
                        break;
                    case "CALIBRATION_CAPTURED":
                        ProcessCalibrationCaptured(moduleId, data);
                        break;
                    case "CALIBRATION_COMPLETE":
                        ProcessCalibrationComplete(moduleId, data);
                        break;
                    default:
                        txtStatusBar.Text = $"Unknown response: {command}";
                        break;
                }
            }
            catch (Exception ex)
            {
                txtStatusBar.Text = $"Response processing error: {ex.Message}";
            }
        }
        
        #endregion

        #region Calibration Workflow
        
        private async Task SetLevelReference()
        {
            if (!_isConnected)
            {
                MessageBox.Show("Please connect to modules first.", "Not Connected", 
                              MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            
            // Confirm boom is manually leveled
            var result = MessageBox.Show(
                "MANUAL LEVEL SETTING PROCEDURE:\n\n" +
                "1. Physically measure the distance from boom center and each boom tip to level ground\n" +
                "2. Manually adjust hydraulic rams until all measurements are EQUAL\n" +
                "3. Verify boom is completely horizontal across its full width\n\n" +
                "Has the boom been manually leveled using physical measurements?",
                "Confirm Manual Level Setting",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question);
            
            if (result != MessageBoxResult.Yes) return;
            
            // Capture reference data from all sensors
            await SendCommand("SET_LEVEL_REFERENCE");
            txtStatusBar.Text = "Capturing level reference from all sensors...";
            
            // Update progress
            _calibrationStepsCompleted = 1;
            UpdateCalibrationProgress();
        }
        
        private async Task CaptureCalibrationPosition(CalibrationPosition position)
        {
            if (!_isConnected)
            {
                MessageBox.Show("Please connect to modules first.", "Not Connected", 
                              MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            
            // Confirm boom is in correct position
            string positionName = position switch
            {
                CalibrationPosition.Level => "LEVEL (horizontal)",
                CalibrationPosition.MaxUp => "MAXIMUM UP angle",
                CalibrationPosition.MaxDown => "MAXIMUM DOWN angle",
                _ => "unknown"
            };
            
            var result = MessageBox.Show(
                $"Ensure the boom is positioned at {positionName} before capturing.\n\n" +
                "Is the boom in the correct position?",
                "Confirm Boom Position",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question);
            
            if (result != MessageBoxResult.Yes) return;
            
            // Send capture command to all modules
            string command = position switch
            {
                CalibrationPosition.Level => "CAPTURE_LEVEL_POSITION",
                CalibrationPosition.MaxUp => "CAPTURE_MAX_UP_POSITION",
                CalibrationPosition.MaxDown => "CAPTURE_MAX_DOWN_POSITION",
                _ => ""
            };
            
            if (!string.IsNullOrEmpty(command))
            {
                await SendCommand(command);
                txtStatusBar.Text = $"Capturing {positionName} position...";
            }
        }
        
        private async Task CompleteCalibration()
        {
            if (!AllPositionsCaptured())
            {
                MessageBox.Show("Please capture all calibration positions first.", "Incomplete Calibration", 
                              MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            
            var result = MessageBox.Show(
                "This will finalize the calibration and save it to the modules.\n\n" +
                "Are you sure you want to complete the calibration?",
                "Complete Calibration",
                MessageBoxButton.YesNo,
                MessageBoxImage.Question);
            
            if (result == MessageBoxResult.Yes)
            {
                await SendCommand("COMPLETE_IMU_CALIBRATION");
                txtStatusBar.Text = "Completing calibration...";
            }
        }
        
        private async Task AbortCalibration()
        {
            var result = MessageBox.Show(
                "This will abort the current calibration process.\n\n" +
                "Are you sure you want to abort?",
                "Abort Calibration",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);
            
            if (result == MessageBoxResult.Yes)
            {
                await SendCommand("ABORT_IMU_CALIBRATION");
                ResetCalibrationState();
                txtStatusBar.Text = "Calibration aborted";
            }
        }
        
        #endregion

        #region Data Processing
        
        private void ProcessModuleStatus(string moduleId, string data)
        {
            if (_moduleStatuses.ContainsKey(moduleId))
            {
                _moduleStatuses[moduleId].IsOnline = data == "ONLINE";
                UpdateUI();
            }
        }
        
        private void ProcessIMUData(string moduleId, string data)
        {
            if (!_realtimeData.ContainsKey(moduleId)) return;
            
            try
            {
                // Parse multi-sensor data format: "qI,qJ,qK,qReal,aX,aY,aZ,gX,gY,gZ,lX,lY,lZ,qAcc,aAcc,gAcc,ramC,ramL,ramR,radarDist,radarValid"
                string[] values = data.Split(',');
                if (values.Length >= 21)
                {
                    var sensorData = _realtimeData[moduleId];
                    
                    // IMU Quaternion
                    sensorData.QuaternionI = float.Parse(values[0]);
                    sensorData.QuaternionJ = float.Parse(values[1]);
                    sensorData.QuaternionK = float.Parse(values[2]);
                    sensorData.QuaternionReal = float.Parse(values[3]);
                    
                    // Raw acceleration
                    sensorData.AccelX = float.Parse(values[4]);
                    sensorData.AccelY = float.Parse(values[5]);
                    sensorData.AccelZ = float.Parse(values[6]);
                    
                    // Gyroscope
                    sensorData.GyroX = float.Parse(values[7]);
                    sensorData.GyroY = float.Parse(values[8]);
                    sensorData.GyroZ = float.Parse(values[9]);
                    
                    // Linear acceleration
                    sensorData.LinearAccelX = float.Parse(values[10]);
                    sensorData.LinearAccelY = float.Parse(values[11]);
                    sensorData.LinearAccelZ = float.Parse(values[12]);
                    
                    // IMU Accuracy values
                    sensorData.QuaternionAccuracy = byte.Parse(values[13]);
                    sensorData.AccelAccuracy = byte.Parse(values[14]);
                    sensorData.GyroAccuracy = byte.Parse(values[15]);
                    
                    // Ram Potentiometer Data (Centre module only)
                    if (moduleId == "Centre")
                    {
                        sensorData.RamPosCenterPercent = float.Parse(values[16]);
                        sensorData.RamPosLeftPercent = float.Parse(values[17]);
                        sensorData.RamPosRightPercent = float.Parse(values[18]);
                    }
                    
                    // Radar Data
                    sensorData.RadarDistance = float.Parse(values[19]);
                    sensorData.RadarValid = values[20] == "1";
                    
                    // Calculate boom angles from different sensors
                    sensorData.IMUBoomAngle = CalculatePitchFromQuaternion(
                        sensorData.QuaternionI, sensorData.QuaternionJ, 
                        sensorData.QuaternionK, sensorData.QuaternionReal);
                    
                    if (moduleId == "Centre")
                    {
                        sensorData.RamBoomAngle = CalculateBoomAngleFromRamPosition(sensorData.RamPosCenterPercent);
                    }
                    
                    sensorData.RadarBoomHeight = sensorData.RadarDistance;
                    
                    sensorData.LastUpdate = DateTime.Now;
                }
            }
            catch (Exception ex)
            {
                txtStatusBar.Text = $"Multi-sensor data parsing error: {ex.Message}";
            }
        }
        
        private void UpdateCalibrationProgress()
        {
            progressCalibration.Value = _calibrationStepsCompleted;
            txtCalibrationProgress.Text = $"{_calibrationStepsCompleted}/4 steps completed";
            
            // Update button states based on progress
            UpdateCalibrationButtons();
        }
        
        private void ProcessCalibrationCaptured(string moduleId, string data)
        {
            // Update calibration status based on captured position
            UpdateCalibrationStatus();
            txtStatusBar.Text = $"Position captured for {moduleId} module";
        }
        
        private void ProcessCalibrationComplete(string moduleId, string data)
        {
            txtStatusBar.Text = $"Calibration completed for {moduleId} module";
            
            // Check if all modules completed calibration
            // Implementation would track completion status per module
        }
        
        #endregion

        #region UI Updates
        
        private void UpdateUI()
        {
            // Update module status indicators
            UpdateModuleStatusIndicator("Left", ellLeftStatus, txtLeftStatus);
            UpdateModuleStatusIndicator("Centre", ellCentreStatus, txtCentreStatus);
            UpdateModuleStatusIndicator("Right", ellRightStatus, txtRightStatus);
            
            // Update calibration button states
            UpdateCalibrationButtons();
            
            // Update IMU accuracy display
            UpdateIMUAccuracyDisplay();
        }
        
        private void UpdateModuleStatusIndicator(string moduleId, System.Windows.Shapes.Ellipse indicator, System.Windows.Controls.TextBlock textBlock)
        {
            if (_moduleStatuses.ContainsKey(moduleId))
            {
                var status = _moduleStatuses[moduleId];
                indicator.Fill = status.IsOnline ? Brushes.Green : Brushes.Red;
                textBlock.Text = $"{status.Name}: {(status.IsOnline ? "ONLINE" : "OFFLINE")}";
            }
        }
        
        private void UpdateCalibrationButtons()
        {
            bool anyModuleOnline = false;
            foreach (var status in _moduleStatuses.Values)
            {
                if (status.IsOnline)
                {
                    anyModuleOnline = true;
                    break;
                }
            }
            
            btnCaptureLevelPosition.IsEnabled = anyModuleOnline;
            btnCaptureMaxUpPosition.IsEnabled = anyModuleOnline;
            btnCaptureMaxDownPosition.IsEnabled = anyModuleOnline;
            btnCompleteCalibration.IsEnabled = AllPositionsCaptured();
        }
        
        private void UpdateIMUAccuracyDisplay()
        {
            var sb = new StringBuilder();
            
            foreach (var kvp in _realtimeData)
            {
                var data = kvp.Value;
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 5)
                {
                    sb.AppendLine($"{kvp.Key} Module:");
                    sb.AppendLine($"  Quaternion Accuracy: {data.QuaternionAccuracy} (0=Unreliable, 3=High)");
                    sb.AppendLine($"  Accelerometer Accuracy: {data.AccelAccuracy}");
                    sb.AppendLine($"  Gyroscope Accuracy: {data.GyroAccuracy}");
                    sb.AppendLine();
                }
            }
            
            if (sb.Length == 0)
            {
                sb.AppendLine("Connect to modules to view IMU accuracy status...");
            }
            
            txtIMUAccuracy.Text = sb.ToString();
        }
        
        private void UpdateRealtimeDataDisplay()
        {
            UpdateQuaternionDisplay();
            UpdateLinearAccelDisplay();
            UpdateRamPositionDisplay();
            UpdateRadarDataDisplay();
            UpdateCalibratedBoomAnglesDisplay();
            UpdateSensorAgreementDisplay();
            UpdateDataQualityIndicators();
        }
        
        private void UpdateQuaternionDisplay()
        {
            var sb = new StringBuilder();
            
            foreach (var kvp in _realtimeData)
            {
                var data = kvp.Value;
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 2)
                {
                    sb.AppendLine($"{kvp.Key}: I={data.QuaternionI:F3}, J={data.QuaternionJ:F3}, K={data.QuaternionK:F3}, Real={data.QuaternionReal:F3}");
                }
            }
            
            if (sb.Length == 0)
            {
                sb.AppendLine("Connect to modules to view quaternion data...");
            }
            
            txtQuaternionData.Text = sb.ToString();
        }
        
        private void UpdateLinearAccelDisplay()
        {
            var sb = new StringBuilder();
            
            foreach (var kvp in _realtimeData)
            {
                var data = kvp.Value;
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 2)
                {
                    sb.AppendLine($"{kvp.Key}: X={data.LinearAccelX:F2}, Y={data.LinearAccelY:F2}, Z={data.LinearAccelZ:F2}");
                }
            }
            
            if (sb.Length == 0)
            {
                sb.AppendLine("Connect to modules to view linear acceleration...");
            }
            
            txtLinearAccelData.Text = sb.ToString();
        }
        
        private void UpdateRamPositionDisplay()
        {
            var sb = new StringBuilder();
            
            // Only Centre module has ram position data
            if (_realtimeData.ContainsKey("Centre"))
            {
                var data = _realtimeData["Centre"];
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 2)
                {
                    sb.AppendLine($"Centre Ram: {data.RamPosCenterPercent:F1}% (Angle: {data.RamBoomAngle:F1}°)");
                    sb.AppendLine($"Left Ram: {data.RamPosLeftPercent:F1}%");
                    sb.AppendLine($"Right Ram: {data.RamPosRightPercent:F1}%");
                }
            }
            
            if (sb.Length == 0)
            {
                sb.AppendLine("Connect to Centre module to view ram positions...");
            }
            
            txtRamPositionData.Text = sb.ToString();
        }
        
        private void UpdateRadarDataDisplay()
        {
            var sb = new StringBuilder();
            
            foreach (var kvp in _realtimeData)
            {
                var data = kvp.Value;
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 2)
                {
                    string validText = data.RadarValid ? "VALID" : "INVALID";
                    sb.AppendLine($"{kvp.Key}: {data.RadarDistance:F2}m ({validText})");
                }
            }
            
            if (sb.Length == 0)
            {
                sb.AppendLine("Connect to modules to view radar distance...");
            }
            
            txtRadarData.Text = sb.ToString();
        }
        
        private void UpdateCalibratedBoomAnglesDisplay()
        {
            var sb = new StringBuilder();
            
            foreach (var kvp in _realtimeData)
            {
                var data = kvp.Value;
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 2)
                {
                    sb.AppendLine($"{kvp.Key} Module:");
                    sb.AppendLine($"  IMU Angle: {data.IMUBoomAngle:F1}°");
                    
                    if (kvp.Key == "Centre")
                    {
                        sb.AppendLine($"  Ram Angle: {data.RamBoomAngle:F1}°");
                    }
                    
                    sb.AppendLine($"  Radar Height: {data.RadarBoomHeight:F2}m");
                    sb.AppendLine();
                }
            }
            
            if (sb.Length == 0)
            {
                sb.AppendLine("Complete calibration to view correlated angles...");
            }
            
            txtCalibratedBoomAngles.Text = sb.ToString();
        }
        
        private void UpdateSensorAgreementDisplay()
        {
            var sb = new StringBuilder();
            
            // Analyze sensor agreement for Centre module (has all sensor types)
            if (_realtimeData.ContainsKey("Centre"))
            {
                var data = _realtimeData["Centre"];
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 2)
                {
                    double imuRamDiff = Math.Abs(data.IMUBoomAngle - data.RamBoomAngle);
                    
                    sb.AppendLine($"IMU vs Ram Difference: {imuRamDiff:F1}°");
                    
                    if (imuRamDiff < 2.0)
                        sb.AppendLine("✅ Excellent agreement (<2°)");
                    else if (imuRamDiff < 5.0)
                        sb.AppendLine("⚠️ Good agreement (<5°)");
                    else
                        sb.AppendLine("❌ Poor agreement (>5°) - Check calibration");
                    
                    sb.AppendLine();
                    sb.AppendLine($"Radar Valid: {(data.RadarValid ? "✅ Yes" : "❌ No")}");
                }
            }
            
            if (sb.Length == 0)
            {
                sb.AppendLine("Calibration required for sensor agreement analysis...");
            }
            
            txtSensorAgreement.Text = sb.ToString();
        }
        
        private void UpdateDataQualityIndicators()
        {
            // Update quality indicators based on data freshness and accuracy
            bool hasRecentIMUData = false;
            bool hasRecentRAMData = false;
            bool hasRecentRadarData = false;
            
            foreach (var kvp in _realtimeData)
            {
                var data = kvp.Value;
                if (data.LastUpdate.HasValue && (DateTime.Now - data.LastUpdate.Value).TotalSeconds < 3)
                {
                    // IMU quality based on accuracy
                    if (data.QuaternionAccuracy >= 2) hasRecentIMUData = true;
                    
                    // RAM quality (Centre module only)
                    if (kvp.Key == "Centre" && data.RamPosCenterPercent > 0) hasRecentRAMData = true;
                    
                    // Radar quality based on validity
                    if (data.RadarValid) hasRecentRadarData = true;
                }
            }
            
            // Update quality indicators
            ellIMUQuality.Fill = hasRecentIMUData ? Brushes.Green : Brushes.Red;
            ellRAMQuality.Fill = hasRecentRAMData ? Brushes.Green : Brushes.Red;
            ellRadarQuality.Fill = hasRecentRadarData ? Brushes.Green : Brushes.Red;
        }
        
        private void UpdateCalibrationStatus()
        {
            // Update status text for each calibration position
            // This would be implemented based on actual calibration data received
            
            txtLevelStatus.Text = "Captured";
            txtLevelStatus.Foreground = Brushes.Green;
            
            // Similar updates for other positions...
        }
        
        #endregion

        #region Helper Methods
        
        private async Task RequestModuleStatus()
        {
            await SendCommand("REQUEST_MODULE_STATUS");
        }
        
        private bool AllPositionsCaptured()
        {
            // Check if all required calibration positions have been captured
            // This would be implemented based on actual calibration state tracking
            return false; // Placeholder
        }
        
        private void ResetCalibrationState()
        {
            _calibrationState = CalibrationState.NotStarted;
            
            // Reset UI status indicators
            txtLevelStatus.Text = "Not captured";
            txtLevelStatus.Foreground = Brushes.Red;
            txtMaxUpStatus.Text = "Not captured";
            txtMaxUpStatus.Foreground = Brushes.Red;
            txtMaxDownStatus.Text = "Not captured";
            txtMaxDownStatus.Foreground = Brushes.Red;
            
            UpdateCalibrationButtons();
        }
        
        private double CalculatePitchFromQuaternion(float qI, float qJ, float qK, float qReal)
        {
            // Convert quaternion to pitch angle (simplified calculation)
            // This would be enhanced with proper calibration offset compensation
            double sinp = 2 * (qReal * qJ - qK * qI);
            if (Math.Abs(sinp) >= 1)
                return Math.CopySign(Math.PI / 2, sinp) * 180.0 / Math.PI; // Use 90 degrees if out of range
            else
                return Math.Asin(sinp) * 180.0 / Math.PI;
        }
        
        private double CalculateBoomAngleFromRamPosition(float ramPositionPercent)
        {
            // Convert ram position percentage to boom angle
            // This is a simplified linear calculation - would be enhanced with actual boom geometry
            // Assuming 50% = level (0°), 0% = max down (-15°), 100% = max up (+15°)
            double angle = (ramPositionPercent - 50.0) * 0.3; // 0.3 degrees per percent
            return angle;
        }
        
        #endregion

        #region Window Events
        
        protected override async void OnClosed(EventArgs e)
        {
            if (_isConnected)
            {
                await DisconnectFromModules();
            }
            
            _statusTimer?.Stop();
            _dataUpdateTimer?.Stop();
            
            base.OnClosed(e);
        }
        
        #endregion
    }

    #region Data Classes
    
    public enum CalibrationState
    {
        NotStarted,
        InProgress,
        Completed,
        Aborted
    }
    
    public enum CalibrationPosition
    {
        Level,
        MaxUp,
        MaxDown
    }
    
    public class ModuleStatus
    {
        public string Name { get; set; } = "";
        public bool IsOnline { get; set; } = false;
    }
    
    public class MultiSensorRealtimeData
    {
        // IMU Data
        public float QuaternionI { get; set; }
        public float QuaternionJ { get; set; }
        public float QuaternionK { get; set; }
        public float QuaternionReal { get; set; }
        
        public float AccelX { get; set; }
        public float AccelY { get; set; }
        public float AccelZ { get; set; }
        
        public float GyroX { get; set; }
        public float GyroY { get; set; }
        public float GyroZ { get; set; }
        
        public float LinearAccelX { get; set; }
        public float LinearAccelY { get; set; }
        public float LinearAccelZ { get; set; }
        
        public byte QuaternionAccuracy { get; set; }
        public byte AccelAccuracy { get; set; }
        public byte GyroAccuracy { get; set; }
        
        // Ram Potentiometer Data
        public float RamPosCenterPercent { get; set; }
        public float RamPosLeftPercent { get; set; }
        public float RamPosRightPercent { get; set; }
        
        // Radar Data
        public float RadarDistance { get; set; }
        public bool RadarValid { get; set; }
        
        public DateTime? LastUpdate { get; set; }
        
        // Calculated boom angles from different sensors
        public double IMUBoomAngle { get; set; }
        public double RamBoomAngle { get; set; }
        public double RadarBoomHeight { get; set; }
    }
    
    public class MultiSensorCalibrationData
    {
        public MultiSensorRealtimeData? LevelReferenceData { get; set; }
        public MultiSensorRealtimeData? LevelPosition { get; set; }
        public MultiSensorRealtimeData? MaxUpPosition { get; set; }
        public MultiSensorRealtimeData? MaxDownPosition { get; set; }
        
        public bool IsLevelReferenceSet => LevelReferenceData != null;
        public bool IsLevelCaptured => LevelPosition != null;
        public bool IsMaxUpCaptured => MaxUpPosition != null;
        public bool IsMaxDownCaptured => MaxDownPosition != null;
        public bool IsComplete => IsLevelReferenceSet && IsLevelCaptured && IsMaxUpCaptured && IsMaxDownCaptured;
        
        // Calibration correlation tables
        public Dictionary<float, float> RamToIMUCorrelation { get; set; } = new();
        public Dictionary<float, float> RadarToIMUCorrelation { get; set; } = new();
        public Dictionary<float, float> RamToRadarCorrelation { get; set; } = new();
    }
    
    #endregion
}
