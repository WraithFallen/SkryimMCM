using System.Text.Json;
using System.Text.Json.Nodes;

namespace SkyrimMCP.Tools;

/// <summary>
/// Base class for all MCP tool classes. Provides shared pipe access and helper methods.
/// </summary>
public abstract class ToolBase
{
    protected readonly IPipeClient _pipe;

    protected ToolBase(IPipeClient pipe)
    {
        _pipe = pipe;
    }

    /// <summary>
    /// Send an in-game notification to the player's HUD.
    /// Fire-and-forget — errors are silently ignored.
    /// </summary>
    protected async Task NotifyInGame(string message)
    {
        try
        {
            await _pipe.SendRequestAsync("show_notification", new JsonObject
            {
                ["message"] = message
            });
        }
        catch { /* notification failure shouldn't break the tool */ }
    }

    /// <summary>
    /// Deserialize a JsonNode response to an object suitable for MCP tool return.
    /// Replaces the repeated pattern: (object?)JsonSerializer.Deserialize&lt;JsonElement&gt;(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" }
    /// </summary>
    protected static object DeserializeResponse(JsonNode? data)
    {
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }
}
