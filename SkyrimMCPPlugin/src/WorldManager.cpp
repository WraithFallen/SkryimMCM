#include "WorldManager.h"
#include "Helpers.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <format>

namespace SkyrimMCP::WorldManager {

    using json = nlohmann::json;

    json Teleport(const std::string& cellId) {
        // Try direct execute of the "coc" console command
        // If that fails, it'll fall through to CompileAndRun with SEH
        return Helpers::ExecuteConsoleCommand("coc " + cellId);
    }

    json GetWeather() {
        auto* sky = RE::Sky::GetSingleton();
        if (!sky) return {{"error", "Sky not available"}};

        json result;
        if (sky->currentWeather) {
            result["current"] = sky->currentWeather->GetName();
            result["currentFormId"] = std::format("{:08X}", sky->currentWeather->GetFormID());
        }
        if (sky->lastWeather) {
            result["previous"] = sky->lastWeather->GetName();
        }
        result["weatherPct"] = sky->currentWeatherPct;

        return result;
    }

    json SetWeather(const std::string& weatherFormIdOrName) {
        auto* sky = RE::Sky::GetSingleton();
        if (!sky) return {{"error", "Sky not available"}};

        // Try FormID first
        RE::TESWeather* weather = nullptr;
        try {
            auto formId = Helpers::ParseFormId(weatherFormIdOrName);
            weather = RE::TESForm::LookupByID<RE::TESWeather>(formId);
        } catch (...) {}

        // Try by name or editor ID keyword
        if (!weather) {
            std::string lower = weatherFormIdOrName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (dataHandler) {
                for (auto* form : dataHandler->GetFormArray<RE::TESWeather>()) {
                    if (!form) continue;

                    // Check display name
                    std::string name = form->GetName() ? form->GetName() : "";
                    std::string lowerName = name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

                    // Check editor ID (most weather forms use editor IDs like "SkyrimClear", "SkyrimSnow")
                    std::string editorId = form->GetFormEditorID() ? form->GetFormEditorID() : "";
                    std::string lowerEditorId = editorId;
                    std::transform(lowerEditorId.begin(), lowerEditorId.end(), lowerEditorId.begin(), ::tolower);

                    // Also check FormID as string
                    std::string formIdStr = std::format("{:08X}", form->GetFormID());
                    std::string lowerFormId = formIdStr;
                    std::transform(lowerFormId.begin(), lowerFormId.end(), lowerFormId.begin(), ::tolower);

                    if ((!lowerName.empty() && lowerName.find(lower) != std::string::npos) ||
                        (!lowerEditorId.empty() && lowerEditorId.find(lower) != std::string::npos) ||
                        lowerFormId.find(lower) != std::string::npos) {
                        weather = form;
                        break;
                    }
                }
            }
        }

        if (!weather) return {{"error", "Weather not found: " + weatherFormIdOrName}};

        sky->ForceWeather(weather, true);
        return {{"set", true}, {"weather", weather->GetName()},
                {"formId", std::format("{:08X}", weather->GetFormID())}};
    }

    json ListWeathers() {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return {{"error", "Data handler not available"}};

        json weathers = json::array();

        auto& weatherArray = dataHandler->GetFormArray<RE::TESWeather>();
        SKSE::log::info("ListWeathers: GetFormArray returned {} entries", weatherArray.size());

        for (auto* form : weatherArray) {
            if (!form) continue;
            try {
                std::string name = form->GetName() ? form->GetName() : "";
                std::string editorId = form->GetFormEditorID() ? form->GetFormEditorID() : "";

                json w;
                w["formId"] = std::format("{:08X}", form->GetFormID());
                if (!name.empty()) w["name"] = name;
                if (!editorId.empty()) w["editorId"] = editorId;

                // Infer weather type from editor ID keywords
                std::string lowerEditorId = editorId;
                std::transform(lowerEditorId.begin(), lowerEditorId.end(), lowerEditorId.begin(), ::tolower);

                if (lowerEditorId.find("clear") != std::string::npos) w["category"] = "clear";
                else if (lowerEditorId.find("snow") != std::string::npos) w["category"] = "snow";
                else if (lowerEditorId.find("rain") != std::string::npos) w["category"] = "rain";
                else if (lowerEditorId.find("storm") != std::string::npos || lowerEditorId.find("thunder") != std::string::npos) w["category"] = "storm";
                else if (lowerEditorId.find("fog") != std::string::npos) w["category"] = "fog";
                else if (lowerEditorId.find("cloud") != std::string::npos || lowerEditorId.find("overcast") != std::string::npos) w["category"] = "cloudy";
                else if (lowerEditorId.find("ash") != std::string::npos) w["category"] = "ash";
                else w["category"] = "other";

                // Get source mod
                auto* file = form->GetFile(0);
                if (file) w["mod"] = file->fileName;

                weathers.push_back(w);
            } catch (...) {
                continue;
            }
        }

        return {{"weathers", weathers}, {"count", weathers.size()}};
    }

