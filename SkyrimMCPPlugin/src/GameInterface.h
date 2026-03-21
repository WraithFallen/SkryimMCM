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
        json ToggleGodMode();
        json ToggleCollision();
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

        // Quest Intelligence (Phase 4)
        json GetQuestStages(const std::string& formIdHex);
        json GetQuestAliases(const std::string& formIdHex);
        json GetQuestItems();

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

        // World (Phase 3)
        json GetWeather();
        json SetWeather(const std::string& weatherFormIdOrName);
        json ListWeathers();
        json GetCellInfo();
        json GetNearbyObjects(float radius, const std::string& typeFilter);
        json UnlockDoor(const std::string& refFormIdHex);
        json LockDoor(const std::string& refFormIdHex, int lockLevel);
        json GetContainerInventory(const std::string& refFormIdHex);
        json DiscoverAllMapMarkers();

        // Targeting
        json GetCrosshairRef();
        json KillActor(const std::string& refFormIdHex);
        json GetEquippedItems();

        // Character Blueprint (Phase 2)
        json GetKnownSpells();
        json GetKnownShouts();
        json GetSkillLevels();
        json GetPerks();
        json UnlockShout(const std::string& formIdHex);
        json GetCharacterBlueprint();
        json GetAppearance();
        json GetFavorites();

        // Utility
        json IsInCombat();
        json GetLoadOrder();
        json GetModFormIdPrefix(const std::string& modName);
        json SaveGame(const std::string& saveName);

        // Notifications
        json ShowNotification(const std::string& message);

        // Search
        json SearchForms(const std::string& query, const std::string& formTypeFilter, int maxResults);

        // Plugin detection
        json GetLoadedSKSEPlugins();
    }

}
