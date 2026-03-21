#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    // All functions MUST be called on the game thread (via TaskQueue)
    namespace GameInterface {
        // Player state
        json GetPlayerInfo();
        json GetInventory();
        json GetGoldCount();
        json GetActiveEffects();

        // World interaction
        json ExecuteConsoleCommand(const std::string& command);
        json AddItem(const std::string& formIdHex, int count);
        json RemoveItem(const std::string& formIdHex, int count);
        json SetActorValue(const std::string& attribute, float value);
        json Teleport(const std::string& cellId);
        json GetNearbyNPCs(float radius);

        // Quest management
        json GetQuestInfo();
        json GetQuestStage(const std::string& formIdHex);
        json SetQuestStage(const std::string& formIdHex, int stage);
        json CompleteQuest(const std::string& formIdHex);
        json StartQuest(const std::string& formIdHex);
        json StopQuest(const std::string& formIdHex);

        // Spells / perks / effects
        json AddSpell(const std::string& formIdHex);
        json RemoveSpell(const std::string& formIdHex);
        json AddPerk(const std::string& formIdHex);
        json RemovePerk(const std::string& formIdHex);

        // NPC interaction
        json GetActorInfo(const std::string& formIdHex);
        json SetActorValueOn(const std::string& actorFormIdHex, const std::string& attribute, float value);
        json MoveActorTo(const std::string& actorFormIdHex, const std::string& targetCellOrRefId);
        json SetRelationshipRank(const std::string& actorFormIdHex, int rank);

        // Equipment
        json EquipItem(const std::string& formIdHex);
        json UnequipItem(const std::string& formIdHex);

        // World state
        json GetGameTime();
        json SetGameTime(float hours);

        // Utility
        json IsInCombat();
        json GetLoadOrder();
        json GetModFormIdPrefix(const std::string& modName);
        json SaveGame(const std::string& saveName);
    }

}
