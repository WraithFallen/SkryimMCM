using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for NPC operations — nearby NPCs, detailed info, inventory, followers, detection, crosshair, kill
/// </summary>
[McpServerToolType]
public class NPCTools : ToolBase
{
    public NPCTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool]
    [Description("Get nearby NPCs within a given radius. Returns refId (use for dot-notation console commands like 'refId.kill') " +
        "and baseId (use for setessential). Also includes name, race, level, distance, hostility, and dead status.")]
    public async Task<object> GetNearbyNPCs(float radius = 4096)
    {
        var data = await _pipe.SendRequestAsync("get_nearby_npcs", new JsonObject
        {
            ["radius"] = radius
        });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get detailed info about an NPC by reference ID — class, combat style, outfit, factions, stats, " +
        "state flags (essential, protected, ghost, child, follower, commanded), position, and hostility. " +
        "Much more detailed than GetActorInfo.")]
    public async Task<object> GetNPCDetailedInfo(string refId)
    {
        var data = await _pipe.SendRequestAsync("get_npc_detailed_info", new JsonObject { ["refId"] = refId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Read an NPC's inventory by reference ID. Shows what items they're carrying — " +
        "weapons, armor, potions, misc items with FormIDs, counts, and values. " +
        "Alias for GetInventory with a refId — kept for convenience.")]
    public async Task<object> GetNPCInventory(string refId)
    {
        var data = await _pipe.SendRequestAsync("get_inventory", new JsonObject { ["refId"] = refId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("List all current followers/teammates — NPCs flagged as player teammates. " +
        "Shows name, race, level, class, health, distance, combat state, and essential status.")]
    public async Task<object> GetFollowers()
    {
        var data = await _pipe.SendRequestAsync("get_followers");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Check if an NPC has detected the player. Returns detection level (0-100) and state " +
        "(undetected, lost, suspicious, detected). Use with GetNearbyNPCs to check stealth status.")]
    public async Task<object> GetDetectionLevel(string refId)
    {
        var data = await _pipe.SendRequestAsync("get_detection_level", new JsonObject { ["refId"] = refId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get the reference ID of whatever the player's crosshair is pointing at. " +
        "Returns refId and baseId. Use refId with dot-notation for console commands (e.g., 'refId.kill'). " +
        "Use baseId for commands like setessential. " +
        "Use this when the player says 'that NPC' or 'this object' — it tells you exactly what they're looking at.")]
    public async Task<object> GetCrosshairRef()
    {
        var data = await _pipe.SendRequestAsync("get_crosshair_ref");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Kill an actor by reference ID. Removes essential/protected flags first. " +
        "Use GetCrosshairRef or GetNearbyNPCs to get the refId. " +
        "CAUTION: Killing essential NPCs can break quests. Confirm with user first.")]
    public async Task<object> KillActor(string refId)
    {
        var data = await _pipe.SendRequestAsync("kill_actor", new JsonObject
        {
            ["refId"] = refId
        });
        var name = data?["name"]?.GetValue<string>() ?? refId;
        await NotifyInGame($"{name} killed");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Play an idle animation on an NPC or the player. Pass the actor's refId and the idle animation's FormID. " +
        "Use SearchForms with type='idle' to find animation FormIDs. " +
        "Example: make an NPC dance, sit, cheer, or perform any registered idle animation.")]
    public async Task<object> PlayIdle(string refId, string idleFormId)
    {
        var data = await _pipe.SendRequestAsync("play_idle", new JsonObject
        {
            ["refId"] = refId,
            ["idleFormId"] = idleFormId
        });
        return DeserializeResponse(data);
    }
}
