using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for combat analysis — combat state, damage stats, threat assessment
/// </summary>
[McpServerToolType]
public class CombatTools : ToolBase
{
    public CombatTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool]
    [Description("Get detailed combat state — whether in combat, current targets (with health/level), " +
        "and combat allies. Use during combat to understand the tactical situation.")]
    public async Task<object> GetCombatState()
    {
        var data = await _pipe.SendRequestAsync("get_combat_state");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get damage and defense stats — armor rating, all resistances, and equipped weapon damage " +
        "for both hands. Use for build analysis or combat preparation.")]
    public async Task<object> GetDamageStats()
    {
        var data = await _pipe.SendRequestAsync("get_damage_stats");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Scan for hostile actors within radius — shows their name, race, level, health, " +
        "equipped weapon with damage, distance, and essential status. Use for threat assessment before or during combat.")]
    public async Task<object> GetThreats(float radius = 4096)
    {
        var data = await _pipe.SendRequestAsync("get_threats", new JsonObject { ["radius"] = radius });
        return DeserializeResponse(data);
    }
}
