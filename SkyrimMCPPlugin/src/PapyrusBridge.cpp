#include "PapyrusBridge.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <future>

namespace SkyrimMCP::PapyrusBridge {

    using json = nlohmann::json;

    // ==================== Catalog Cache ====================

    static std::mutex s_catalogMutex;
    static json s_catalog;
    static bool s_catalogLoaded = false;

    static std::string GetCatalogPath() {
        auto skyrimPath = std::filesystem::current_path();
        return (skyrimPath / "Data" / "SKSE" / "Plugins" / "SkyrimMCP_PapyrusCatalog.json").string();
    }

    // ==================== PSC Scanner ====================

    struct ParsedFunction {
        std::string name;
        std::string returnType;
        std::vector<std::pair<std::string, std::string>> params;  // type, name
        bool isGlobal = false;
    };

    static std::vector<ParsedFunction> ParseNativeFunctions(const std::string& content) {
        std::vector<ParsedFunction> functions;

        // Match: [ReturnType] function FuncName(params) [global] native
        std::regex funcRegex(
            R"(^\s*(?:(\w+)\s+)?function\s+(\w+)\s*\(([^)]*)\)\s*(.*?native.*?)$)",
            std::regex::icase);

        std::regex paramRegex(R"((\w+)\s+(\w+))");

        auto begin = std::sregex_iterator(content.begin(), content.end(), funcRegex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            auto& match = *it;
            std::string modifiers = match[4].str();

            // Only native functions
            std::string lowerMod = modifiers;
            std::transform(lowerMod.begin(), lowerMod.end(), lowerMod.begin(), ::tolower);
            if (lowerMod.find("native") == std::string::npos) continue;

            ParsedFunction func;
            func.returnType = match[1].matched ? match[1].str() : "void";
            func.name = match[2].str();
            func.isGlobal = lowerMod.find("global") != std::string::npos;

            // Parse parameters
            std::string paramStr = match[3].str();
            auto pBegin = std::sregex_iterator(paramStr.begin(), paramStr.end(), paramRegex);
            auto pEnd = std::sregex_iterator();
            for (auto pit = pBegin; pit != pEnd; ++pit) {
                func.params.push_back({(*pit)[1].str(), (*pit)[2].str()});
            }

            functions.push_back(func);
        }

        return functions;
    }

    json ScanPapyrusSources() {
        auto skyrimPath = std::filesystem::current_path();
        auto sourceDir = skyrimPath / "Data" / "Scripts" / "Source";

        if (!std::filesystem::exists(sourceDir)) {
            return {{"error", "Scripts/Source directory not found"}};
        }

        json catalog;
        catalog["version"] = 1;
        catalog["source"] = "runtime_psc_scan";
        int totalScripts = 0;
        int totalFunctions = 0;

        json scripts = json::object();

        for (auto& entry : std::filesystem::directory_iterator(sourceDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".psc") continue;

            try {
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::string content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
                file.close();

                auto functions = ParseNativeFunctions(content);
                if (functions.empty()) continue;

                // Get script name from scriptName declaration or filename
                std::string scriptName = entry.path().stem().string();
                std::regex scriptNameRegex(R"(^\s*scriptName\s+(\w+))", std::regex::icase);
                std::smatch snMatch;
                if (std::regex_search(content, snMatch, scriptNameRegex)) {
                    scriptName = snMatch[1].str();
                }

                json scriptJson;
                scriptJson["sourceFile"] = entry.path().filename().string();
                scriptJson["functionCount"] = functions.size();

                json funcArray = json::array();
                for (auto& func : functions) {
                    json f;
                    f["name"] = func.name;
                    f["returnType"] = func.returnType;
                    f["isGlobal"] = func.isGlobal;

                    json params = json::array();
                    for (auto& [type, name] : func.params) {
                        params.push_back({{"type", type}, {"name", name}});
                    }
                    f["params"] = params;
                    funcArray.push_back(f);
                }
                scriptJson["functions"] = funcArray;

                scripts[scriptName] = scriptJson;
                totalScripts++;
                totalFunctions += functions.size();

            } catch (...) {
                continue;
            }
        }

        catalog["totalScripts"] = totalScripts;
        catalog["totalFunctions"] = totalFunctions;
        catalog["scripts"] = scripts;

        // Write to cache file
        try {
            std::ofstream outFile(GetCatalogPath());
            if (outFile.is_open()) {
                outFile << catalog.dump(2);
                outFile.close();
                SKSE::log::info("Papyrus catalog written: {} scripts, {} functions",
                    totalScripts, totalFunctions);
            }
        } catch (...) {
            SKSE::log::error("Failed to write Papyrus catalog file");
        }

        return catalog;
    }

