#include "GameInterface.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <excpt.h>
#include <format>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace SkyrimMCP::GameInterface {

    // ==================== Console Output Capture ====================
    //
    // Two capture mechanisms:
    // 1. VPrint hook — intercepts every Print() call, captures full multi-line output
    // 2. lastMessage fallback — reads the 1024-byte buffer for the last line only
    //
    // The hook is installed once. During command execution, output is appended
    // to a thread-safe buffer. If anything goes wrong, we fall back to lastMessage.

    static std::mutex s_captureMutex;
    static std::string s_captureBuffer;
    static bool s_capturing = false;
    static bool s_hookInstalled = false;

    // Trampoline storage for original VPrint
    static REL::Relocation<decltype(&RE::ConsoleLog::VPrint)> s_originalVPrint;

    // Our VPrint hook — captures output when s_capturing is true
    static void Hook_VPrint(RE::ConsoleLog* a_this, const char* a_fmt, std::va_list a_args) {
        if (s_capturing) {
            try {
                char buf[4096];
                int len = vsnprintf(buf, sizeof(buf), a_fmt, a_args);
                if (len > 0) {
                    std::lock_guard lock(s_captureMutex);
                    s_captureBuffer.append(buf, std::min(len, (int)sizeof(buf) - 1));
                    s_captureBuffer += "\n";
                }
            } catch (...) {
                // Silently ignore capture errors — never crash the game for logging
            }
        }

        // Always call original so console UI still works
        s_originalVPrint(a_this, a_fmt, a_args);
    }

    // lastMessage fallback
    static std::string CaptureLastConsoleOutput() {
        auto* console = RE::ConsoleLog::GetSingleton();
        if (!console) return "";
        return std::string(console->lastMessage);
    }

    static void ClearConsoleOutput() {
        auto* console = RE::ConsoleLog::GetSingleton();
        if (console) {
            std::memset(console->lastMessage, 0, sizeof(console->lastMessage));
        }
    }

    static void BeginCapture() {
        std::lock_guard lock(s_captureMutex);
        s_captureBuffer.clear();
        // Cap buffer to prevent unbounded growth
        s_captureBuffer.reserve(65536);
        s_capturing = true;
    }

    static std::string EndCapture() {
        std::lock_guard lock(s_captureMutex);
        s_capturing = false;
        // Truncate if absurdly large (safety rail)
        if (s_captureBuffer.size() > 65536) {
            s_captureBuffer.resize(65536);
            s_captureBuffer += "\n... [output truncated at 64KB]";
        }
        return std::move(s_captureBuffer);
    }

    // --- Helpers ---

    static RE::FormID ParseFormId(const std::string& hex) {
        return static_cast<RE::FormID>(std::stoul(hex, nullptr, 16));
    }

    static std::optional<RE::ActorValue> ResolveActorValue(const std::string& attribute) {
        static const std::unordered_map<std::string, RE::ActorValue> map = {
            {"health", RE::ActorValue::kHealth}, {"magicka", RE::ActorValue::kMagicka},
            {"stamina", RE::ActorValue::kStamina}, {"carryweight", RE::ActorValue::kCarryWeight},
            {"speedmult", RE::ActorValue::kSpeedMult}, {"onehanded", RE::ActorValue::kOneHanded},
            {"twohanded", RE::ActorValue::kTwoHanded}, {"archery", RE::ActorValue::kArchery},
            {"block", RE::ActorValue::kBlock}, {"smithing", RE::ActorValue::kSmithing},
            {"heavyarmor", RE::ActorValue::kHeavyArmor}, {"lightarmor", RE::ActorValue::kLightArmor},
            {"pickpocket", RE::ActorValue::kPickpocket}, {"lockpicking", RE::ActorValue::kLockpicking},
            {"sneak", RE::ActorValue::kSneak}, {"alchemy", RE::ActorValue::kAlchemy},
            {"speech", RE::ActorValue::kSpeech}, {"alteration", RE::ActorValue::kAlteration},
            {"conjuration", RE::ActorValue::kConjuration}, {"destruction", RE::ActorValue::kDestruction},
            {"illusion", RE::ActorValue::kIllusion}, {"restoration", RE::ActorValue::kRestoration},
            {"enchanting", RE::ActorValue::kEnchanting},
        };
        auto it = map.find(attribute);
        if (it != map.end()) return it->second;
        return std::nullopt;
    }

    // ==================== Player State ====================

    json GetPlayerInfo() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto* base = player->GetActorBase();
        auto pos = player->GetPosition();
        auto* cell = player->GetParentCell();
        auto* worldspace = player->GetWorldspace();
        auto* avo = player->AsActorValueOwner();

        json result;
        result["name"] = base ? base->GetName() : "unknown";
        result["race"] = base && base->GetRace() ? base->GetRace()->GetName() : "unknown";
        result["level"] = player->GetLevel();

        if (avo) {
            result["health"] = avo->GetActorValue(RE::ActorValue::kHealth);
            result["healthMax"] = avo->GetBaseActorValue(RE::ActorValue::kHealth);
            result["magicka"] = avo->GetActorValue(RE::ActorValue::kMagicka);
            result["magickaMax"] = avo->GetBaseActorValue(RE::ActorValue::kMagicka);
            result["stamina"] = avo->GetActorValue(RE::ActorValue::kStamina);
            result["staminaMax"] = avo->GetBaseActorValue(RE::ActorValue::kStamina);
        }

        result["posX"] = pos.x;
        result["posY"] = pos.y;
        result["posZ"] = pos.z;
        result["cellName"] = cell ? cell->GetName() : "unknown";
        result["worldspaceName"] = worldspace ? worldspace->GetName() : "interior";

        return result;
    }

    json GetInventory() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto inv = player->GetInventory();
        json items = json::array();

        for (auto& [form, data] : inv) {
            if (!form || data.first <= 0) continue;

            json item;
            item["formId"] = std::format("{:08X}", form->GetFormID());
            item["name"] = form->GetName();
            item["count"] = data.first;

            switch (form->GetFormType()) {
                case RE::FormType::Weapon: item["type"] = "weapon"; break;
                case RE::FormType::Armor: item["type"] = "armor"; break;
                case RE::FormType::AlchemyItem: item["type"] = "potion"; break;
                case RE::FormType::Ingredient: item["type"] = "ingredient"; break;
                case RE::FormType::Book: item["type"] = "book"; break;
                case RE::FormType::Misc: item["type"] = "misc"; break;
                case RE::FormType::Ammo: item["type"] = "ammo"; break;
                case RE::FormType::SoulGem: item["type"] = "soulgem"; break;
                case RE::FormType::KeyMaster: item["type"] = "key"; break;
                case RE::FormType::Scroll: item["type"] = "scroll"; break;
                default: item["type"] = "other"; break;
            }

            if (auto* boundObj = form->As<RE::TESBoundObject>()) {
                item["weight"] = boundObj->GetWeight();
                item["value"] = boundObj->GetGoldValue();
            }

            items.push_back(item);
        }

        return {{"items", items}};
    }

    json GetGoldCount() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        int count = 0;
        if (gold) {
            auto inv = player->GetInventory([gold](const RE::TESBoundObject& obj) {
                return obj.GetFormID() == gold->GetFormID();
            });
            for (auto& [form, data] : inv) {
                count += data.first;
            }
        }

        return {{"gold", count}};
    }

    json GetActiveEffects() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto* magicTarget = player->AsMagicTarget();
        if (!magicTarget) return {{"error", "Magic target not available"}};

        auto* effectList = magicTarget->GetActiveEffectList();
        json effects = json::array();

        if (effectList) {
            for (auto& activeEffect : *effectList) {
                if (!activeEffect) continue;
                auto* baseEffect = activeEffect->GetBaseObject();

                json e;
                e["name"] = baseEffect ? baseEffect->GetName() : "unknown";
                if (baseEffect) {
                    e["formId"] = std::format("{:08X}", baseEffect->GetFormID());
                }
                e["magnitude"] = activeEffect->magnitude;
                e["duration"] = activeEffect->duration;
                e["elapsed"] = activeEffect->elapsedSeconds;
                effects.push_back(e);
            }
        }

        return {{"effects", effects}};
    }

    // ==================== World Interaction ====================

    // Separate function for SEH — MSVC can't mix __try with C++ destructors
    static bool RunScriptSEH(RE::Script* script, RE::PlayerCharacter* player) {
        __try {
            script->CompileAndRun(player, RE::COMPILER_NAME::kDefaultCompiler);
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    json ExecuteConsoleCommand(const std::string& command) {
        // Reuse a single Script instance — creating and deleting Script objects
        // via IFormFactory corrupts memory and crashes the game.
        static RE::Script* cachedScript = nullptr;
        if (!cachedScript) {
            auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
            if (!factory) return {{"error", "Script factory not available"}};
            cachedScript = factory->Create();
            if (!cachedScript) return {{"error", "Failed to create script"}};
        }

        // Capture output using both methods
        ClearConsoleOutput();
        BeginCapture();

        cachedScript->SetCommand(command);
        bool ok = RunScriptSEH(cachedScript, RE::PlayerCharacter::GetSingleton());

        if (!ok) {
            SKSE::log::error("Command '{}' caused an access violation — caught by SEH", command);
        }

        std::string hookOutput = EndCapture();
        std::string lastMsg = CaptureLastConsoleOutput();

        if (!ok) {
            return {{"error", "Command caused a crash (caught safely): " + command},
                    {"command", command},
                    {"partialOutput", hookOutput.empty() ? lastMsg : hookOutput}};
        }

        // Use hook output if we got it, otherwise fall back to lastMessage
        std::string output = hookOutput.empty() ? lastMsg : hookOutput;

        // Log both for debugging
        SKSE::log::info("Command '{}' — hook captured {} bytes, lastMessage {} bytes",
            command, hookOutput.size(), lastMsg.size());

        json result;
        result["executed"] = true;
        result["command"] = command;
        if (!output.empty()) {
            result["output"] = output;
        }
        // Include both for diagnostics while we're testing
        if (!hookOutput.empty() && !lastMsg.empty() && hookOutput != lastMsg) {
            result["hookOutput"] = hookOutput;
            result["lastMessage"] = lastMsg;
        }
        return result;
    }

    json AddItem(const std::string& formIdHex, int count) {
        try {
            auto* form = RE::TESForm::LookupByID(ParseFormId(formIdHex));
            if (!form) return {{"error", "Form not found: " + formIdHex}};

            auto* boundObj = form->As<RE::TESBoundObject>();
            if (!boundObj) return {{"error", "Not a valid item: " + formIdHex}};

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return {{"error", "Player not available"}};

            player->AddObjectToContainer(boundObj, nullptr, count, nullptr);
            return {{"added", true}, {"formId", formIdHex}, {"count", count}, {"name", form->GetName()}};
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    json RemoveItem(const std::string& formIdHex, int count) {
        try {
            auto* form = RE::TESForm::LookupByID(ParseFormId(formIdHex));
            if (!form) return {{"error", "Form not found: " + formIdHex}};

            auto* boundObj = form->As<RE::TESBoundObject>();
            if (!boundObj) return {{"error", "Not a valid item: " + formIdHex}};

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return {{"error", "Player not available"}};

            player->RemoveItem(boundObj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            return {{"removed", true}, {"formId", formIdHex}, {"count", count}, {"name", form->GetName()}};
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    json SetActorValue(const std::string& attribute, float value) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto av = ResolveActorValue(attribute);
        if (!av) return {{"error", "Unknown attribute: " + attribute}};

        auto* avo = player->AsActorValueOwner();
        if (!avo) return {{"error", "Actor value owner not available"}};

        avo->SetActorValue(*av, value);
        return {{"set", true}, {"attribute", attribute}, {"value", value}};
    }

    json Teleport(const std::string& cellId) {
        return ExecuteConsoleCommand("coc " + cellId);
    }

    json GetNearbyNPCs(float radius) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto playerPos = player->GetPosition();
        json npcs = json::array();

        auto* processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) return {{"error", "Process lists not available"}};

        for (auto& actorHandle : processLists->highActorHandles) {
            auto actorPtr = actorHandle.get();
            if (!actorPtr) continue;

            auto* actor = actorPtr.get();
            if (!actor || actor == player) continue;

            auto actorPos = actor->GetPosition();
            float dx = actorPos.x - playerPos.x;
            float dy = actorPos.y - playerPos.y;
            float dz = actorPos.z - playerPos.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (distance > radius) continue;

            auto* base = actor->GetActorBase();
            json npc;
            npc["formId"] = std::format("{:08X}", actor->GetFormID());
            npc["name"] = actor->GetName();
            npc["race"] = base && base->GetRace() ? base->GetRace()->GetName() : "unknown";
            npc["level"] = actor->GetLevel();
            npc["distance"] = distance;
            npc["isDead"] = actor->IsDead();
            npc["isHostile"] = actor->IsHostileToActor(player);

            npcs.push_back(npc);
        }

        return {{"npcs", npcs}};
    }

    // ==================== Quest Management ====================

    json GetQuestInfo() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return {{"error", "Data handler not available"}};

        json quests = json::array();

        for (auto* quest : dataHandler->GetFormArray<RE::TESQuest>()) {
            try {
                if (!quest || !quest->IsActive()) continue;

                json q;
                q["formId"] = std::format("{:08X}", quest->GetFormID());
                q["name"] = quest->GetName() ? quest->GetName() : "";
                q["currentStage"] = quest->GetCurrentStageID();
                q["completed"] = quest->IsCompleted();

                json objectives = json::array();
                for (auto& obj : quest->objectives) {
                    if (obj && obj->initialized) {
                        json o;
                        o["index"] = obj->index;
                        o["text"] = obj->displayText.c_str() ? obj->displayText.c_str() : "";
                        o["state"] = static_cast<int>(obj->state.get());
                        objectives.push_back(o);
                    }
                }
                q["objectives"] = objectives;
                quests.push_back(q);
            } catch (...) {
                // Skip quests with bad data (common with modded games)
                continue;
            }
        }

        return {{"quests", quests}};
    }

    json GetQuestStage(const std::string& formIdHex) {
        try {
            auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(ParseFormId(formIdHex));
            if (!quest) return {{"error", "Quest not found: " + formIdHex}};

            json result;
            result["formId"] = formIdHex;
            result["name"] = quest->GetName();
            result["currentStage"] = quest->GetCurrentStageID();
            result["isActive"] = quest->IsActive();
            result["isCompleted"] = quest->IsCompleted();
            result["isRunning"] = quest->IsRunning();

            json objectives = json::array();
            for (auto& obj : quest->objectives) {
                if (obj && obj->initialized) {
                    objectives.push_back({
                        {"index", obj->index},
                        {"text", obj->displayText.c_str()},
                        {"state", static_cast<int>(obj->state.get())}
                    });
                }
            }
            result["objectives"] = objectives;
            return result;
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    json SetQuestStage(const std::string& formIdHex, int stage) {
        return ExecuteConsoleCommand(std::format("setstage {} {}", formIdHex, stage));
    }

    json CompleteQuest(const std::string& formIdHex) {
        return ExecuteConsoleCommand(std::format("completeallobjectives {}", formIdHex));
    }

    json StartQuest(const std::string& formIdHex) {
        return ExecuteConsoleCommand(std::format("startquest {}", formIdHex));
    }

    json StopQuest(const std::string& formIdHex) {
        return ExecuteConsoleCommand(std::format("stopquest {}", formIdHex));
    }

    // ==================== Spells / Perks ====================

    json AddSpell(const std::string& formIdHex) {
        try {
            auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(ParseFormId(formIdHex));
            if (!spell) return {{"error", "Spell not found: " + formIdHex}};

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return {{"error", "Player not available"}};

            player->AddSpell(spell);
            return {{"added", true}, {"formId", formIdHex}, {"name", spell->GetName()}};
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    json RemoveSpell(const std::string& formIdHex) {
        // Use console command — RemoveSpell API varies across versions
        return ExecuteConsoleCommand(std::format("player.removespell {}", formIdHex));
    }

    json AddPerk(const std::string& formIdHex) {
        try {
            auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(ParseFormId(formIdHex));
            if (!perk) return {{"error", "Perk not found: " + formIdHex}};

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return {{"error", "Player not available"}};

            player->AddPerk(perk);
            return {{"added", true}, {"formId", formIdHex}, {"name", perk->GetName()}};
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    json RemovePerk(const std::string& formIdHex) {
        return ExecuteConsoleCommand(std::format("player.removeperk {}", formIdHex));
    }

    // ==================== NPC Interaction ====================

    json GetActorInfo(const std::string& formIdHex) {
        try {
            auto* form = RE::TESForm::LookupByID(ParseFormId(formIdHex));
            if (!form) return {{"error", "Form not found: " + formIdHex}};

            auto* actor = form->As<RE::Actor>();
            if (!actor) return {{"error", "Not an actor: " + formIdHex}};

            auto* base = actor->GetActorBase();
            auto pos = actor->GetPosition();
            auto* avo = actor->AsActorValueOwner();

            json result;
            result["formId"] = formIdHex;
            result["name"] = actor->GetName();
            result["race"] = base && base->GetRace() ? base->GetRace()->GetName() : "unknown";
            result["level"] = actor->GetLevel();
            result["isDead"] = actor->IsDead();
            result["isInCombat"] = actor->IsInCombat();
            result["posX"] = pos.x;
            result["posY"] = pos.y;
            result["posZ"] = pos.z;

            if (avo) {
                result["health"] = avo->GetActorValue(RE::ActorValue::kHealth);
                result["healthMax"] = avo->GetBaseActorValue(RE::ActorValue::kHealth);
            }

            auto* cell = actor->GetParentCell();
            result["cellName"] = cell ? cell->GetName() : "unknown";

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                result["isHostile"] = actor->IsHostileToActor(player);
            }

            return result;
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    json SetActorValueOn(const std::string& actorFormIdHex, const std::string& attribute, float value) {
        try {
            auto* form = RE::TESForm::LookupByID(ParseFormId(actorFormIdHex));
            if (!form) return {{"error", "Form not found: " + actorFormIdHex}};

            auto* actor = form->As<RE::Actor>();
            if (!actor) return {{"error", "Not an actor: " + actorFormIdHex}};

            auto av = ResolveActorValue(attribute);
            if (!av) return {{"error", "Unknown attribute: " + attribute}};

            auto* avo = actor->AsActorValueOwner();
            if (!avo) return {{"error", "Actor value owner not available"}};

            avo->SetActorValue(*av, value);
            return {{"set", true}, {"actor", actorFormIdHex}, {"attribute", attribute}, {"value", value}};
        } catch (...) {
            return {{"error", "Invalid FormID: " + actorFormIdHex}};
        }
    }

    json MoveActorTo(const std::string& actorFormIdHex, const std::string& targetCellOrRefId) {
        // Use console commands for reliability
        ExecuteConsoleCommand(std::format("prid {}", actorFormIdHex));
        return ExecuteConsoleCommand(std::format("moveto player"));
    }

    json SetRelationshipRank(const std::string& actorFormIdHex, int rank) {
        return ExecuteConsoleCommand(
            std::format("setrelationshiprank {} {}", actorFormIdHex, rank));
    }

    // ==================== Equipment ====================

    json EquipItem(const std::string& formIdHex) {
        try {
            auto* form = RE::TESForm::LookupByID(ParseFormId(formIdHex));
            if (!form) return {{"error", "Form not found: " + formIdHex}};

            auto* boundObj = form->As<RE::TESBoundObject>();
            if (!boundObj) return {{"error", "Not an equippable item: " + formIdHex}};

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return {{"error", "Player not available"}};

            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) return {{"error", "Equip manager not available"}};

            equipManager->EquipObject(player, boundObj);
            return {{"equipped", true}, {"formId", formIdHex}, {"name", form->GetName()}};
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    json UnequipItem(const std::string& formIdHex) {
        try {
            auto* form = RE::TESForm::LookupByID(ParseFormId(formIdHex));
            if (!form) return {{"error", "Form not found: " + formIdHex}};

            auto* boundObj = form->As<RE::TESBoundObject>();
            if (!boundObj) return {{"error", "Not an equippable item: " + formIdHex}};

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return {{"error", "Player not available"}};

            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) return {{"error", "Equip manager not available"}};

            equipManager->UnequipObject(player, boundObj);
            return {{"unequipped", true}, {"formId", formIdHex}, {"name", form->GetName()}};
        } catch (...) {
            return {{"error", "Invalid FormID: " + formIdHex}};
        }
    }

    // ==================== World State ====================

    json GetGameTime() {
        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) return {{"error", "Calendar not available"}};

        json result;
        result["gameHour"] = calendar->GetHour();
        result["daysPassed"] = calendar->GetDaysPassed();
        result["day"] = calendar->GetDay();
        result["month"] = calendar->GetMonth();
        result["year"] = calendar->GetYear();
        result["dayOfWeek"] = calendar->GetDayOfWeek();
        return result;
    }

    json SetGameTime(float hours) {
        return ExecuteConsoleCommand(std::format("set gamehour to {:.1f}", hours));
    }

    json IsInCombat() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        return {{"inCombat", player->IsInCombat()}};
    }

    // ==================== Load Order ====================

    json SaveGame(const std::string& saveName) {
        return ExecuteConsoleCommand("save " + saveName);
    }

    json GetLoadOrder() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return {{"error", "Data handler not available"}};

        json mods = json::array();

        // Regular plugins (xx prefix)
        auto loadedModCount = dataHandler->GetLoadedModCount();
        auto** loadedMods = dataHandler->GetLoadedMods();
        for (std::uint8_t i = 0; i < loadedModCount; i++) {
            auto* mod = loadedMods[i];
            if (!mod) continue;

            json m;
            m["name"] = mod->fileName;
            m["index"] = i;
            m["prefix"] = std::format("{:02X}", i);
            m["type"] = "esp";
            mods.push_back(m);
        }

        // Light plugins (FExxx prefix)
        auto lightModCount = dataHandler->GetLoadedLightModCount();
        auto** lightMods = dataHandler->GetLoadedLightMods();
        for (std::uint16_t i = 0; i < lightModCount; i++) {
            auto* mod = lightMods[i];
            if (!mod) continue;

            json m;
            m["name"] = mod->fileName;
            m["index"] = i;
            m["prefix"] = std::format("FE{:03X}", i);
            m["type"] = "esl";
            mods.push_back(m);
        }

        return {{"mods", mods}, {"regularCount", loadedModCount}, {"lightCount", lightModCount}};
    }

    json GetModFormIdPrefix(const std::string& modName) {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return {{"error", "Data handler not available"}};

        // Try regular mod first
        auto modIndex = dataHandler->GetLoadedModIndex(modName);
        if (modIndex.has_value()) {
            return {
                {"mod", modName},
                {"type", "esp"},
                {"index", modIndex.value()},
                {"prefix", std::format("{:02X}", modIndex.value())},
                {"formIdFormat", std::format("{:02X}xxxxxx", modIndex.value())}
            };
        }

        // Try light mod
        auto lightIndex = dataHandler->GetLoadedLightModIndex(modName);
        if (lightIndex.has_value()) {
            return {
                {"mod", modName},
                {"type", "esl"},
                {"index", lightIndex.value()},
                {"prefix", std::format("FE{:03X}", lightIndex.value())},
                {"formIdFormat", std::format("FE{:03X}xxx", lightIndex.value())}
            };
        }

        return {{"error", "Mod not found in load order: " + modName}};
    }

}