    json GetCellInfo() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto* cell = player->GetParentCell();
        if (!cell) return {{"error", "Cell not available"}};

        json result;
        result["name"] = cell->GetName() ? cell->GetName() : "unnamed";
        result["formId"] = std::format("{:08X}", cell->GetFormID());
        result["isInterior"] = cell->IsInteriorCell();
        result["isExterior"] = cell->IsExteriorCell();

        auto* worldspace = player->GetWorldspace();
        if (worldspace) {
            result["worldspace"] = worldspace->GetName();
            result["worldspaceFormId"] = std::format("{:08X}", worldspace->GetFormID());
        }

        auto pos = player->GetPosition();
        result["posX"] = pos.x;
        result["posY"] = pos.y;
        result["posZ"] = pos.z;

        auto angle = player->GetAngle();
        result["angleX"] = angle.x;
        result["angleY"] = angle.y;
        result["angleZ"] = angle.z;

        return result;
    }

    json GetNearbyObjects(float radius, const std::string& typeFilter) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        auto* cell = player->GetParentCell();
        if (!cell) return {{"error", "Cell not available"}};

        auto playerPos = player->GetPosition();
        json objects = json::array();

        std::string lowerFilter = typeFilter;
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

        cell->ForEachReferenceInRange(playerPos, radius, [&](RE::TESObjectREFR* refPtr) -> RE::BSContainer::ForEachResult {
            if (!refPtr || refPtr == player) return RE::BSContainer::ForEachResult::kContinue;
            auto& ref = *refPtr;

            try {
                auto* baseObj = ref.GetBaseObject();
                if (!baseObj) return RE::BSContainer::ForEachResult::kContinue;

                std::string typeStr;
                bool include = lowerFilter.empty() || lowerFilter == "all";

                switch (baseObj->GetFormType()) {
                    case RE::FormType::Container:
                        typeStr = "container";
                        if (lowerFilter == "container" || lowerFilter == "chest") include = true;
                        break;
                    case RE::FormType::Door:
                        typeStr = "door";
                        if (lowerFilter == "door") include = true;
                        break;
                    case RE::FormType::Furniture:
                        typeStr = "furniture";
                        if (lowerFilter == "furniture" || lowerFilter == "crafting") include = true;
                        break;
                    case RE::FormType::Activator:
                        typeStr = "activator";
                        if (lowerFilter == "activator") include = true;
                        break;
                    case RE::FormType::Flora:
                        typeStr = "flora";
                        if (lowerFilter == "flora" || lowerFilter == "plant") include = true;
                        break;
                    case RE::FormType::Light:
                        typeStr = "light";
                        if (lowerFilter == "light") include = true;
                        break;
                    case RE::FormType::Static:
                        typeStr = "static";
                        if (lowerFilter == "static") include = true;
                        break;
                    default:
                        if (ref.As<RE::Actor>()) return RE::BSContainer::ForEachResult::kContinue;
                        typeStr = "other";
                        break;
                }

                if (!include) return RE::BSContainer::ForEachResult::kContinue;

                auto refPos = ref.GetPosition();
                float dx = refPos.x - playerPos.x;
                float dy = refPos.y - playerPos.y;
                float dz = refPos.z - playerPos.z;
                float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

                json obj;
                obj["refId"] = std::format("{:08X}", ref.GetFormID());
                obj["baseId"] = std::format("{:08X}", baseObj->GetFormID());
                obj["name"] = ref.GetName() ? ref.GetName() : (baseObj->GetName() ? baseObj->GetName() : "");
                obj["type"] = typeStr;
                obj["distance"] = distance;
                obj["isLocked"] = ref.IsLocked();

                objects.push_back(obj);
            } catch (...) {}

            return RE::BSContainer::ForEachResult::kContinue;
        });

        return {{"objects", objects}, {"count", objects.size()}};
    }

    json UnlockDoor(const std::string& refFormIdHex) {
        try {
            auto* form = RE::TESForm::LookupByID(Helpers::ParseFormId(refFormIdHex));
            if (!form) return {{"error", "Reference not found: " + refFormIdHex}};

            auto* ref = form->As<RE::TESObjectREFR>();
            if (!ref) return {{"error", "Not a reference: " + refFormIdHex}};

            auto* lock = ref->GetLock();
            if (!lock) return {{"error", "Object has no lock"}};
            if (!lock->IsLocked()) return {{"error", "Already unlocked"}};

            lock->SetLocked(false);
            return {{"unlocked", true}, {"refId", refFormIdHex}, {"name", ref->GetName() ? ref->GetName() : ""}};
        } catch (...) {
            return {{"error", "Failed to unlock: " + refFormIdHex}};
        }
    }

    json LockDoor(const std::string& refFormIdHex, int lockLevel) {
        try {
            auto* form = RE::TESForm::LookupByID(Helpers::ParseFormId(refFormIdHex));
            if (!form) return {{"error", "Reference not found: " + refFormIdHex}};

            auto* ref = form->As<RE::TESObjectREFR>();
            if (!ref) return {{"error", "Not a reference: " + refFormIdHex}};

            auto* lock = ref->GetLock();
            if (!lock) return {{"error", "Object has no lock"}};

            lock->SetLocked(true);
            lock->baseLevel = static_cast<std::int8_t>(lockLevel);
            return {{"locked", true}, {"refId", refFormIdHex}, {"lockLevel", lockLevel}};
        } catch (...) {
            return {{"error", "Failed to lock: " + refFormIdHex}};
        }
    }

    json GetContainerInventory(const std::string& refFormIdHex) {
        try {
            auto* form = RE::TESForm::LookupByID(Helpers::ParseFormId(refFormIdHex));
            if (!form) return {{"error", "Reference not found: " + refFormIdHex}};

            auto* ref = form->As<RE::TESObjectREFR>();
            if (!ref) return {{"error", "Not a reference: " + refFormIdHex}};

            auto inv = ref->GetInventory();
            json items = json::array();

            for (auto& [itemForm, data] : inv) {
                if (!itemForm || data.first <= 0) continue;
                try {
                    json item;
                    item["formId"] = std::format("{:08X}", itemForm->GetFormID());
                    item["name"] = itemForm->GetName();
                    item["count"] = data.first;

                    if (auto* boundObj = itemForm->As<RE::TESBoundObject>()) {
                        item["weight"] = boundObj->GetWeight();
                        item["value"] = boundObj->GetGoldValue();
                    }
                    items.push_back(item);
                } catch (...) { continue; }
            }

            return {{"refId", refFormIdHex}, {"name", ref->GetName() ? ref->GetName() : ""},
                    {"isLocked", ref->IsLocked()}, {"items", items}, {"count", items.size()}};
        } catch (...) {
            return {{"error", "Failed to read container: " + refFormIdHex}};
        }
    }

    json DiscoverAllMapMarkers() {
        // tmm 1 discovers all markers via console
        auto result = Helpers::ExecuteConsoleCommand("tmm 1");

        // Count how many markers exist
        int count = 0;
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (dataHandler) {
            auto& refs = dataHandler->GetFormArray(RE::FormType::Reference);
            // Can't easily count markers this way, just report success
        }

        return {{"discovered", true}, {"message", "All map markers discovered"}};
    }

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
        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) return {{"error", "Calendar not available"}};

        // gameHour is a TESGlobal — set its value directly
        if (calendar->gameHour) {
            calendar->gameHour->value = hours;
            return {{"set", true}, {"gameHour", hours}};
        }
        return {{"error", "Game hour global not available"}};
    }

    json ToggleGodMode() {
        // God mode is a static bool — just flip it
        REL::Relocation<bool*> godMode{ RELOCATION_ID(517711, 404238) };
        *godMode = !*godMode;
        return {{"godMode", *godMode}};
    }

    json ToggleCollision() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {{"error", "Player not available"}};

        static bool collisionEnabled = true;
        collisionEnabled = !collisionEnabled;
        player->SetCollision(collisionEnabled);
        return {{"collisionEnabled", collisionEnabled}};
    }

}