    json GetPapyrusCatalog() {
        std::lock_guard lock(s_catalogMutex);

        if (s_catalogLoaded) {
            return s_catalog;
        }

        // Try loading cached catalog
        try {
            std::ifstream file(GetCatalogPath());
            if (file.is_open()) {
                s_catalog = json::parse(file);
                s_catalogLoaded = true;
                SKSE::log::info("Papyrus catalog loaded from cache: {} scripts",
                    s_catalog.value("totalScripts", 0));
                return s_catalog;
            }
        } catch (...) {}

        // No cache — scan now
        SKSE::log::info("No Papyrus catalog cache found, scanning...");
        s_catalog = ScanPapyrusSources();
        s_catalogLoaded = true;
        return s_catalog;
    }

    json GetScriptFunctions(const std::string& className) {
        auto catalog = GetPapyrusCatalog();
        auto& scripts = catalog["scripts"];

        if (scripts.contains(className)) {
            return scripts[className];
        }

        // Case-insensitive search
        std::string lowerName = className;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        for (auto& [name, data] : scripts.items()) {
            std::string lowerScript = name;
            std::transform(lowerScript.begin(), lowerScript.end(), lowerScript.begin(), ::tolower);
            if (lowerScript == lowerName) {
                return data;
            }
        }

        return {{"error", "Script class not found: " + className}};
    }

    json GetScriptsOnRef(const std::string& refFormIdHex) {
        RE::FormID formId = 0;
        try {
            if (refFormIdHex.empty() || refFormIdHex == "player") {
                formId = 0x14u;
            } else {
                std::size_t idx = 0;
                unsigned long val = std::stoul(refFormIdHex, &idx, 16);
                if (idx != refFormIdHex.size())
                    throw std::invalid_argument("trailing chars");
                formId = static_cast<RE::FormID>(val);
            }
        } catch (...) {
            return {{"error", "Invalid refId: " + refFormIdHex}};
        }

        auto* form = RE::TESForm::LookupByID(formId);
        if (!form) {
            return {{"error", "Form not found: " + refFormIdHex}};
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            return {{"error", "Papyrus VM not available"}};
        }

        auto* policy = vm->handlePolicy;
        if (!policy) {
            return {{"error", "VM handle policy not available"}};
        }

        RE::VMHandle handle = policy->GetHandleForObject(form->GetFormType(), form);
        if (handle == policy->EmptyHandle()) {
            return {
                {"refId", refFormIdHex},
                {"scripts", json::array()},
                {"count", 0}
            };
        }

        json scripts = json::array();
        {
            RE::BSSpinLockGuard lock{vm->attachedScriptsLock};
            auto it = vm->attachedScripts.find(handle);
            if (it != vm->attachedScripts.end()) {
                for (auto& attachedScript : it->second) {
                    auto* obj = attachedScript.get();
                    if (obj) {
                        auto* typeInfo = obj->GetTypeInfo();
                        if (typeInfo) {
                            const char* name = typeInfo->GetName();
                            if (name && name[0] != '\0') {
                                scripts.push_back(std::string(name));
                            }
                        }
                    }
                }
            }
        }

        return {
            {"refId", refFormIdHex},
            {"scripts", scripts},
            {"count", scripts.size()}
        };
    }

