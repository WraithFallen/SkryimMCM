#include "Protocol.h"
#include "EventSystem.h"
#include "GameInterface.h"
#include "TaskQueue.h"

#include <SKSE/SKSE.h>

#include <atomic>
#include <functional>
#include <optional>
#include <unordered_map>

namespace SkyrimMCP::Protocol {

    using Handler = std::function<std::string(const std::string& id, const json& params)>;

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

    // ==================== Safety Gating ====================
    //
    // State-changing actions are classified into tiers and gated against the game
    // safety state (loading / kill-move / saving) before dispatch:
    //   SAVE_LOAD  — save/load/teleport/quest-flow: ALWAYS enforced (corruption /
    //                lost-progress risk).
    //   ARBITRARY  — execute_command / call_papyrus_function: ALWAYS enforced.
    //   DURABLE    — inventory/stats/perks/etc. that persist in the save: enforced
    //                only when the runtime policy flag is on (default OFF — the
    //                /skylink protocol already mitigates procedurally). Toggle via
    //                the `set_safety_policy` action {enforceDurable: bool}.
    //   (anything else is ungated.)
    // A blocked action returns an error and is NOT dispatched; the caller can force
    // it by passing params.override_safety = true.

    enum class SafetyTier { Ungated, Durable, SaveLoad, Arbitrary };

    static std::atomic<bool> g_enforceDurable{false};

    static SafetyTier ClassifyAction(const std::string& action) {
        static const std::unordered_map<std::string, SafetyTier> tiers = {
            // SAVE / LOAD — always enforced
            {"save_game", SafetyTier::SaveLoad},
            {"load_save", SafetyTier::SaveLoad},
            {"load_most_recent_save", SafetyTier::SaveLoad},
            {"teleport", SafetyTier::SaveLoad},
            {"set_quest_stage", SafetyTier::SaveLoad},
            {"start_quest", SafetyTier::SaveLoad},
            {"stop_quest", SafetyTier::SaveLoad},
            {"complete_quest", SafetyTier::SaveLoad},
            // ARBITRARY — open-ended execution, always enforced
            {"execute_command", SafetyTier::Arbitrary},
            {"call_papyrus_function", SafetyTier::Arbitrary},
            // DURABLE MUTATION — enforced only when policy enables it
            {"add_item", SafetyTier::Durable},
            {"remove_item", SafetyTier::Durable},
            {"set_actor_value", SafetyTier::Durable},
            {"set_actor_value_on", SafetyTier::Durable},
            {"set_relationship_rank", SafetyTier::Durable},
            {"add_spell", SafetyTier::Durable},
            {"remove_spell", SafetyTier::Durable},
            {"add_perk", SafetyTier::Durable},
            {"remove_perk", SafetyTier::Durable},
            {"equip_item", SafetyTier::Durable},
            {"unequip_item", SafetyTier::Durable},
            {"unlock_shout", SafetyTier::Durable},
            {"kill_actor", SafetyTier::Durable},
            {"set_level", SafetyTier::Durable},
            {"clear_bounty", SafetyTier::Durable},
            {"lock_door", SafetyTier::Durable},
            {"unlock_door", SafetyTier::Durable},
            {"move_actor_to", SafetyTier::Durable},
            {"set_game_time", SafetyTier::Durable},
            {"set_weather", SafetyTier::Durable},
            {"discover_all_map_markers", SafetyTier::Durable},  // tmm 1 — permanently reveals markers
            // Intentionally NOT gated (reversible runtime toggles, harmless in any
            // state): toggle_god_mode, toggle_immortal_mode, toggle_collision.
            // Also ungated: transient effects (show_notification, play/stop_music,
            // play/stop_idle) and all read-only get_* actions.
            // MAINTENANCE: any NEW action that persists in the save or executes
            // arbitrary code MUST be added here — unlisted actions are Ungated.
        };
        auto it = tiers.find(action);
        return it != tiers.end() ? it->second : SafetyTier::Ungated;
    }

