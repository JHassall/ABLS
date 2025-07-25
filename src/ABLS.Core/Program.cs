using ABLS.Core.Models;
using ABLS.Core.Networking;
using System;
using System.Threading.Tasks;

namespace ABLS.Core
{
    class Program
    {
        static async Task Main(string[] args)
        {
            // --- Configuration ---
            int listenPort = 8888;
            string controlTeensyIp = "192.168.1.100"; // Example IP for the control Teensy
            int controlTeensyPort = 8889;

            Console.WriteLine("ABLS Core Application Starting...");

            var networkManager = new NetworkManager(listenPort, controlTeensyIp, controlTeensyPort);

            // Subscribe to the event to handle incoming sensor data
            networkManager.OnSensorDataReceived += (packet) =>
            {
                Console.WriteLine($"[RECV] Packet from {packet.SourceId}:");
                Console.WriteLine($"  GPS: Lat={packet.Gps.Latitude}, Lon={packet.Gps.Longitude}, Alt={packet.Gps.Altitude}");
                Console.WriteLine($"  IMU: Roll={packet.Imu.Roll}, Pitch={packet.Imu.Pitch}, Yaw={packet.Imu.Yaw}");
                Console.WriteLine($"  Radar: {packet.RadarDistanceM}m, Ram: {packet.RamPositionPercent}%");
            };

            networkManager.Start();

            // --- Main Application Loop (Simulation) ---
            // In the real app, this loop would contain the core control logic.
            // Here, we just send a test command every 5 seconds.
            while (true)
            {
                await Task.Delay(5000);

                var command = new ControlCommandPacket
                {
                    TargetId = "wing_right",
                    Command = "set_position",
                    Value = 55.5
                };

                Console.WriteLine($"[SEND] Command to {command.TargetId}: Set position to {command.Value}%");
                await networkManager.SendCommandAsync(command);
            }

            // The loop runs forever in this example. In a real app, you'd have a shutdown mechanism.
            // networkManager.Stop();
        }
    }
}
