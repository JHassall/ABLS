using System.Windows;

namespace ABLSIMUCalibrator
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// Professional IMU Calibration Application for ABLS Agricultural Boom Control
    /// </summary>
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            
            // Set application properties
            this.ShutdownMode = ShutdownMode.OnMainWindowClose;
            
            // Handle unhandled exceptions
            this.DispatcherUnhandledException += App_DispatcherUnhandledException;
        }
        
        private void App_DispatcherUnhandledException(object sender, System.Windows.Threading.DispatcherUnhandledExceptionEventArgs e)
        {
            MessageBox.Show($"An unexpected error occurred:\n\n{e.Exception.Message}", 
                          "ABLS IMU Calibrator Error", 
                          MessageBoxButton.OK, 
                          MessageBoxImage.Error);
            
            e.Handled = true;
        }
    }
}
