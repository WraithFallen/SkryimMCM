using System.ComponentModel;
using System.Text.Json;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP;

/// <summary>
/// MCP Tools for Skyrim interaction via SKSE plugin pipe
/// </summary>
[McpServerToolType]
public class SkyrimTools
{
    private readonly PipeClient _pipe;

    public SkyrimTools(PipeClient pipe)
    {
        _pipe = pipe;
    }

    [McpServerTool]
    [Description("Check if Skyrim is running and the SKSE MCP plugin is connected")]
    public async Task<object> CheckConnection()
    {
        bool connected = await _pipe.PingAsync();
        return connected
            ? new { connected = true, message = "Connected to Skyrim via SKSE plugin" }
            : new { connected = false, message = "Cannot connect to Skyrim. Make sure the game is running with SKSE and the SkyrimMCPPlugin is loaded." };
    }

    [McpServerTool]
    [Description("Get player information including name, race, level, health, magicka, stamina, and current location")]
    public async Task<object> GetPlayerInfo()
    {
        var data = await _pipe.SendRequestAsync("get_player_info");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get the player's full inventory with item names, counts, types, weights, and values")]
    public async Task<object> GetInventory()
    {
        var data = await _pipe.SendRequestAsync("get_inventory");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get the player's current gold count")]
    public async Task<object> GetGoldCount()
    {
        var data = await _pipe.SendRequestAsync("get_gold_count");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Execute a console command in Skyrim (e.g., 'tgm', 'player.additem 0000000f 1000')")]
    public async Task<object> ExecuteConsoleCommand(string command)
    {
        var data = await _pipe.SendRequestAsync("execute_command", new JsonObject
        {
            ["command"] = command
        });
        return new { success = true, message = $"Executed command: {command}" };
    }

    [McpServerTool]
    [Description("Add an item to player inventory by name or FormID")]
    public async Task<object> AddItem(string item, int count = 1)
    {
        // Resolve friendly name to FormID
        var formId = SkyrimOffsets.GetItemFormId(item) ?? item;

        var data = await _pipe.SendRequestAsync("add_item", new JsonObject
        {
            ["formId"] = formId,
            ["count"] = count
        });

        var name = data?["name"]?.GetValue<string>() ?? item;
        return new { success = true, message = $"Added {count}x {name} (FormID: {formId})" };
    }

    [McpServerTool]
    [Description("Remove an item from player inventory by name or FormID")]
    public async Task<object> RemoveItem(string item, int count = 1)
    {
        var formId = SkyrimOffsets.GetItemFormId(item) ?? item;

        var data = await _pipe.SendRequestAsync("remove_item", new JsonObject
        {
            ["formId"] = formId,
            ["count"] = count
        });

        var name = data?["name"]?.GetValue<string>() ?? item;
        return new { success = true, message = $"Removed {count}x {name} (FormID: {formId})" };
    }

    [McpServerTool]
    [Description("Teleport player to a location (e.g., 'whiterun', 'riverwood', 'solitude')")]
    public async Task<object> Teleport(string location)
    {
        var cellId = SkyrimOffsets.GetLocationCellId(location) ?? location.ToLower();

        await _pipe.SendRequestAsync("teleport", new JsonObject
        {
            ["cellId"] = cellId
        });

        return new { success = true, message = $"Teleporting to {location}" };
    }

    [McpServerTool]
    [Description("Toggle god mode (invincibility) for the player")]
    public async Task<object> ToggleGodMode()
    {
        await _pipe.SendRequestAsync("execute_command", new JsonObject
        {
            ["command"] = "tgm"
        });
        return new { success = true, message = "Toggled god mode" };
    }

    [McpServerTool]
    [Description("Set player's health to a specific value")]
    public async Task<object> SetHealth(float value)
    {
        await _pipe.SendRequestAsync("set_actor_value", new JsonObject
        {
            ["attribute"] = "health",
            ["value"] = value
        });
        return new { success = true, message = $"Set health to {value}" };
    }

    [McpServerTool]
    [Description("Set player's magicka to a specific value")]
    public async Task<object> SetMagicka(float value)
    {
        await _pipe.SendRequestAsync("set_actor_value", new JsonObject
        {
            ["attribute"] = "magicka",
            ["value"] = value
        });
        return new { success = true, message = $"Set magicka to {value}" };
    }

    [McpServerTool]
    [Description("Set player's stamina to a specific value")]
    public async Task<object> SetStamina(float value)
    {
        await _pipe.SendRequestAsync("set_actor_value", new JsonObject
        {
            ["attribute"] = "stamina",
            ["value"] = value
        });
        return new { success = true, message = $"Set stamina to {value}" };
    }

    [McpServerTool]
    [Description("Toggle collision to walk through walls")]
    public async Task<object> ToggleCollision()
    {
        await _pipe.SendRequestAsync("execute_command", new JsonObject
        {
            ["command"] = "tcl"
        });
        return new { success = true, message = "Toggled collision" };
    }

    [McpServerTool]
    [Description("List all known item FormIDs and location names that can be used with other commands")]
    public object ListKnownItems()
    {
        return new
        {
            items = SkyrimOffsets.Items,
            locations = SkyrimOffsets.Locations,
            message = "Use these names with add_item, remove_item, and teleport commands, or provide FormIDs directly"
        };
    }

    [McpServerTool]
    [Description("Get information about active quests including objectives and completion status")]
    public async Task<object> GetQuestInfo()
    {
        var data = await _pipe.SendRequestAsync("get_quest_info");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get nearby NPCs within a given radius, including their name, race, level, distance, and hostility")]
    public async Task<object> GetNearbyNPCs(float radius = 4096)
    {
        var data = await _pipe.SendRequestAsync("get_nearby_npcs", new JsonObject
        {
            ["radius"] = radius
        });
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }
}