    // ==================== Pending timers / VM registrations (item 115) ====================
    // READ-ONLY. SkyrimVM keeps every RegisterForUpdate / RegisterForSingleUpdate(GameTime) as an
    // UpdateDataEvent in two lock-guarded arrays, keyed by VM handle. A script whose update chain died has
    // NO entry here -- which is the observation that "stuck quest / dead framework" diagnosis has lacked.
    // Handles: for a form, low 32 bits = FormID (quest aliases etc. use other handle types; they appear in
    // "all" mode with formId = low 32 bits and whatever LookupByID finds, or null).
    namespace {
        json describeHandle(RE::VMHandle h) {
            char buf[20];
            std::snprintf(buf, sizeof(buf), "%016llX", static_cast<unsigned long long>(h));
            const auto fid = static_cast<RE::FormID>(h & 0xFFFFFFFFull);
            char fbuf[12];
            std::snprintf(fbuf, sizeof(fbuf), "%08X", fid);
            // The low 32 bits are the FormID ONLY for a form-owned handle; alias and active-effect handles use
            // other layouts, so the lookup below is labelled a GUESS unless the handle type is the form's own
            // (brief 55 Q9d -- unverified layout, so the label is deliberate).
            const auto htype = static_cast<std::uint32_t>(h >> 32);
            json j = {{"handle", buf}, {"formId_low32", fbuf}, {"handleType", htype}};
            if (auto* f = RE::TESForm::LookupByID(fid)) {
                const char* name = f->GetName();
                const char* edid = f->GetFormEditorID();
                const bool own = htype == static_cast<std::uint32_t>(f->GetFormType());
                j["owner_confidence"] = own ? "form handle (type matches)" : "GUESS (handle type != form type)";
                j["formType"] = RE::FormTypeToString(f->GetFormType());
                if (name && name[0]) j["name"] = name;
                if (edid && edid[0]) j["editorId"] = edid;
            }
            return j;
        }

        // Primitive copy taken UNDER the queue lock; everything else (form lookup, names, JSON) happens after
        // release, because the game thread needs these locks to schedule and fire events (Codex brief 55 A4).
        struct UpdateSnap {
            RE::VMHandle handle;
            bool repeat;
            bool gameTime;
            std::uint32_t updateTime;
            std::uint32_t timeToSendEvent;
        };
        struct LosSnap {
            RE::VMHandle handle;
            RE::FormID viewer;
            RE::FormID target;
            int type;
        };
        constexpr std::size_t kMaxAllRows = 500;

        json describeUpdate(const UpdateSnap& e, std::uint32_t now) {
            json j = describeHandle(e.handle);
            j["clock"] = e.gameTime ? "game" : "real";
            j["kind"] = e.repeat ? "repeat" : "single";
            j["updateTime_raw"] = e.updateTime;
            j["timeToSendEvent_raw"] = e.timeToSendEvent;
            // Modular: both are uint32 clocks that wrap (~49.7 days of real ms); valid while the interval is
            // under half the clock range (brief 55 B5).
            const auto remaining = static_cast<std::int64_t>(static_cast<std::int32_t>(e.timeToSendEvent - now));
            const bool gameTime = e.gameTime;
            j["remaining_raw"] = remaining;
            // Derived, labelled as such: real clock is milliseconds; game clock is days x 1000 (SkyrimVM.h).
            if (gameTime) {
                j["interval_game_hours_derived"] = e.updateTime / 1000.0 * 24.0;
                j["remaining_game_hours_derived"] = remaining / 1000.0 * 24.0;
            } else {
                j["interval_seconds_derived"] = e.updateTime / 1000.0;
                j["remaining_seconds_derived"] = remaining / 1000.0;
            }
            return j;
        }
    }

