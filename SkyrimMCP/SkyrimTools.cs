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

    /// <summary>
    /// Send an in-game notification to the player's HUD.
    /// Fire-and-forget — errors are silently ignored.
    /// </summary>
    private async Task NotifyInGame(string message)
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
        "IMPORTANT — TARGETING NPCs/OBJECTS: Use dot-notation with the REFERENCE ID (refId), NOT prid. " +
        "Example: 'FE124B0E.kill' or 'FE124B0E.setav health 500'. The prid command does NOT persist between calls. " +
        "Get refIds from GetNearbyNPCs or GetCrosshairRef. " +
        "For player-targeted commands use 'player.' prefix: 'player.additem 0000000F 100'. " +
        "CAUTION: Commands referencing invalid FormIDs will crash the game. Always verify FormIDs before use. " +
        "Avoid running commands during loading screens, combat, or scripted events. " +
        "Prefer dedicated tools (AddItem, Teleport, KillActor, etc.) over raw console commands when available.")]
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
        await NotifyInGame($"Added {count}x {name}");
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
        await NotifyInGame($"Removed {count}x {name}");
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
    [Description("Get the current weather conditions.")]
    public async Task<object> GetWeather()
    {
        var data = await _pipe.SendRequestAsync("get_weather");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Change the weather. Pass a weather FormID or a search term like 'snow', 'rain', 'clear', 'storm', 'fog'. " +
        "The weather transitions naturally over a few seconds.")]
    public async Task<object> SetWeather(string weather)
    {
        var data = await _pipe.SendRequestAsync("set_weather", new JsonObject { ["weather"] = weather });
        var name = data?["weather"]?.GetValue<string>() ?? weather;
        await NotifyInGame($"Weather: {name}");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get detailed info about the player's current cell/location including interior/exterior status, " +
        "worldspace, exact coordinates, and facing angle.")]
    public async Task<object> GetCellInfo()
    {
        var data = await _pipe.SendRequestAsync("get_cell_info");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
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
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Unlock a locked door or container by reference ID. Use GetNearbyObjects or GetCrosshairRef to find the refId.")]
    public async Task<object> UnlockDoor(string refId)
    {
        var data = await _pipe.SendRequestAsync("unlock_door", new JsonObject { ["refId"] = refId });
        await NotifyInGame("Unlocked");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Lock a door or container by reference ID. lockLevel: 1=novice, 25=apprentice, 50=adept, 75=expert, 100=master, 255=requires key.")]
    public async Task<object> LockDoor(string refId, int lockLevel = 50)
    {
        var data = await _pipe.SendRequestAsync("lock_door", new JsonObject { ["refId"] = refId, ["lockLevel"] = lockLevel });
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Read the inventory of a container by reference ID. Use GetNearbyObjects to find containers, then pass the refId. " +
        "Shows all items with name, FormID, count, weight, and value. Also shows locked status.")]
    public async Task<object> GetContainerInventory(string refId)
    {
        var data = await _pipe.SendRequestAsync("get_container_inventory", new JsonObject { ["refId"] = refId });
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Discover all map markers on the map. Uses tmm 1 console command. " +
        "CAUTION: This reveals every location on the map and cannot be undone without reloading a save.")]
    public async Task<object> DiscoverAllMapMarkers()
    {
        var data = await _pipe.SendRequestAsync("discover_all_map_markers");
        await NotifyInGame("All map markers discovered");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
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
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get all currently equipped items including armor, weapons, jewelry, and shield. " +
        "Shows which slot each item occupies (head, body, hands, feet, leftHand, rightHand, etc.).")]
    public async Task<object> GetEquippedItems()
    {
        var data = await _pipe.SendRequestAsync("get_equipped_items");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get all spells the player has learned, organized by school (Alteration, Conjuration, etc.) and type (spell, power, ability).")]
    public async Task<object> GetKnownSpells()
    {
        var data = await _pipe.SendRequestAsync("get_known_spells");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get all shouts the player knows, including per-word unlock status and recovery times.")]
    public async Task<object> GetKnownShouts()
    {
        var data = await _pipe.SendRequestAsync("get_known_shouts");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get all 18 skill levels (current and base values) plus player level.")]
    public async Task<object> GetSkillLevels()
    {
        var data = await _pipe.SendRequestAsync("get_skill_levels");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get all perks the player has acquired, with rank information.")]
    public async Task<object> GetPerks()
    {
        var data = await _pipe.SendRequestAsync("get_perks");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Unlock a shout or word of power by FormID. If given a shout FormID, unlocks all three words. " +
        "If given an individual word FormID, unlocks just that word. Uses teachword + unlockword console commands.")]
    public async Task<object> UnlockShout(string formId)
    {
        var data = await _pipe.SendRequestAsync("unlock_shout", new JsonObject { ["formId"] = formId });
        await NotifyInGame("Shout unlocked");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Add multiple items to player inventory in a single call. " +
        "Pass a JSON array of items, each with 'formId' (or 'name') and optional 'count' (default 1). " +
        "Example: [{\"formId\":\"0000000F\",\"count\":1000},{\"name\":\"iron_sword\",\"count\":5}] " +
        "Items that fail are skipped and reported in the errors list. " +
        "Use this for bulk operations like applying a character blueprint's inventory.")]
    public async Task<object> BulkAddItems(string items)
    {
        List<object> results = new();
        List<object> errors = new();
        int successCount = 0;

        try
        {
            var itemArray = JsonNode.Parse(items)?.AsArray();
            if (itemArray == null) return new { error = "Invalid JSON array" };

            foreach (var item in itemArray)
            {
                try
                {
                    var formId = item?["formId"]?.GetValue<string>();
                    var name = item?["name"]?.GetValue<string>();
                    var count = item?["count"]?.GetValue<int>() ?? 1;

                    var resolvedId = formId ?? SkyrimOffsets.GetItemFormId(name ?? "") ?? name;
                    if (string.IsNullOrEmpty(resolvedId))
                    {
                        errors.Add(new { item = name ?? formId, error = "Could not resolve FormID" });
                        continue;
                    }

                    await _pipe.SendRequestAsync("add_item", new JsonObject
                    {
                        ["formId"] = resolvedId,
                        ["count"] = count
                    });
                    successCount++;
                }
                catch (Exception ex)
                {
                    errors.Add(new { item = item?.ToJsonString(), error = ex.Message });
                }
            }
        }
        catch (Exception ex)
        {
            return new { error = $"Failed to parse items: {ex.Message}" };
        }

        await NotifyInGame($"Added {successCount} items");
        return new { success = true, added = successCount, failed = errors.Count, errors };
    }

    [McpServerTool]
    [Description("Get the player's appearance data: race, sex, weight, height, all 19 face morph values, " +
        "face presets, and head parts (hair, eyes, etc.). Use this for character recreation or face morphing.")]
    public async Task<object> GetAppearance()
    {
        var data = await _pipe.SendRequestAsync("get_appearance");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get the player's favorited spells and items (the ones assigned to the favorites menu/hotkeys).")]
    public async Task<object> GetFavorites()
    {
        var data = await _pipe.SendRequestAsync("get_favorites");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
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

        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("Get the reference ID of whatever the player's crosshair is pointing at. " +
        "Returns refId and baseId. Use refId with dot-notation for console commands (e.g., 'refId.kill'). " +
        "Use baseId for commands like setessential. " +
        "Use this when the player says 'that NPC' or 'this object' — it tells you exactly what they're looking at.")]
    public async Task<object> GetCrosshairRef()
    {
        var data = await _pipe.SendRequestAsync("get_crosshair_ref");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
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
    [Description("Get nearby NPCs within a given radius. Returns refId (use for dot-notation console commands like 'refId.kill') " +
        "and baseId (use for setessential). Also includes name, race, level, distance, hostility, and dead status.")]
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

        // Notification is handled by the plugin after save completes
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
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
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
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
    }

    [McpServerTool]
    [Description("List all installed SKSE plugins with integration potential. " +
        "Identifies plugins like ConsoleUtilSSE, HDT-SMP, PapyrusUtil, RaceMenu, TrueHUD, etc. " +
        "and notes which ones have APIs that could extend MCP capabilities.")]
    public async Task<object> GetLoadedSKSEPlugins()
    {
        var data = await _pipe.SendRequestAsync("get_loaded_skse_plugins");
        return (object?)JsonSerializer.Deserialize<JsonElement>(data?.ToJsonString() ?? "{}") ?? new { error = "No data returned" };
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
}
