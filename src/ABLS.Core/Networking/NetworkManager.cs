using ABLS.Core.Models;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace ABLS.Core.Networking
{
    /// <summary>
    /// Manages all network communication for the ABLS application.
    /// </summary>
    public class NetworkManager
        public event Action<SensorDataPacket> OnSensorDataReceived;

        private readonly UdpListener<SensorDataPacket> _sensorListener;
        private readonly UdpSender _commandSender;

        // Configuration for the control Teensy
        private readonly string _controlTeensyIp;
        private readonly int _controlTeensyPort;

        public NetworkManager(int listenPort, string controlTeensyIp, int controlTeensyPort)
        {
            _sensorListener = new UdpListener<SensorDataPacket>(listenPort);
            _sensorListener.OnPacketReceived += (packet) => OnSensorDataReceived?.Invoke(packet);

            _commandSender = new UdpSender();
            _controlTeensyIp = controlTeensyIp;
            _controlTeensyPort = controlTeensyPort;
        }

        /// <summary>
        /// Starts listening for incoming sensor data.
        /// </summary>
        public void Start()
        {
            Console.WriteLine("Starting Network Manager...");
            _sensorListener.StartListening();
            Console.WriteLine($"Listening for sensor data on port {_sensorListener._udpClient.Client.LocalEndPoint}...");
        }

        /// <summary>
        /// Stops the network manager and closes connections.
        /// </summary>
        public void Stop()
        {
            Console.WriteLine("Stopping Network Manager...");
            _sensorListener.StopListening();
            _commandSender.Close();
        }

        /// <summary>
        /// Sends a control command to the control Teensy.
        /// </summary>
        public async Task SendCommandAsync(ControlCommandPacket command)
        {
            await _commandSender.SendPacketAsync(command, _controlTeensyIp, _controlTeensyPort);
        }
    }
}
