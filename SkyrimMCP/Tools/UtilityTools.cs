using System.ComponentModel;
using System.Text.Json.Nodes;
using ModelContextProtocol.Server;

namespace SkyrimMCP.Tools;

/// <summary>
/// MCP Tools for utility operations — console commands, notifications, search, load order, save, events, shouts
/// </summary>
[McpServerToolType]
public class UtilityTools : ToolBase
{
    public UtilityTools(IPipeClient pipe) : base(pipe) { }

    [McpServerTool]
    [Description("FALLBACK ONLY — Execute a raw Skyrim console command when NO dedicated tool exists. " +
        "ALWAYS check dedicated tools first: " +
        "Player: GetPlayerInfo, GetSkillLevels, GetPerks, GetKnownSpells, GetKnownShouts, GetAppearance, GetFavorites, GetPlayerFactions. " +
        "Items: GetInventory, GetEquippedItems, AddItem, RemoveItem, BulkAddItems. " +
        "Quests: GetQuestInfo, GetQuestStages, GetQuestAliases, GetQuestItems, SetQuestStage. " +
        "NPCs: GetNearbyNPCs, GetNPCDetailedInfo, GetNPCInventory, GetFollowers, GetDetectionLevel, GetCrosshairRef, KillActor. " +
        "World: Teleport, GetWeather, SetWeather, GetCellInfo, GetNearbyObjects, UnlockDoor, LockDoor, GetContainerInventory. " +
        "Actions: ToggleGodMode, ToggleCollision, SetHealth, SetMagicka, SetStamina, SaveGame, SearchForms, ShowNotification. " +
        "Only use this tool if none of the above cover your need. " +
        "TARGETING: Use dot-notation with refId (e.g., 'FE124B0E.kill'), NOT prid — prid does not persist between calls. " +
        "Player commands: use 'player.' prefix (e.g., 'player.additem 0000000F 100'). " +
        "CAUTION: Invalid FormIDs crash the game. Avoid during loading screens or scripted events. " +
        "Console output may be captured if the command produces text output.")]
    public async Task<object> ExecuteConsoleCommand(string command)
    {
        var data = await _pipe.SendRequestAsync("execute_command", new JsonObject
        {
            ["command"] = command
        });
        return new { success = true, message = $"Executed command: {command}" };
    }

    [McpServerTool]
    [Description("Show a notification message on the player's HUD (the same style as quest updates and skill increases). " +
        "Use this to inform the player about actions you're taking or important information. " +
        "Keep messages short and natural — they appear briefly in the top-left corner of the screen.")]
    public async Task<object> ShowNotification(string message)
    {
        await _pipe.SendRequestAsync("show_notification", new JsonObject
        {
            ["message"] = message
        });
        return new { success = true, message };
    }

    [McpServerTool]
    [Description("Search for forms (items, NPCs, quests, spells, etc.) by name or editor ID. " +
        "This is the equivalent of the 'help' console command but safe and returns structured data. " +
        "Use this to find FormIDs for items, NPCs, quests, or any game record from any mod in the load order. " +
        "Optionally filter by type: weapon, armor, potion, book, misc, spell, perk, quest, npc, enchantment, shout, key, ammo, ingredient, soulgem, scroll, location. " +
        "Returns up to maxResults matches (default 25).")]
    public async Task<object> SearchForms(string query, string type = "all", int maxResults = 25)
    {
        var data = await _pipe.SendRequestAsync("search_forms", new JsonObject
        {
            ["query"] = query,
            ["type"] = type,
            ["maxResults"] = maxResults
        });
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Get the full load order of all active plugins (ESP, ESM, ESL) with their FormID prefixes. " +
        "Use this to determine the correct FormID prefix for items from a specific mod.")]
    public async Task<object> GetLoadOrder()
    {
        var data = await _pipe.SendRequestAsync("get_load_order");
        return DeserializeResponse(data);
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
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("List all installed SKSE plugins with integration potential. " +
        "Identifies plugins like ConsoleUtilSSE, HDT-SMP, PapyrusUtil, RaceMenu, TrueHUD, etc. " +
        "and notes which ones have APIs that could extend MCP capabilities.")]
    public async Task<object> GetLoadedSKSEPlugins()
    {
        var data = await _pipe.SendRequestAsync("get_loaded_skse_plugins");
        return DeserializeResponse(data);
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

        // Notification is handled by the plugin after save completes
        return new { success = true, message = $"Game saved as: {saveName}" };
    }

    [McpServerTool]
    [Description("Poll for recent game events. Returns all events since last poll. " +
        "Events: combat, death, quest_stage, quest_start_stop, location_change, cell_loaded, " +
        "inventory_change, equip, hit, spell_cast, activate, open_close, sleep_ended, wait_ended, " +
        "fast_travel, lock_changed, stat_change. " +
        "Framework quest noise is auto-filtered. For focused polling, pass eventTypes (array of types to include) " +
        "or excludeTypes (array to exclude) as parameters.")]
    public async Task<object> PollEvents(string? eventTypes = null, string? excludeTypes = null)
    {
        var parms = new JsonObject();
        if (!string.IsNullOrEmpty(eventTypes))
        {
            var types = new JsonArray();
            foreach (var t in eventTypes.Split(',', StringSplitOptions.TrimEntries))
                types.Add(t);
            parms["eventTypes"] = types;
        }
        if (!string.IsNullOrEmpty(excludeTypes))
        {
            var types = new JsonArray();
            foreach (var t in excludeTypes.Split(',', StringSplitOptions.TrimEntries))
                types.Add(t);
            parms["excludeTypes"] = types;
        }
        var data = await _pipe.SendRequestAsync("poll_events", parms);
        return DeserializeResponse(data);
    }

    [McpServerTool]
    [Description("Unlock a shout or word of power by FormID. If given a shout FormID, unlocks all three words. " +
        "If given an individual word FormID, unlocks just that word. Uses teachword + unlockword console commands.")]
    public async Task<object> UnlockShout(string formId)
    {
        var data = await _pipe.SendRequestAsync("unlock_shout", new JsonObject { ["formId"] = formId });
        await NotifyInGame("Shout unlocked");
        return DeserializeResponse(data);
    }
}
