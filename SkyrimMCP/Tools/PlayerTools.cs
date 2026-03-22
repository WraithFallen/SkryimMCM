using System.ComponentModel;
using System.Text.Json;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for player state queries — info, skills, perks, spells, shouts, appearance, factions
/// </summary>
[McpServerToolType]
public class PlayerTools : ToolBase
{
    public PlayerTools(IPipeClient pipe) : base(pipe) { }

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
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get the player's current gold count")]
    public async Task<object> GetGoldCount()
    {
        var data = await _pipe.SendRequestAsync("get_gold_count");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all 18 skill levels (current and base values) plus player level.")]
    public async Task<object> GetSkillLevels()
    {
        var data = await _pipe.SendRequestAsync("get_skill_levels");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all perks the player has acquired, with rank information.")]
    public async Task<object> GetPerks()
    {
        var data = await _pipe.SendRequestAsync("get_perks");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all spells the player has learned, organized by school (Alteration, Conjuration, etc.) and type (spell, power, ability).")]
    public async Task<object> GetKnownSpells()
    {
        var data = await _pipe.SendRequestAsync("get_known_spells");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all shouts the player knows, including per-word unlock status and recovery times.")]
    public async Task<object> GetKnownShouts()
    {
        var data = await _pipe.SendRequestAsync("get_known_shouts");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get the player's appearance data: race, sex, weight, height, all 19 face morph values, " +
        "face presets, and head parts (hair, eyes, etc.). Use this for character recreation or face morphing.")]
    public async Task<object> GetAppearance()
    {
        var data = await _pipe.SendRequestAsync("get_appearance");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get the player's favorited spells and items (the ones assigned to the favorites menu/hotkeys).")]
    public async Task<object> GetFavorites()
    {
        var data = await _pipe.SendRequestAsync("get_favorites");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Export a complete character blueprint as JSON. Includes player info, all 18 skills, perks, " +
        "known spells, known shouts, equipped items, full inventory, gold, and active effects. " +
        "Use this to save a character template that can be re-applied to a new character. " +
        "IMPORTANT: Pass an outputPath to write the blueprint directly to a file on disk — the data is too large " +
        "for the MCP response. Example outputPath: 'C:\\Users\\cory\\Downloads\\blueprint.json'")]
    public async Task<object> GetCharacterBlueprint(string? outputPath = null)
    {
        var data = await _pipe.SendRequestAsync("get_character_blueprint");

        if (!string.IsNullOrEmpty(outputPath) && data != null)
        {
            try
            {
                var jsonString = data.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
                await File.WriteAllTextAsync(outputPath, jsonString);
                await NotifyInGame("Blueprint exported");

                // Return summary instead of full data
                var spellCount = data["spells"]?["count"]?.GetValue<int>() ?? 0;
                var perkCount = data["perks"]?["count"]?.GetValue<int>() ?? 0;
                var shoutCount = data["shouts"]?["count"]?.GetValue<int>() ?? 0;
                var itemCount = data["inventory"]?["items"]?.AsArray()?.Count ?? 0;
                var effectCount = data["activeEffects"]?["effects"]?.AsArray()?.Count ?? 0;

                return new
                {
                    success = true,
                    outputPath,
                    message = $"Blueprint saved to {outputPath}",
                    summary = new
                    {
                        spells = spellCount,
                        perks = perkCount,
                        shouts = shoutCount,
                        inventoryItems = itemCount,
                        activeEffects = effectCount
                    }
                };
            }
            catch (Exception ex)
            {
                return new { error = $"Failed to write blueprint: {ex.Message}" };
            }
        }

        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get all factions the player belongs to, with rank for each.")]
    public async Task<object> GetPlayerFactions()
    {
        var data = await _pipe.SendRequestAsync("get_player_factions");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Check for diseases, vampirism, and lycanthropy. Returns vampire/werewolf status, " +
        "active diseases with names and effects, and the player's current race.")]
    public async Task<object> GetDiseaseStatus()
    {
        var data = await _pipe.SendRequestAsync("get_disease_status");
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get detailed info about a specific spell by FormID — school, casting type (fire-and-forget, concentration, constant), " +
        "delivery (self, aimed, target), magicka cost, casting perk, and full effect list with magnitudes/durations. " +
        "Use SearchForms to find spell FormIDs first.")]
    public async Task<object> GetSpellDetails(string formId)
    {
        var data = await _pipe.SendRequestAsync("get_spell_details", new JsonObject { ["formId"] = formId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get enchantment details for an item in the player's inventory by FormID. " +
        "Returns enchantment name, charge level, and full effect list with magnitudes. " +
        "Also returns the item's display name if it was custom-renamed.")]
    public async Task<object> GetEnchantmentInfo(string formId)
    {
        var data = await _pipe.SendRequestAsync("get_enchantment_info", new JsonObject { ["formId"] = formId });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get the player's magic resistance values — magic, fire, frost, shock, poison, and disease resistance percentages.")]
    public async Task<object> GetMagicResistances()
    {
        var data = await _pipe.SendRequestAsync("get_magic_resistances");
        return DeserializeResponse(data);
    }
}
