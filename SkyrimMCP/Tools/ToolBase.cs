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
    /// Resolve a caller-supplied output path and confirm it stays inside the
    /// allowed output root — containment against an LLM writing to arbitrary
    /// locations (e.g. system dirs or the game install). The root defaults to the
    /// user profile (so Downloads/Documents/Desktop all work, matching the tool
    /// descriptions) and is overridable via the SKYLINK_OUTPUT_ROOT env var.
    /// Returns the normalized absolute path on success; sets <paramref name="error"/>
    /// (and returns null) if the path is empty or escapes the root. (Audit 19 MED #5.)
    ///
    /// LIMITATION: containment is lexical (Path.GetFullPath + prefix check). A
    /// directory junction/symlink the user has placed INSIDE the root that points
    /// outside it is not resolved and would not be caught. This is acceptable for
    /// the threat model (local single-user misuse containment, Claude-only client —
    /// not a hostile-network boundary); a self-created junction redirecting an own
    /// write is self-sabotage. Full reparse-point resolution is a queued LOW item.
    /// </summary>
    protected static string? ResolveContainedPath(string? requested, out string? error)
    {
        error = null;
        if (string.IsNullOrWhiteSpace(requested))
        {
            error = "Output path is required";
            return null;
        }

        var root = Environment.GetEnvironmentVariable("SKYLINK_OUTPUT_ROOT");
        if (string.IsNullOrWhiteSpace(root))
            root = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);

        string rootFull, reqFull;
        try
        {
            rootFull = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
            reqFull = Path.TrimEndingDirectorySeparator(Path.GetFullPath(requested));
        }
        catch (Exception ex)
        {
            error = $"Invalid output path: {ex.Message}";
            return null;
        }

        // Boundary-safe containment: equal to the root, or under it with a
        // separator boundary (so C:\Users\Bob does not match C:\Users\Bobby).
        var withSep = rootFull + Path.DirectorySeparatorChar;
        if (!reqFull.Equals(rootFull, StringComparison.OrdinalIgnoreCase) &&
            !reqFull.StartsWith(withSep, StringComparison.OrdinalIgnoreCase))
        {
            error = $"Output path must be within '{rootFull}' " +
                    "(set the SKYLINK_OUTPUT_ROOT env var to change the allowed root)";
            return null;
        }

        return reqFull;
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