    // Returns a ready-to-send block response if the action must be refused;
    // std::nullopt to let it proceed.
    static std::optional<std::string> CheckSafetyGate(const std::string& id,
            const std::string& action, const json& params) {
        SafetyTier tier = ClassifyAction(action);
        bool enforced = (tier == SafetyTier::SaveLoad) || (tier == SafetyTier::Arbitrary) ||
                        (tier == SafetyTier::Durable && g_enforceDurable.load());
        if (!enforced) return std::nullopt;

        // Explicit caller override
        if (params.value("override_safety", false)) {
            SKSE::log::warn("Action '{}' proceeding despite safety gate (override_safety=true)", action);
            return std::nullopt;
        }

        // Evaluate game safety on the game thread (short timeout so the gate itself
        // never hangs the 5s work budget). A timeout means the game thread is not
        // pumping — i.e. loading — which we treat as unsafe.
        try {
            json safety = TaskQueue::RunOnGameThread([]() { return GameInterface::GetGameSafety(); }, 2000);
            if (safety.value("safe", false)) return std::nullopt;  // safe — allow
            std::string warning = safety.value("warning", "game is in an unsafe state");
            return MakeResponse(id, false, {},
                "Blocked (" + action + "): " + warning +
                ". Retry when safe, or pass params.override_safety=true to force.").dump() + "\n";
        } catch (...) {
            return MakeResponse(id, false, {},
                "Blocked (" + action + "): game is busy or loading (safety check timed out). "
                "Retry when loaded, or pass params.override_safety=true to force.").dump() + "\n";
        }
    }

    // Helper: wrap a no-arg game thread call
    static std::string GameThread(const std::string& id, std::function<json()> fn) {
        auto result = TaskQueue::RunOnGameThread(std::move(fn));
        if (result.contains("error")) {
            return MakeResponse(id, false, {}, result["error"].get<std::string>()).dump() + "\n";
        }
        return MakeResponse(id, true, result).dump() + "\n";
    }

    // Helper: require a string param, then run on game thread
    static std::string RequireStr(const std::string& id, const json& params,
        const char* paramName, std::function<json(const std::string&)> fn) {
        std::string val = params.value(paramName, "");
        if (val.empty()) return MakeResponse(id, false, {}, std::string("Missing '") + paramName + "' parameter").dump() + "\n";
        return GameThread(id, [fn, val]() { return fn(val); });
    }

