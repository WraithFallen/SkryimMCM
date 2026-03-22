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
    /// </summary>
    protected static object DeserializeResponse(JsonNode? data)
    {
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    /// <summary>
    /// Apply paging to a response that contains an array field.
    /// Returns a paged envelope with items, page, pageSize, totalItems, totalPages, hasMore.
    /// If page=0 or pageSize=0, returns summary only (no items).
    /// </summary>
    protected static object PageResponse(JsonNode? data, string arrayField, int page = 1, int pageSize = 50)
    {
        if (data == null) return new { error = "No data returned" };

        var array = data[arrayField]?.AsArray();
        if (array == null) return DeserializeResponse(data);

        int totalItems = array.Count;
        int totalPages = pageSize > 0 ? (int)Math.Ceiling((double)totalItems / pageSize) : 1;

        // Summary only mode
        if (page == 0 || pageSize == 0)
        {
            return new
            {
                totalItems,
                totalPages = pageSize > 0 ? totalPages : 0,
                summaryOnly = true
            };
        }

        // Slice the array
        var items = array
            .Skip((page - 1) * pageSize)
            .Take(pageSize)
            .Select(i => JsonSerializer.Deserialize<JsonElement>(i?.ToJsonString() ?? "{}"))
            .ToArray();

        return new
        {
            items,
            page,
            pageSize,
            totalItems,
            totalPages,
            hasMore = page < totalPages
        };
    }
}
