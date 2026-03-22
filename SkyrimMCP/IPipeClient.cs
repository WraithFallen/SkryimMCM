using System.Text.Json.Nodes;

namespace SkyrimMCP;

/// <summary>
/// Interface for named pipe communication with the SKSE plugin
/// </summary>
public interface IPipeClient : IDisposable
{
    bool IsConnected { get; }
    Task<bool> TryConnectAsync();
    Task<JsonNode?> SendRequestAsync(string action, JsonObject? parameters = null);
    Task<bool> PingAsync();
}