    static json GetScriptTimersImpl(const std::string& refFormIdHex);

    // Every response, including the early returns, carries the provisional-runtime warning (brief 56 C1).
    json GetScriptTimers(const std::string& refFormIdHex) {
        json out = GetScriptTimersImpl(refFormIdHex);
        if (out.is_object()) {
            out["unverified"] = "SkyrimVM member offsets come from CommonLibSSE-NG's reverse-engineered header and "
                                "are NOT yet validated on AE 1.6.1170; *_derived units come from its comments (real "
                                "deadlines vs currentVMMenuModeTime, game deadlines vs currentVMDaysPassed x1000). "
                                "Treat every value as provisional until a live check (Codex briefs 55/56).";
        }
        return out;
    }

    static json GetScriptTimersImpl(const std::string& refFormIdHex) {
        auto* svm = RE::SkyrimVM::GetSingleton();
        if (!svm) {
            return {{"error", "SkyrimVM not available"}};
        }
        const bool all = refFormIdHex == "all";
        RE::VMHandle want = 0;
        if (!all) {
            RE::FormID formId = 0;
            try {
                if (refFormIdHex.empty() || refFormIdHex == "player") {
                    formId = 0x14u;
                } else {
                    std::size_t idx = 0;
                    unsigned long val = std::stoul(refFormIdHex, &idx, 16);
                    if (idx != refFormIdHex.size())
                        throw std::invalid_argument("trailing chars");
                    formId = static_cast<RE::FormID>(val);
                }
            } catch (...) {
                return {{"error", "Invalid refId: " + refFormIdHex}};
            }
            auto* form = RE::TESForm::LookupByID(formId);
            if (!form) {
                return {{"error", "Form not found: " + refFormIdHex}};
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm || !vm->handlePolicy) {
                return {{"error", "Papyrus VM not available"}};
            }
            want = vm->handlePolicy->GetHandleForObject(form->GetFormType(), form);
            if (want == vm->handlePolicy->EmptyHandle()) {
                return {{"refId", refFormIdHex}, {"bound", false},
                        {"note", "no VM handle: nothing script-side is bound to this form, so it cannot hold a timer"}};
            }
        }

        std::uint32_t nowReal = 0, nowGame = 0, nowMenu = 0;
        {
            RE::BSSpinLockGuard lock{svm->currentVMTimeLock};
            nowReal = svm->currentVMTime;
            nowGame = svm->currentVMDaysPassed;
            nowMenu = svm->currentVMMenuModeTime;  // header: real deadlines = updateTime + this clock (brief 56 B5)
        }
        std::vector<UpdateSnap> usnap;
        std::size_t totalReal = 0, totalGame = 0;
        bool truncated = false;
        {
            RE::BSSpinLockGuard lock{svm->queuedOnUpdateEventLock};
            totalReal = svm->queuedOnUpdateEvents.size();
            totalGame = svm->queuedOnUpdateGameEvents.size();
            usnap.reserve(std::min<std::size_t>(totalReal + totalGame, kMaxAllRows));
            auto take = [&](auto& arr, bool game) {
                for (auto& p : arr) {
                    if (!p || !(all || p->handle == want)) continue;
                    if (usnap.size() >= kMaxAllRows) { truncated = true; return; }
                    usnap.push_back({p->handle, p->updateType == RE::SkyrimVM::UpdateDataEvent::UpdateType::kRepeat,
                                     game, p->updateTime, p->timeToSendEvent});
                }
            };
            take(svm->queuedOnUpdateEvents, false);
            take(svm->queuedOnUpdateGameEvents, true);
        }
        std::vector<LosSnap> lsnap;
        {
            RE::BSSpinLockGuard lock{svm->queuedLOSEventCheckLock};
            for (auto& p : svm->queuedLOSEventChecks) {
                if (!p || !(all || p->handle == want)) continue;
                if (usnap.size() + lsnap.size() >= kMaxAllRows) { truncated = true; break; }  // ONE shared cap (brief 56 B7)
                lsnap.push_back({p->handle, p->akViewerFormID, p->akTargetFormID, static_cast<int>(p->losEventType)});
            }
        }
        // Locks released: now resolve and serialise.
        json updates = json::array();
        for (auto& e : usnap) updates.push_back(describeUpdate(e, e.gameTime ? nowGame : nowMenu));
        json los = json::array();
        for (auto& e : lsnap) {
            json j = describeHandle(e.handle);
            char v[12], t[12];
            std::snprintf(v, sizeof(v), "%08X", e.viewer);
            std::snprintf(t, sizeof(t), "%08X", e.target);
            j["viewer"] = v;
            j["target"] = t;
            j["event"] = e.type == 0 ? "gain" : e.type == 1 ? "lost" : "both";
            los.push_back(j);
        }
        json out = {
            {"refId", all ? "all" : refFormIdHex},
            {"updates", updates},
            {"lineOfSight", los},
            {"truncated_at", truncated ? json(kMaxAllRows) : json(nullptr)},
            {"queue_totals", {{"onUpdate", totalReal}, {"onUpdateGameTime", totalGame}}},
            {"vm_clock", {{"currentVMTime_ms", nowReal}, {"currentVMMenuModeTime_ms", nowMenu}, {"currentVMDaysPassed_x1000", nowGame}}},
        };
        // Primitives only under each lock; JSON after release (brief 56 A4).
        std::size_t w1 = 0, w2 = 0, w3 = 0;
        {
            RE::BSSpinLockGuard lock{svm->queuedWaitEventLock};
            w1 = svm->queuedWaitCalls.size();
            w2 = svm->queuedWaitMenuModeCalls.size();
            w3 = svm->queuedWaitGameCalls.size();
        }
        out["pending_waits"] = {{"wait", w1}, {"waitMenuMode", w2}, {"waitGameTime", w3}};
        if (!all) {
            bool sleep = false, stats = false, inv = false;
            {
                RE::BSSpinLockGuard lock{svm->registeredSleepEventsLock};
                sleep = svm->registeredSleepEvents.contains(want);
            }
            {
                RE::BSSpinLockGuard lock{svm->registeredStatsEventsLock};
                stats = svm->registeredStatsEvents.contains(want);
            }
            {
                RE::BSSpinLockGuard lock{svm->InventoryEventFilterMapLock};
                inv = svm->InventoryEventFilterMap.contains(want);
            }
            out["handle"] = describeHandle(want)["handle"];
            out["registeredForSleep"] = sleep;
            out["registeredForTrackedStats"] = stats;
            out["hasInventoryEventFilter"] = inv;
        }
        out["not_covered"] = "SKSE registrations (RegisterForKey/Menu/ModEvent/CameraState/ControlDown...) live in "
                             "SKSE's own RegistrationSets, and animation events on the actor's graph; neither is "
                             "in SkyrimVM, so their absence here proves nothing.";
        return out;
    }

