#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <excpt.h>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace SkyrimMCP::Helpers {

    using json = nlohmann::json;

    // ==================== Form ID / Actor Value Helpers ====================

    inline RE::FormID ParseFormId(const std::string& hex) {
        return static_cast<RE::FormID>(std::stoul(hex, nullptr, 16));
    }

    inline std::optional<RE::ActorValue> ResolveActorValue(const std::string& attribute) {
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

    // ==================== Name Resolution ====================

    inline std::string GetRefName(RE::TESObjectREFR* ref) {
        if (!ref) return "unknown";
        auto name = ref->GetName();
        return (name && name[0]) ? name : std::format("{:08X}", ref->GetFormID());
    }

    inline std::string GetFormName(RE::FormID formId) {
        auto* form = RE::TESForm::LookupByID(formId);
        if (!form) return std::format("{:08X}", formId);
        auto name = form->GetName();
        return (name && name[0]) ? name : std::format("{:08X}", formId);
    }

    // ==================== Spell / Magic Helpers ====================

    inline std::string ActorValueToSchool(RE::ActorValue av) {
        switch (av) {
            case RE::ActorValue::kAlteration: return "Alteration";
            case RE::ActorValue::kConjuration: return "Conjuration";
            case RE::ActorValue::kDestruction: return "Destruction";
            case RE::ActorValue::kIllusion: return "Illusion";
            case RE::ActorValue::kRestoration: return "Restoration";
            case RE::ActorValue::kEnchanting: return "Enchanting";
            default: return "Other";
        }
    }

    inline std::string SpellTypeToString(RE::MagicSystem::SpellType type) {
        switch (type) {
            case RE::MagicSystem::SpellType::kSpell: return "spell";
            case RE::MagicSystem::SpellType::kDisease: return "disease";
            case RE::MagicSystem::SpellType::kPower: return "power";
            case RE::MagicSystem::SpellType::kLesserPower: return "lesserPower";
            case RE::MagicSystem::SpellType::kAbility: return "ability";
            case RE::MagicSystem::SpellType::kPoison: return "poison";
            case RE::MagicSystem::SpellType::kAddiction: return "addiction";
            case RE::MagicSystem::SpellType::kVoicePower: return "shout";
            default: return "other";
        }
    }

    // ==================== Console Output Capture ====================
    //
    // Hooks ConsoleLog's internal print function to capture all output.
    // The hook intercepts every call to ConsoleLog::Print/VPrint and
    // appends output to a capture buffer when capturing is active.

    // Global capture state
    inline std::mutex& GetCaptureMutex() {
        static std::mutex mutex;
        return mutex;
    }
    inline std::string& GetCaptureBuffer() {
        static std::string buffer;
        return buffer;
    }
    inline bool& IsCapturing() {
        static bool capturing = false;
        return capturing;
    }

    // Original function pointer — set by InstallConsoleHook
    using ConsolePrint_t = void(RE::ConsoleLog*, const char*, std::va_list);
    inline ConsolePrint_t* OriginalVPrint = nullptr;

    // Our detour — captures output then calls original
    // NOTE: This function replaces VPrint in memory. It MUST have the same
    // calling convention and signature. On x64, member functions use
    // RCX=this, RDX=fmt, R8=va_list (fastcall).
    inline void HookedVPrint(RE::ConsoleLog* a_this, const char* a_fmt, std::va_list a_args) {
        // Capture output if active
        if (IsCapturing() && a_fmt) {
            try {
                va_list argsCopy;
                va_copy(argsCopy, a_args);
                char buf[4096];
                int len = vsnprintf(buf, sizeof(buf), a_fmt, argsCopy);
                va_end(argsCopy);
                if (len > 0) {
                    std::lock_guard lock(GetCaptureMutex());
                    auto& capBuf = GetCaptureBuffer();
                    if (capBuf.size() < 65536) {
                        capBuf.append(buf, std::min(len, (int)sizeof(buf) - 1));
                        capBuf += "\n";
                    }
                }
            } catch (...) {}
        }
        // Call original — critical: if null, output is lost but no crash
        if (OriginalVPrint) {
            OriginalVPrint(a_this, a_fmt, a_args);
        }
    }

    // Install the hook — call once after engine init (kDataLoaded)
    inline void InstallConsoleHook() {
        // VPrint is at RELOCATION_ID(50180, 51110)
        REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(50180, 51110) };

        SKSE::log::info("Attempting console hook at {:X}", target.address());

        // Verify the target looks valid
        auto* ptr = reinterpret_cast<std::uint8_t*>(target.address());
        if (!ptr) {
            SKSE::log::error("Console hook target is null");
            return;
        }

        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(128);

        // Try write_branch<5> first, fall back to <6>
        bool hooked = false;
        try {
            OriginalVPrint = reinterpret_cast<ConsolePrint_t*>(
                trampoline.write_branch<5>(target.address(), reinterpret_cast<std::uintptr_t>(&HookedVPrint)));
            hooked = (OriginalVPrint != nullptr);
            if (hooked) {
                SKSE::log::info("Console hook installed (branch<5>), original at {:X}",
                    reinterpret_cast<std::uintptr_t>(OriginalVPrint));
            }
        } catch (...) {
            SKSE::log::warn("write_branch<5> failed, trying <6>");
        }

        if (!hooked) {
            try {
                OriginalVPrint = reinterpret_cast<ConsolePrint_t*>(
                    trampoline.write_branch<6>(target.address(), reinterpret_cast<std::uintptr_t>(&HookedVPrint)));
                hooked = (OriginalVPrint != nullptr);
                if (hooked) {
                    SKSE::log::info("Console hook installed (branch<6>), original at {:X}",
                        reinterpret_cast<std::uintptr_t>(OriginalVPrint));
                }
            } catch (...) {
                SKSE::log::error("write_branch<6> also failed — console output capture disabled");
            }
        }

        if (!hooked) {
            SKSE::log::error("Console hook could not be installed — output capture disabled");
            OriginalVPrint = nullptr;
        }
    }

    inline void BeginCapture() {
        std::lock_guard lock(GetCaptureMutex());
        GetCaptureBuffer().clear();
        GetCaptureBuffer().reserve(65536);
        IsCapturing() = true;
    }

    inline std::string EndCapture() {
        std::lock_guard lock(GetCaptureMutex());
        IsCapturing() = false;
        auto& buf = GetCaptureBuffer();
        if (buf.size() > 65536) {
            buf.resize(65536);
            buf += "\n... [output truncated at 64KB]";
        }
        return std::move(buf);
    }

    // Fallback — read lastMessage directly
    inline std::string CaptureConsoleOutput() {
        // If hook is installed, this shouldn't be needed
        // but serves as fallback
        auto* console = RE::ConsoleLog::GetSingleton();
        if (!console) return "";
        return std::string(console->lastMessage);
    }

    inline void ClearConsoleOutput() {
        auto* console = RE::ConsoleLog::GetSingleton();
        if (console) {
            std::memset(console->lastMessage, 0, sizeof(console->lastMessage));
        }
    }

    // ==================== Console Command Execution ====================
    //
    // Console command execution approach adapted from ConsoleUtilSSE by VersuchDrei
    // (https://github.com/VersuchDrei/ConsoleUtilSSE, MIT License)
    //
    // Key insight: CommonLibSSE-NG's Script::CompileAndRun() uses a relocation that
    // crashes on game versions >= 1.6.1130. ConsoleUtilSSE uses a version-conditional
    // relocation (441582 for patch >= 1130) that works correctly.

    inline void CompileAndRunImpl(RE::Script* script, RE::ScriptCompiler* compiler,
        RE::COMPILER_NAME name, RE::TESObjectREFR* targetRef) {
        using func_t = decltype(CompileAndRunImpl);
        REL::Relocation<func_t> func{
            RELOCATION_ID(21416, REL::Module::get().version().patch() < 1130 ? 21890 : 441582)
        };
        return func(script, compiler, name, targetRef);
    }

    inline void CompileAndRunSafe(RE::Script* script, RE::TESObjectREFR* targetRef,
        RE::COMPILER_NAME name = RE::COMPILER_NAME::kSystemWindowCompiler) {
        RE::ScriptCompiler compiler;
        CompileAndRunImpl(script, &compiler, name, targetRef);
    }

    // SEH wrapper for safety
    inline bool RunScriptSEH(RE::Script* script, RE::TESObjectREFR* targetRef) {
        __try {
            CompileAndRunSafe(script, targetRef);
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline json ExecuteConsoleCommand(const std::string& command) {
        // Console command execution using the approach from ConsoleUtilSSE
        // by VersuchDrei (https://github.com/VersuchDrei/ConsoleUtilSSE, MIT License)

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
        if (!factory) return {{"error", "Script factory not available"}};

        auto* script = factory->Create();
        if (!script) return {{"error", "Failed to create script"}};

        // Clear and start capturing console output
        ClearConsoleOutput();
        BeginCapture();

        script->SetCommand(command);
        bool ok = RunScriptSEH(script, player);

        // Get captured output from hook
        std::string hookOutput = EndCapture();
        // Also get lastMessage as fallback
        std::string lastMsg = CaptureConsoleOutput();

        // Use whichever has more content
        std::string output = hookOutput.size() >= lastMsg.size() ? hookOutput : lastMsg;

        if (!ok) {
            SKSE::log::error("Command '{}' caused an access violation — caught by SEH", command);
            return {{"error", "Command failed (caught safely): " + command},
                    {"command", command},
                    {"partialOutput", output}};
        }

        SKSE::log::info("Command '{}' — hook: {} bytes, lastMsg: {} bytes",
            command, hookOutput.size(), lastMsg.size());

        json result;
        result["executed"] = true;
        result["command"] = command;
        if (!output.empty()) result["output"] = output;
        return result;
    }

}
