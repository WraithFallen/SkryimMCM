#include "InventoryManager.h"
#include "Helpers.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <format>

namespace SkyrimMCP::InventoryManager {

    using json = nlohmann::json;

    json AddItem(const std::string& formIdHex, int count) {
        try {
            auto* form = RE::TESForm::LookupByID(Helpers::ParseFormId(formIdHex));
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
            auto* form = RE::TESForm::LookupByID(Helpers::ParseFormId(formIdHex));
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

    json EquipItem(const std::string& formIdHex) {
        try {
            auto* form = RE::TESForm::LookupByID(Helpers::ParseFormId(formIdHex));
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
            auto* form = RE::TESForm::LookupByID(Helpers::ParseFormId(formIdHex));
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

}
