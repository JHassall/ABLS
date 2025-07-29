/*
 * RgFModuleUpdater - Standalone Windows WPF Application
 * 
 * Toughbook-side firmware update orchestrator for ABLS Teensy modules.
 * Handles sequential firmware updates with safety checks, progress monitoring,
 * and version synchronization across all three modules.
 * 
 * Author: RgF Engineering
 * Version: 1.0.0
 */

using System;
using System.Windows;

namespace RgFModuleUpdater
{
    public partial class App : Application
    {
        [STAThread]
        static void Main(string[] args)
        {
            var app = new App();
            app.InitializeComponent();
            app.Run();
        }

        private void InitializeComponent()
        {
            this.StartupUri = new Uri("MainWindow.xaml", UriKind.Relative);
        }
    }
}
