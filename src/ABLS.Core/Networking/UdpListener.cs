using ABLS.Core.Models;
using Newtonsoft.Json;
using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace ABLS.Core.Networking
{
    /// <summary>
    /// Listens for UDP packets on a specified port and raises an event when a valid packet is received.
    /// </summary>
    /// <typeparam name="T">The type of the data packet to expect.</typeparam>
    public class UdpListener<T>
    {
        public event Action<T> OnPacketReceived;

        private readonly UdpClient _udpClient;
        private bool _isListening = false;

        public UdpListener(int port)
        {
            _udpClient = new UdpClient(port);
        }

        /// <summary>
        /// Starts the listener loop asynchronously.
        /// </summary>
        public void StartListening()
        {
            if (_isListening) return;
            _isListening = true;
            Task.Run(async () =>
            {
                while (_isListening)
                {
                    try
                    {
                        UdpReceiveResult result = await _udpClient.ReceiveAsync();
                        string json = Encoding.UTF8.GetString(result.Buffer);
                        T packet = JsonConvert.DeserializeObject<T>(json);
                        if (packet != null)
                        {
                            OnPacketReceived?.Invoke(packet);
                        }
                    }
                    catch (Exception ex)
                    {
                        // Log the exception (e.g., to a console or a log file)
                        Console.WriteLine($"Error receiving UDP packet: {ex.Message}");
                    }
                }
            });
        }

        /// <summary>
        /// Stops the listener.
        /// </summary>
        public void StopListening()
        {
            _isListening = false;
            _udpClient.Close();
            _udpClient.Dispose();
        }
    }
}