    static std::unordered_map<std::string, Handler>& GetRegistry() {
        static std::unordered_map<std::string, Handler> registry;
        static bool initialized = false;

        if (initialized) return registry;
        initialized = true;

        // === Direct handlers (no game thread) ===

        registry["ping"] = [](const std::string& id, const json&) {
            return MakeResponse(id, true, json::object()).dump() + "\n";
        };

        // Safety policy control — toggles whether the DURABLE-mutation tier is
        // gated. SAVE/LOAD and ARBITRARY are always gated regardless. Not itself gated.
        registry["set_safety_policy"] = [](const std::string& id, const json& params) {
            if (params.contains("enforceDurable")) {
                g_enforceDurable.store(params.value("enforceDurable", false));
            }
            json result;
            result["enforceDurable"] = g_enforceDurable.load();
            result["note"] = "SAVE/LOAD and ARBITRARY actions are always gated; "
                             "this toggles the DURABLE-mutation tier only.";
            return MakeResponse(id, true, result).dump() + "\n";
        };

        registry["poll_events"] = [](const std::string& id, const json& params) {
            auto events = EventSystem::GetSingleton().DrainEvents();
            auto includeTypes = params.value("eventTypes", json::array());
            auto excludeTypes = params.value("excludeTypes", json::array());

            json arr = json::array();
            for (auto& e : events) {
                std::string eventType = e.value("event", "");
                if (!includeTypes.empty()) {
                    bool found = false;
                    for (auto& t : includeTypes) {
                        if (t.get<std::string>() == eventType) { found = true; break; }
                    }
                    if (!found) continue;
                }
                if (!excludeTypes.empty()) {
                    bool excluded = false;
                    for (auto& t : excludeTypes) {
                        if (t.get<std::string>() == eventType) { excluded = true; break; }
                    }
                    if (excluded) continue;
                }
                arr.push_back(e);
            }

            // Include muted event summary
            auto mutedSummary = EventSystem::GetSingleton().GetMutedSummary();
            json result;
            result["events"] = arr;
            result["count"] = arr.size();
            result["mutedTotal"] = mutedSummary.value("mutedTotal", 0);
            if (mutedSummary.contains("muted") && !mutedSummary["muted"].empty()) {
                result["muted"] = mutedSummary["muted"];
            }
            return MakeResponse(id, true, result).dump() + "\n";
        };

        registry["save_game"] = [](const std::string& id, const json& params) {
            std::string saveName = params.value("saveName", "MCPSave");
            SKSE::GetTaskInterface()->AddTask([saveName]() {
                GameInterface::SaveGame(saveName);
            });
            return MakeResponse(id, true, {{"saving", true}, {"saveName", saveName}}).dump() + "\n";
        };

        // === No-param game thread handlers ===

        auto noParam = [&](const char* name, std::function<json()> fn) {
            registry[name] = [fn](const std::string& id, const json&) {
                return GameThread(id, fn);
            };
        };

        noParam("get_player_info", []() { return GameInterface::GetPlayerInfo(); });

        registry["get_inventory"] = [](const std::string& id, const json& params) {
            std::string refId = params.value("refId", "");
            return GameThread(id, [refId]() { return GameInterface::GetInventory(refId); });
        };
        noParam("get_gold_count", []() { return GameInterface::GetGoldCount(); });
        noParam("get_active_effects", []() { return GameInterface::GetActiveEffects(); });
        noParam("get_equipped_items", []() { return GameInterface::GetEquippedItems(); });
        noParam("get_known_spells", []() { return GameInterface::GetKnownSpells(); });
        noParam("get_known_shouts", []() { return GameInterface::GetKnownShouts(); });
        noParam("get_skill_levels", []() { return GameInterface::GetSkillLevels(); });
        noParam("get_perks", []() { return GameInterface::GetPerks(); });
        noParam("get_appearance", []() { return GameInterface::GetAppearance(); });
        noParam("get_favorites", []() { return GameInterface::GetFavorites(); });
        noParam("get_character_blueprint", []() { return GameInterface::GetCharacterBlueprint(); });
        noParam("is_in_combat", []() { return GameInterface::IsInCombat(); });
        noParam("get_quest_info", []() { return GameInterface::GetQuestInfo(); });
        noParam("get_quest_items", []() { return GameInterface::GetQuestItems(); });
        noParam("toggle_god_mode", []() { return GameInterface::ToggleGodMode(); });
        registry["toggle_collision"] = [](const std::string& id, const json& params) {
            std::string refId = params.value("refId", "");
            return GameThread(id, [refId]() { return GameInterface::ToggleCollision(refId); });
        };
        noParam("get_weather", []() { return GameInterface::GetWeather(); });
        noParam("list_weathers", []() { return GameInterface::ListWeathers(); });
        noParam("get_cell_info", []() { return GameInterface::GetCellInfo(); });
        noParam("get_game_time", []() { return GameInterface::GetGameTime(); });
        noParam("get_crosshair_ref", []() { return GameInterface::GetCrosshairRef(); });
        noParam("get_load_order", []() { return GameInterface::GetLoadOrder(); });
        noParam("get_loaded_skse_plugins", []() { return GameInterface::GetLoadedSKSEPlugins(); });
        noParam("discover_all_map_markers", []() { return GameInterface::DiscoverAllMapMarkers(); });
        registry["get_factions"] = [](const std::string& id, const json& params) {
            std::string refId = params.value("refId", "");
            return GameThread(id, [refId]() { return GameInterface::GetFactions(refId); });
        };
        registry["get_player_factions"] = registry["get_factions"];  // backward compat alias
        noParam("get_bounties", []() { return GameInterface::GetBounties(); });
        noParam("get_magic_resistances", []() { return GameInterface::GetMagicResistances(); });
        noParam("get_disease_status", []() { return GameInterface::GetDiseaseStatus(); });
        noParam("get_powers", []() { return GameInterface::GetPowers(); });
        noParam("get_followers", []() { return GameInterface::GetFollowers(); });
        noParam("toggle_immortal_mode", []() { return GameInterface::ToggleImmortalMode(); });
        noParam("get_combat_state", []() { return GameInterface::GetCombatState(); });
        noParam("get_menu_state", []() { return GameInterface::GetMenuState(); });
        noParam("get_game_safety", []() { return GameInterface::GetGameSafety(); });
        noParam("load_most_recent_save", []() { return GameInterface::LoadMostRecentSave(); });

        // Papyrus Bridge — these run on pipe thread (no game thread needed for catalog)
        registry["get_papyrus_catalog"] = [](const std::string& id, const json&) {
            // Catalog scan is file I/O, safe on any thread
            auto result = GameInterface::GetPapyrusCatalog();
            if (result.contains("error"))
                return MakeResponse(id, false, {}, result["error"].get<std::string>()).dump() + "\n";
            return MakeResponse(id, true, result).dump() + "\n";
        };
        registry["scan_papyrus_sources"] = [](const std::string& id, const json&) {
            auto result = GameInterface::ScanPapyrusSources();
            if (result.contains("error"))
                return MakeResponse(id, false, {}, result["error"].get<std::string>()).dump() + "\n";
            return MakeResponse(id, true, result).dump() + "\n";
        };
        // get_script_functions is registered below with other formIdParam entries
        registry["call_papyrus_function"] = [](const std::string& id, const json& params) {
            std::string className = params.value("className", "");
            std::string functionName = params.value("functionName", "");
            auto args = params.value("args", json::array());
            if (className.empty() || functionName.empty())
                return MakeResponse(id, false, {}, "Missing className or functionName").dump() + "\n";
            // DO NOT use GameThread — DispatchStaticCall queues onto the VM which
            // executes on the game thread. If we block the game thread waiting for
            // the result, we deadlock. Call directly from the pipe thread instead.
            auto result = GameInterface::CallPapyrusFunction(className, functionName, args);
            if (result.contains("error"))
                return MakeResponse(id, false, {}, result["error"].get<std::string>()).dump() + "\n";
            return MakeResponse(id, true, result).dump() + "\n";
        };
        noParam("get_damage_stats", []() { return GameInterface::GetDamageStats(); });

        registry["set_level"] = [](const std::string& id, const json& params) {
            int level = params.value("level", 1);
            return GameThread(id, [level]() { return GameInterface::SetLevel(level); });
        };

        registry["get_threats"] = [](const std::string& id, const json& params) {
            float radius = params.value("radius", 4096.0f);
            return GameThread(id, [radius]() { return GameInterface::GetThreats(radius); });
        };

        registry["get_nearby_merchants"] = [](const std::string& id, const json& params) {
            float radius = params.value("radius", 4096.0f);
            return GameThread(id, [radius]() { return GameInterface::GetNearbyMerchants(radius); });
        };

        // === Single formId/refId param handlers ===

        auto formIdParam = [&](const char* name, const char* paramName, std::function<json(const std::string&)> fn) {
            registry[name] = [paramName, fn](const std::string& id, const json& params) {
                return RequireStr(id, params, paramName, fn);
            };
        };

        formIdParam("get_quest_stage", "formId", [](const std::string& f) { return GameInterface::GetQuestStage(f); });
        formIdParam("get_quest_stages", "formId", [](const std::string& f) { return GameInterface::GetQuestStages(f); });
        formIdParam("get_quest_aliases", "formId", [](const std::string& f) { return GameInterface::GetQuestAliases(f); });
        formIdParam("complete_quest", "formId", [](const std::string& f) { return GameInterface::CompleteQuest(f); });
        formIdParam("start_quest", "formId", [](const std::string& f) { return GameInterface::StartQuest(f); });
        formIdParam("stop_quest", "formId", [](const std::string& f) { return GameInterface::StopQuest(f); });
        formIdParam("add_spell", "formId", [](const std::string& f) { return GameInterface::AddSpell(f); });
        formIdParam("remove_spell", "formId", [](const std::string& f) { return GameInterface::RemoveSpell(f); });
        formIdParam("add_perk", "formId", [](const std::string& f) { return GameInterface::AddPerk(f); });
        formIdParam("remove_perk", "formId", [](const std::string& f) { return GameInterface::RemovePerk(f); });
        formIdParam("get_actor_info", "formId", [](const std::string& f) { return GameInterface::GetActorInfo(f); });
        formIdParam("equip_item", "formId", [](const std::string& f) { return GameInterface::EquipItem(f); });
        formIdParam("unequip_item", "formId", [](const std::string& f) { return GameInterface::UnequipItem(f); });
        formIdParam("unlock_shout", "formId", [](const std::string& f) { return GameInterface::UnlockShout(f); });
        formIdParam("unlock_door", "refId", [](const std::string& r) { return GameInterface::UnlockDoor(r); });
        formIdParam("kill_actor", "refId", [](const std::string& r) { return GameInterface::KillActor(r); });
        registry["play_idle"] = [](const std::string& id, const json& params) {
            std::string idleFormId = params.value("idleFormId", "");
            std::string refId = params.value("refId", "");
            if (idleFormId.empty())
                return MakeResponse(id, false, {}, "Missing idleFormId").dump() + "\n";
            return GameThread(id, [idleFormId, refId]() { return GameInterface::PlayIdle(idleFormId, refId); });
        };
        registry["get_current_idle"] = [](const std::string& id, const json& params) {
            std::string refId = params.value("refId", "");
            return GameThread(id, [refId]() { return GameInterface::GetCurrentIdle(refId); });
        };
        registry["stop_idle"] = [](const std::string& id, const json& params) {
            std::string refId = params.value("refId", "");
            return GameThread(id, [refId]() { return GameInterface::StopIdle(refId); });
        };

        formIdParam("get_npc_detailed_info", "refId", [](const std::string& r) { return GameInterface::GetNPCDetailedInfo(r); });
        formIdParam("get_npc_inventory", "refId", [](const std::string& r) { return GameInterface::GetInventory(r); });
        formIdParam("get_container_inventory", "refId", [](const std::string& r) { return GameInterface::GetContainerInventory(r); });
        formIdParam("get_detection_level", "refId", [](const std::string& r) { return GameInterface::GetDetectionLevel(r); });
        formIdParam("get_mod_formid_prefix", "modName", [](const std::string& m) { return GameInterface::GetModFormIdPrefix(m); });
        formIdParam("set_weather", "weather", [](const std::string& w) { return GameInterface::SetWeather(w); });
        formIdParam("load_save", "saveName", [](const std::string& s) { return GameInterface::LoadSave(s); });
        formIdParam("get_merchant_inventory", "refId", [](const std::string& r) { return GameInterface::GetMerchantInventory(r); });
        formIdParam("clear_bounty", "factionFormId", [](const std::string& f) { return GameInterface::ClearBounty(f); });
        formIdParam("play_music", "formId", [](const std::string& f) { return GameInterface::PlayMusic(f); });
        noParam("stop_music", []() { return GameInterface::StopMusic(); });
        formIdParam("get_spell_details", "formId", [](const std::string& f) { return GameInterface::GetSpellDetails(f); });
        formIdParam("get_enchantment_info", "formId", [](const std::string& f) { return GameInterface::GetEnchantmentInfo(f); });
        formIdParam("get_script_functions", "className", [](const std::string& c) { return GameInterface::GetScriptFunctions(c); });
        formIdParam("get_scripts_on_ref", "refId", [](const std::string& r) { return GameInterface::GetScriptsOnRef(r); });

        // === Multi-param handlers ===

        registry["execute_command"] = [](const std::string& id, const json& params) {
            std::string command = params.value("command", "");
            if (command.empty()) return MakeResponse(id, false, {}, "Missing 'command' parameter").dump() + "\n";
            return GameThread(id, [command]() { return GameInterface::ExecuteConsoleCommand(command); });
        };

        registry["add_item"] = [](const std::string& id, const json& params) {
            std::string formId = params.value("formId", "");
            int count = params.value("count", 1);
            if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
            return GameThread(id, [formId, count]() { return GameInterface::AddItem(formId, count); });
        };

        registry["remove_item"] = [](const std::string& id, const json& params) {
            std::string formId = params.value("formId", "");
            int count = params.value("count", 1);
            if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
            return GameThread(id, [formId, count]() { return GameInterface::RemoveItem(formId, count); });
        };

        registry["set_actor_value"] = [](const std::string& id, const json& params) {
            std::string attribute = params.value("attribute", "");
            float value = params.value("value", 0.0f);
            std::string refId = params.value("refId", "");
            if (attribute.empty()) return MakeResponse(id, false, {}, "Missing 'attribute' parameter").dump() + "\n";
            return GameThread(id, [attribute, value, refId]() { return GameInterface::SetActorValue(attribute, value, refId); });
        };

        registry["teleport"] = [](const std::string& id, const json& params) {
            std::string cellId = params.value("cellId", "");
            if (cellId.empty()) return MakeResponse(id, false, {}, "Missing 'cellId' parameter").dump() + "\n";
            return GameThread(id, [cellId]() { return GameInterface::Teleport(cellId); });
        };

        registry["set_quest_stage"] = [](const std::string& id, const json& params) {
            std::string formId = params.value("formId", "");
            int stage = params.value("stage", 0);
            if (formId.empty()) return MakeResponse(id, false, {}, "Missing 'formId' parameter").dump() + "\n";
            return GameThread(id, [formId, stage]() { return GameInterface::SetQuestStage(formId, stage); });
        };

        registry["set_actor_value_on"] = [](const std::string& id, const json& params) {
            std::string actorId = params.value("actorFormId", "");
            std::string attribute = params.value("attribute", "");
            float value = params.value("value", 0.0f);
            if (actorId.empty() || attribute.empty()) return MakeResponse(id, false, {}, "Missing parameters").dump() + "\n";
            return GameThread(id, [actorId, attribute, value]() { return GameInterface::SetActorValue(attribute, value, actorId); });
        };

        registry["move_actor_to"] = [](const std::string& id, const json& params) {
            std::string actorId = params.value("actorFormId", "");
            std::string target = params.value("target", "");
            if (actorId.empty()) return MakeResponse(id, false, {}, "Missing 'actorFormId' parameter").dump() + "\n";
            return GameThread(id, [actorId, target]() { return GameInterface::MoveActorTo(actorId, target); });
        };

        registry["set_relationship_rank"] = [](const std::string& id, const json& params) {
            std::string actorId = params.value("actorFormId", "");
            int rank = params.value("rank", 0);
            if (actorId.empty()) return MakeResponse(id, false, {}, "Missing 'actorFormId' parameter").dump() + "\n";
            return GameThread(id, [actorId, rank]() { return GameInterface::SetRelationshipRank(actorId, rank); });
        };

        registry["lock_door"] = [](const std::string& id, const json& params) {
            std::string refId = params.value("refId", "");
            int lockLevel = params.value("lockLevel", 1);
            if (refId.empty()) return MakeResponse(id, false, {}, "Missing 'refId' parameter").dump() + "\n";
            return GameThread(id, [refId, lockLevel]() { return GameInterface::LockDoor(refId, lockLevel); });
        };

        registry["get_nearby_npcs"] = [](const std::string& id, const json& params) {
            float radius = params.value("radius", 4096.0f);
            return GameThread(id, [radius]() { return GameInterface::GetNearbyNPCs(radius); });
        };

        registry["get_nearby_objects"] = [](const std::string& id, const json& params) {
            float radius = params.value("radius", 2048.0f);
            std::string typeFilter = params.value("type", "all");
            return GameThread(id, [radius, typeFilter]() { return GameInterface::GetNearbyObjects(radius, typeFilter); });
        };

        registry["set_game_time"] = [](const std::string& id, const json& params) {
            float hours = params.value("hours", 12.0f);
            return GameThread(id, [hours]() { return GameInterface::SetGameTime(hours); });
        };

        registry["show_notification"] = [](const std::string& id, const json& params) {
            std::string message = params.value("message", "");
            if (message.empty()) return MakeResponse(id, false, {}, "Missing 'message' parameter").dump() + "\n";
            return GameThread(id, [message]() { return GameInterface::ShowNotification(message); });
        };

        registry["search_forms"] = [](const std::string& id, const json& params) {
            std::string query = params.value("query", "");
            std::string typeFilter = params.value("type", "all");
            int maxResults = params.value("maxResults", 25);
            if (query.empty()) return MakeResponse(id, false, {}, "Missing 'query' parameter").dump() + "\n";
            return GameThread(id, [query, typeFilter, maxResults]() { return GameInterface::SearchForms(query, typeFilter, maxResults); });
        };

        return registry;
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

        // Safety gate — refuse state-changing actions in unsafe states before dispatch.
        if (auto blocked = CheckSafetyGate(id, action, params)) {
            return *blocked;
        }

        try {
            auto& registry = GetRegistry();
            auto it = registry.find(action);
            if (it == registry.end()) {
                return MakeResponse(id, false, {}, "Unknown action: " + action).dump() + "\n";
            }
            return it->second(id, params);
        } catch (const std::exception& e) {
            SKSE::log::error("Error handling action '{}': {}", action, e.what());
            return MakeResponse(id, false, {}, e.what()).dump() + "\n";
        }
    }

}
