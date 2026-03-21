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
    [Description("Execute a console command in Skyrim (e.g., 'tgm', 'player.additem 0000000f 1000'). " +
        "CAUTION: Commands referencing invalid FormIDs will crash the game. Always verify FormIDs before use. " +
        "Avoid running commands during loading screens, combat, or scripted events. " +
        "Prefer dedicated tools (AddItem, Teleport, etc.) over raw console commands when available.")]
    public async Task<object> ExecuteConsoleCommand(string command)
    {
        var data = await _pipe.SendRequestAsync("execute_command", new JsonObject
        {
            ["command"] = command
        });
        return new { success = true, message = $"Executed command: {command}" };
    }

    [McpServerTool]
    [Description("Add an item to player inventory by name or FormID. " +
        "CAUTION: Invalid FormIDs that don't exist in the current load order will crash the game. " +
        "Use ListKnownItems for safe FormIDs, or verify the FormID exists before adding.")]
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
    [Description("Remove an item from player inventory by name or FormID. " +
        "CAUTION: Removing quest items can break quest progression and may be irreversible. " +
        "Confirm with the user before removing items you're not sure about.")]
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
    [Description("Teleport player to a location (e.g., 'whiterun', 'riverwood', 'solitude'). " +
        "CAUTION: Teleporting during combat, scripted scenes, or dialogue can crash the game. " +
        "Ask the user to confirm they are in a safe state before teleporting. " +
        "Invalid cell IDs will crash the game — use known locations from ListKnownItems or verify the cell ID first.")]
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
    [Description("Toggle collision to walk through walls. " +
        "CAUTION: Disabling collision can cause the player to fall through the world. " +
        "Remind the user to save before using and to re-enable collision when done.")]
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
    [Description("Get information about active quests including objectives and completion status. " +
        "Note: In heavily modded games this may return a large number of quests. " +
        "For a specific quest, use ExecuteConsoleCommand with 'getqueststage <questID>' instead.")]
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

    [McpServerTool]
    [Description("Save the game. Automatically generates a descriptive save name from player name, level, and location. " +
        "Creates both .ess and .skse co-save files. You can provide a custom save name or leave it blank for auto-naming. " +
        "CAUTION: Always save before performing risky operations like quest stage manipulation or teleporting.")]
    public async Task<object> SaveGame(string? saveName = null)
    {
        // If no name provided, build one from player info
        if (string.IsNullOrWhiteSpace(saveName))
        {
            try
            {
                var playerData = await _pipe.SendRequestAsync("get_player_info");
                var name = playerData?["name"]?.GetValue<string>() ?? "Player";
                var level = playerData?["level"]?.GetValue<int>() ?? 0;
                var cell = playerData?["cellName"]?.GetValue<string>() ?? "Unknown";
                var timestamp = DateTime.Now.ToString("yyyyMMdd-HHmmss");

                // Clean invalid filename characters
                name = string.Join("_", name.Split(Path.GetInvalidFileNameChars()));
                cell = string.Join("_", cell.Split(Path.GetInvalidFileNameChars()));

                saveName = $"MCP_{name}_Lv{level}_{cell}_{timestamp}";
            }
            catch
            {
                saveName = $"MCP_Save_{DateTime.Now:yyyyMMdd-HHmmss}";
            }
        }

        await _pipe.SendRequestAsync("save_game", new JsonObject
        {
            ["saveName"] = saveName
        });

        return new { success = true, message = $"Game saved as: {saveName}" };
    }

    [McpServerTool]
    [Description("Get the full load order of all active plugins (ESP, ESM, ESL) with their FormID prefixes. " +
        "Use this to determine the correct FormID prefix for items from a specific mod.")]
    public async Task<object> GetLoadOrder()
    {
        var data = await _pipe.SendRequestAsync("get_load_order");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Look up the FormID prefix for a specific mod/plugin by filename (e.g., 'HLIORemi.esp'). " +
        "Returns the hex prefix needed to construct valid FormIDs for that mod's records. " +
        "For regular plugins, the prefix is 2 hex digits (e.g., '3A'). " +
        "For light plugins (ESL/ESPFE), the prefix is 'FE' followed by 3 hex digits (e.g., 'FE01A').")]
    public async Task<object> GetModFormIdPrefix(string modName)
    {
        var data = await _pipe.SendRequestAsync("get_mod_formid_prefix", new JsonObject
        {
            ["modName"] = modName
        });
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }
}
