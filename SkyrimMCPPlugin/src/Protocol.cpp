#include "Protocol.h"
#include "EventSystem.h"
#include "GameInterface.h"
#include "TaskQueue.h"

#include <SKSE/SKSE.h>

namespace SkyrimMCP::Protocol {

    static json MakeResponse(const std::string& id, bool success, const json& data, const std::string& error = "") {
        json response;
        response["id"] = id;
        response["success"] = success;
        if (success) {
            response["data"] = data;
        } else {
            response["error"] = error;
        }
        return response;
    }

    std::string HandleRequest(const std::string& requestLine) {
        json request;
        try {
            request = json::parse(requestLine);
        } catch (const json::parse_error& e) {
            return MakeResponse("", false, {}, std::string("JSON parse error: ") + e.what()).dump() + "\n";
        }

        std::string id = request.value("id", "");
        std::string action = request.value("action", "");
        json params = request.value("params", json::object());

        try {
            // ping is handled directly — no game thread needed
            if (action == "ping") {
                return MakeResponse(id, true, json::object()).dump() + "\n";
            }

            // poll_events — drain queued game events, no game thread needed
            // Optional filters: "eventTypes" (array of types to include), "excludeTypes" (array to exclude)
            if (action == "poll_events") {
                auto events = EventSystem::GetSingleton().DrainEvents();

                // Apply filters if provided
                auto includeTypes = params.value("eventTypes", json::array());
                auto excludeTypes = params.value("excludeTypes", json::array());

                json arr = json::array();
                for (auto& e : events) {
                    std::string eventType = e.value("event", "");

                    // Include filter
                    if (!includeTypes.empty()) {
                        bool found = false;
                        for (auto& t : includeTypes) {
                            if (t.get<std::string>() == eventType) { found = true; break; }
                        }
                        if (!found) continue;
                    }

                    // Exclude filter
                    if (!excludeTypes.empty()) {
                        bool excluded = false;
                        for (auto& t : excludeTypes) {
                            if (t.get<std::string>() == eventType) { excluded = true; break; }
                        }
                        if (excluded) continue;
                    }

                    arr.push_back(e);
                }
                return MakeResponse(id, true, {{"events", arr}, {"count", arr.size()}}).dump() + "\n";
            }

            // All other actions are dispatched to the game thread
            json result;

            if (action == "get_player_info") {
                result = TaskQueue::RunOnGameThread([]() {
                    return GameInterface::GetPlayerInfo();
                });
            }
            else if (action == "get_inventory") {
                result = TaskQueue::RunOnGameThread([]() {
                    return GameInterface::GetInventory();
                });
            }
            else if (action == "get_gold_count") {
                result = TaskQueue::RunOnGameThread([]() {
                    return GameInterface::GetGoldCount();
                });
            }
            else if (action == "execute_command") {
                std::string command = params.value("command", "");
                if (command.empty()) {
                    return MakeResponse(id, false, {}, "Missing 'command' parameter").dump() + "\n";
                }
                result = TaskQueue::RunOnGameThread([command]() {
                    return GameInterface::ExecuteConsoleCommand(command);
                });
            }
            else if (action == "add_item") {
                std::string formId = params.value("formId", "");
                int count = params.value("count", 1);
                if (formId.empty()) {
                    return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                }
                result = TaskQueue::RunOnGameThread([formId, count]() {
                    return GameInterface::AddItem(formId, count);
                });
            }
            else if (action == "remove_item") {
                std::string formId = params.value("formId", "");
                int count = params.value("count", 1);
                if (formId.empty()) {
                    return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                }
                result = TaskQueue::RunOnGameThread([formId, count]() {
                    return GameInterface::RemoveItem(formId, count);
                });
            }
            else if (action == "set_actor_value") {
                std::string attribute = params.value("attribute", "");
                float value = params.value("value", 0.0f);
                if (attribute.empty()) {
                    return MakeResponse(id, false, {}, "Missing 'attribute' parameter").dump() + "\n";
                }
                result = TaskQueue::RunOnGameThread([attribute, value]() {
                    return GameInterface::SetActorValue(attribute, value);
                });
            }
            else if (action == "teleport") {
                std::string cellId = params.value("cellId", "");
                if (cellId.empty()) {
                    return MakeResponse(id, false, {}, "Missing 'cellId' parameter").dump() + "\n";
                }
                result = TaskQueue::RunOnGameThread([cellId]() {
                    return GameInterface::Teleport(cellId);
                });
            }
            else if (action == "toggle_god_mode") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::ToggleGodMode(); });
            }
            else if (action == "toggle_collision") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::ToggleCollision(); });
            }
            else if (action == "get_quest_info") {
                result = TaskQueue::RunOnGameThread([]() {
                    return GameInterface::GetQuestInfo();
                });
            }
            else if (action == "get_nearby_npcs") {
                float radius = params.value("radius", 4096.0f);
                result = TaskQueue::RunOnGameThread([radius]() {
                    return GameInterface::GetNearbyNPCs(radius);
                });
            }
            // Quest Intelligence (Phase 4)
            else if (action == "get_quest_stages") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::GetQuestStages(formId); });
            }
            else if (action == "get_quest_aliases") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::GetQuestAliases(formId); });
            }
            else if (action == "get_quest_items") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetQuestItems(); });
            }
            // Quest management (existing)
            else if (action == "get_quest_stage") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::GetQuestStage(formId); });
            }
            else if (action == "set_quest_stage") {
                std::string formId = params.value("formId", "");
                int stage = params.value("stage", 0);
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId, stage]() { return GameInterface::SetQuestStage(formId, stage); });
            }
            else if (action == "complete_quest") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::CompleteQuest(formId); });
            }
            else if (action == "start_quest") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::StartQuest(formId); });
            }
            else if (action == "stop_quest") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::StopQuest(formId); });
            }
            // Spells / perks
            else if (action == "add_spell") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::AddSpell(formId); });
            }
            else if (action == "remove_spell") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::RemoveSpell(formId); });
            }
            else if (action == "add_perk") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::AddPerk(formId); });
            }
            else if (action == "remove_perk") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::RemovePerk(formId); });
            }
            // Active effects
            else if (action == "get_active_effects") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetActiveEffects(); });
            }
            // NPC interaction
            else if (action == "get_actor_info") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::GetActorInfo(formId); });
            }
            else if (action == "set_actor_value_on") {
                std::string actorId = params.value("actorFormId", "");
                std::string attribute = params.value("attribute", "");
                float value = params.value("value", 0.0f);
                if (actorId.empty() || attribute.empty()) return MakeResponse(id, false, {}, "Missing parameters").dump() + "\n";
                result = TaskQueue::RunOnGameThread([actorId, attribute, value]() { return GameInterface::SetActorValueOn(actorId, attribute, value); });
            }
            else if (action == "move_actor_to") {
                std::string actorId = params.value("actorFormId", "");
                std::string target = params.value("target", "");
                if (actorId.empty()) return MakeResponse(id, false, {}, "Missing 'actorFormId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([actorId, target]() { return GameInterface::MoveActorTo(actorId, target); });
            }
            else if (action == "set_relationship_rank") {
                std::string actorId = params.value("actorFormId", "");
                int rank = params.value("rank", 0);
                if (actorId.empty()) return MakeResponse(id, false, {}, "Missing 'actorFormId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([actorId, rank]() { return GameInterface::SetRelationshipRank(actorId, rank); });
            }
            // NPC & Faction (Phase 5)
            else if (action == "get_player_factions") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetPlayerFactions(); });
            }
            else if (action == "get_npc_detailed_info") {
                std::string refId = params.value("refId", "");
                if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([refId]() { return GameInterface::GetNPCDetailedInfo(refId); });
            }
            else if (action == "get_npc_inventory") {
                std::string refId = params.value("refId", "");
                if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([refId]() { return GameInterface::GetNPCInventory(refId); });
            }
            else if (action == "get_followers") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetFollowers(); });
            }
            else if (action == "get_detection_level") {
                std::string refId = params.value("refId", "");
                if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([refId]() { return GameInterface::GetDetectionLevel(refId); });
            }
            // Equipment
            else if (action == "equip_item") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::EquipItem(formId); });
            }
            else if (action == "unequip_item") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::UnequipItem(formId); });
            }
            // World state
            else if (action == "get_game_time") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetGameTime(); });
            }
            else if (action == "set_game_time") {
                float hours = params.value("hours", 12.0f);
                result = TaskQueue::RunOnGameThread([hours]() { return GameInterface::SetGameTime(hours); });
            }
            // World (Phase 3)
            else if (action == "get_weather") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetWeather(); });
            }
            else if (action == "list_weathers") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::ListWeathers(); });
            }
            else if (action == "set_weather") {
                std::string weather = params.value("weather", "");
                if (weather.empty()) return MakeResponse(id, false, {}, "Missing 'weather' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([weather]() { return GameInterface::SetWeather(weather); });
            }
            else if (action == "get_cell_info") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetCellInfo(); });
            }
            else if (action == "get_nearby_objects") {
                float radius = params.value("radius", 2048.0f);
                std::string typeFilter = params.value("type", "all");
                result = TaskQueue::RunOnGameThread([radius, typeFilter]() { return GameInterface::GetNearbyObjects(radius, typeFilter); });
            }
            else if (action == "unlock_door") {
                std::string refId = params.value("refId", "");
                if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([refId]() { return GameInterface::UnlockDoor(refId); });
            }
            else if (action == "lock_door") {
                std::string refId = params.value("refId", "");
                int lockLevel = params.value("lockLevel", 1);
                if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([refId, lockLevel]() { return GameInterface::LockDoor(refId, lockLevel); });
            }
            else if (action == "get_container_inventory") {
                std::string refId = params.value("refId", "");
                if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([refId]() { return GameInterface::GetContainerInventory(refId); });
            }
            else if (action == "discover_all_map_markers") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::DiscoverAllMapMarkers(); });
            }
            else if (action == "get_crosshair_ref") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetCrosshairRef(); });
            }
            else if (action == "kill_actor") {
                std::string refId = params.value("refId", "");
                if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([refId]() { return GameInterface::KillActor(refId); });
            }
            else if (action == "get_equipped_items") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetEquippedItems(); });
            }
            // Character Blueprint (Phase 2)
            else if (action == "get_known_spells") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetKnownSpells(); });
            }
            else if (action == "get_known_shouts") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetKnownShouts(); });
            }
            else if (action == "get_skill_levels") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetSkillLevels(); });
            }
            else if (action == "get_perks") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetPerks(); });
            }
            else if (action == "get_appearance") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetAppearance(); });
            }
            else if (action == "get_favorites") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetFavorites(); });
            }
            else if (action == "get_character_blueprint") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetCharacterBlueprint(); });
            }
            else if (action == "unlock_shout") {
                std::string formId = params.value("formId", "");
                if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([formId]() { return GameInterface::UnlockShout(formId); });
            }
            else if (action == "is_in_combat") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::IsInCombat(); });
            }
            // Notifications
            else if (action == "show_notification") {
                std::string message = params.value("message", "");
                if (message.empty()) return MakeResponse(id, false, {}, "Missing 'message' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([message]() { return GameInterface::ShowNotification(message); });
            }
            // Search
            else if (action == "search_forms") {
                std::string query = params.value("query", "");
                std::string typeFilter = params.value("type", "all");
                int maxResults = params.value("maxResults", 25);
                if (query.empty()) return MakeResponse(id, false, {}, "Missing 'query' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([query, typeFilter, maxResults]() { return GameInterface::SearchForms(query, typeFilter, maxResults); });
            }
            // Save game — fire and forget, don't wait for completion
            else if (action == "save_game") {
                std::string saveName = params.value("saveName", "MCPSave");
                // Queue save on game thread but don't block pipe waiting for it
                SKSE::GetTaskInterface()->AddTask([saveName]() {
                    GameInterface::SaveGame(saveName);
                });
                return MakeResponse(id, true, {{"saving", true}, {"saveName", saveName}}).dump() + "\n";
            }
            // Load order
            else if (action == "get_load_order") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetLoadOrder(); });
            }
            else if (action == "get_loaded_skse_plugins") {
                result = TaskQueue::RunOnGameThread([]() { return GameInterface::GetLoadedSKSEPlugins(); });
            }
            else if (action == "get_mod_formid_prefix") {
                std::string modName = params.value("modName", "");
                if (modName.empty()) return MakeResponse(id, false, {}, "Missing 'modName' parameter").dump() + "\n";
                result = TaskQueue::RunOnGameThread([modName]() { return GameInterface::GetModFormIdPrefix(modName); });
            }
            else {
                return MakeResponse(id, false, {}, "Unknown action: " + action).dump() + "\n";
            }

            // Check if the result itself contains an error field
            if (result.contains("error")) {
                return MakeResponse(id, false, {}, result["error"].get<std::string>()).dump() + "\n";
            }

            return MakeResponse(id, true, result).dump() + "\n";

        } catch (const std::exception& e) {
            SKSE::log::error("Error handling action '{}': {}", action, e.what());
            return MakeResponse(id, false, {}, e.what()).dump() + "\n";
        }
    }

}
