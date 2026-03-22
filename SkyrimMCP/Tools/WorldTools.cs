using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for world interaction — weather, cell info, objects, locks, containers, map, teleport, god mode, stats
/// </summary>
[McpServerToolType]
public class WorldTools : ToolBase
{
    public WorldTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool]
    [Description("Get the current weather conditions.")]
    public async Task<object> GetWeather()
    {
        var data = await _pipe.SendRequestAsync("get_weather");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("List all available weather types with FormIDs, editor IDs, categories (clear, snow, rain, storm, fog, cloudy, ash), " +
        "and which mod they come from. Use the FormID or editor ID with SetWeather to change the weather.")]
    public async Task<object> ListWeathers()
    {
        var data = await _pipe.SendRequestAsync("list_weathers");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Change the weather. Pass a weather FormID or a search term like 'snow', 'rain', 'clear', 'storm', 'fog'. " +
        "The weather transitions naturally over a few seconds.")]
    public async Task<object> SetWeather(string weather)
    {
        var data = await _pipe.SendRequestAsync("set_weather", new JsonObject { ["weather"] = weather });
        var name = data?["weather"]?.GetValue<string>() ?? weather;
        await NotifyInGame($"Weather: {name}");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get detailed info about the player's current cell/location including interior/exterior status, " +
        "worldspace, exact coordinates, and facing angle.")]
    public async Task<object> GetCellInfo()
    {
        var data = await _pipe.SendRequestAsync("get_cell_info");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Find nearby objects like containers, doors, furniture, crafting stations, activators, and flora. " +
        "Filter by type: 'all', 'container', 'chest', 'door', 'furniture', 'crafting', 'activator', 'flora', 'light'. " +
        "Returns refId, name, type, distance, and locked status. Use this for 'what's nearby?' or 'find a forge'.")]
    public async Task<object> GetNearbyObjects(float radius = 2048, string type = "all")
    {
        var data = await _pipe.SendRequestAsync("get_nearby_objects", new JsonObject
        {
            ["radius"] = radius,
            ["type"] = type
        });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Unlock a locked door or container by reference ID. Use GetNearbyObjects or GetCrosshairRef to find the refId.")]
    public async Task<object> UnlockDoor(string refId)
    {
        var data = await _pipe.SendRequestAsync("unlock_door", new JsonObject { ["refId"] = refId });
        await NotifyInGame("Unlocked");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Lock a door or container by reference ID. lockLevel: 1=novice, 25=apprentice, 50=adept, 75=expert, 100=master, 255=requires key.")]
    public async Task<object> LockDoor(string refId, int lockLevel = 50)
    {
        var data = await _pipe.SendRequestAsync("lock_door", new JsonObject { ["refId"] = refId, ["lockLevel"] = lockLevel });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Read the inventory of a container by reference ID. Use GetNearbyObjects to find containers, then pass the refId. " +
        "Shows all items with name, FormID, count, weight, and value. Also shows locked status.")]
    public async Task<object> GetContainerInventory(string refId)
    {
        var data = await _pipe.SendRequestAsync("get_container_inventory", new JsonObject { ["refId"] = refId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Discover all map markers on the map. Uses tmm 1 console command. " +
        "CAUTION: This reveals every location on the map and cannot be undone without reloading a save.")]
    public async Task<object> DiscoverAllMapMarkers()
    {
        var data = await _pipe.SendRequestAsync("discover_all_map_markers");
        await NotifyInGame("All map markers discovered");
        return DeserializeResponse(data);
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
    [Description("Toggle god mode (invincibility) for the player. Uses direct engine API — no console command.")]
    public async Task<object> ToggleGodMode()
    {
        var data = await _pipe.SendRequestAsync("toggle_god_mode");
        var godMode = data?["godMode"]?.GetValue<bool>() ?? false;
        var msg = godMode ? "God Mode On" : "God Mode Off";
        await NotifyInGame(msg);
        return new { success = true, godMode, message = msg };
    }

    [McpServerTool]
    [Description("Toggle collision to walk through walls. Uses direct engine API — no console command. " +
        "CAUTION: Disabling collision can cause the player to fall through the world. " +
        "Remind the user to save before using and to re-enable collision when done.")]
    public async Task<object> ToggleCollision()
    {
        var data = await _pipe.SendRequestAsync("toggle_collision");
        var enabled = data?["collisionEnabled"]?.GetValue<bool>() ?? true;
        var msg = enabled ? "Collision On" : "No Clip";
        await NotifyInGame(msg);
        return new { success = true, collisionEnabled = enabled, message = msg };
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
        await NotifyInGame($"Health set to {value}");
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
        await NotifyInGame($"Magicka set to {value}");
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
        await NotifyInGame($"Stamina set to {value}");
        return new { success = true, message = $"Set stamina to {value}" };
    }
}
