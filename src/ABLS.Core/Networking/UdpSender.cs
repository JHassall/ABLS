using Newtonsoft.Json;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace ABLS.Core.Networking
{
    /// <summary>
    /// Sends UDP packets containing serialized data to a specified endpoint.
    /// </summary>
    public class UdpSender
    {
        private readonly UdpClient _udpClient;

        public UdpSender()
        {
            _udpClient = new UdpClient();
        }

        /// <summary>
        /// Asynchronously sends a data packet to the specified IP address and port.
        /// </summary>
        /// <typeparam name="T">The type of the data packet.</typeparam>
        /// <param name="packet">The data packet to send.</param>
        /// <param name="hostname">The destination IP address or hostname.</param>
        /// <param name="port">The destination port.</param>
        public async Task SendPacketAsync<T>(T packet, string hostname, int port)
        {
            string json = JsonConvert.SerializeObject(packet);
            byte[] data = Encoding.UTF8.GetBytes(json);
            await _udpClient.SendAsync(data, data.Length, hostname, port);
        }

        public void Close()
        {
            _udpClient.Close();
            _udpClient.Dispose();
        }
    }
}