    // ==================== VM Call Bridge ====================

    // Custom callback functor that stores the result and signals a promise
    class PapyrusCallback : public RE::BSScript::IStackCallbackFunctor {
    public:
        std::promise<json> promise;
        // Resolve the promise at most once. The VM holds its own refcounted
        // pointer to this callback (passed by ref to DispatchStaticCall), so a
        // late/duplicate invocation can fire after CallPapyrusFunction has already
        // returned on timeout — set_value() on an already-satisfied or reader-gone
        // promise would otherwise throw std::future_error and escape. (Audit 19 LOW.)
        std::atomic<bool> resolved{false};

        PapyrusCallback() = default;

        void operator()(RE::BSScript::Variable a_result) override {
            if (resolved.exchange(true)) return;  // duplicate callback — ignore
            try {
                json result;
                auto rawType = a_result.GetType().GetRawType();

                if (rawType == RE::BSScript::TypeInfo::RawType::kNone) {
                    result = nullptr;
                } else if (rawType == RE::BSScript::TypeInfo::RawType::kString) {
                    result = std::string(a_result.GetString());
                } else if (rawType == RE::BSScript::TypeInfo::RawType::kInt) {
                    result = a_result.GetSInt();
                } else if (rawType == RE::BSScript::TypeInfo::RawType::kFloat) {
                    result = a_result.GetFloat();
                } else if (rawType == RE::BSScript::TypeInfo::RawType::kBool) {
                    result = a_result.GetBool();
                } else {
                    result = "complex_type";
                }

                promise.set_value({{"result", result}});
            } catch (...) {
                try { promise.set_value({{"error", "Failed to unpack result"}}); } catch (...) {}
            }
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    json CallPapyrusFunction(const std::string& className,
                              const std::string& functionName,
                              const json& args) {
        auto* vm = RE::SkyrimVM::GetSingleton();
        if (!vm || !vm->impl) {
            return {{"error", "Papyrus VM not available"}};
        }

        SKSE::log::info("Calling Papyrus: {}.{}({} args)", className, functionName, args.size());

        // Build function arguments from JSON array
        // Uses a custom IFunctionArguments that packs Variables at runtime
        class DynamicArgs : public RE::BSScript::IFunctionArguments {
        public:
            std::vector<RE::BSScript::Variable> vars;

            bool operator()(RE::BSScrapArray<RE::BSScript::Variable>& a_dst) const override {
                a_dst.resize(vars.size());
                for (size_t i = 0; i < vars.size(); i++) {
                    a_dst[i] = vars[i];
                }
                return true;
            }
        };

        // OWNERSHIP: the Skyrim VM takes ownership of the IFunctionArguments* and
        // frees it — this is CommonLibSSE's own contract (RE::MakeFunctionArguments
        // returns a raw `new` pointer that SendEvent/DispatchStaticCall consume and
        // delete; it is never freed by the caller). So this raw `new` is correct and
        // is NOT a leak. Do NOT wrap it in a unique_ptr / delete it here — that is a
        // double-free (verified against CommonLibSSE-NG headers 2026-06-13; this
        // reverses the earlier "dynArgs leaks" audit finding, which was wrong).
        auto* dynArgs = new DynamicArgs();

        if (args.is_array()) {
            for (auto& arg : args) {
                RE::BSScript::Variable v;
                if (arg.is_string()) {
                    v.SetString(arg.get<std::string>());
                } else if (arg.is_number_integer()) {
                    v.Pack(arg.get<std::int32_t>());
                } else if (arg.is_number_float()) {
                    v.SetFloat(arg.get<float>());
                } else if (arg.is_boolean()) {
                    v.SetBool(arg.get<bool>());
                }
                // TODO: Form types would need FormID resolution
                dynArgs->vars.push_back(v);
            }
        }

        RE::BSScript::IFunctionArguments* funcArgs = dynArgs;

        // Create callback
        auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>(new PapyrusCallback());
        auto* callbackPtr = static_cast<PapyrusCallback*>(callback.get());

        auto future = callbackPtr->promise.get_future();

        // Dispatch the call
        bool dispatched = vm->impl->DispatchStaticCall(
            RE::BSFixedString(className),
            RE::BSFixedString(functionName),
            funcArgs,
            callback);

        if (!dispatched) {
            return {{"error", "Failed to dispatch Papyrus call: " + className + "." + functionName}};
        }

        // Wait for result with timeout
        auto status = future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::timeout) {
            return {{"error", "Papyrus call timed out: " + className + "." + functionName},
                    {"note", "Function may have completed but result was not received in time"}};
        }

        auto result = future.get();
        result["className"] = className;
        result["functionName"] = functionName;
        return result;
    }

}
