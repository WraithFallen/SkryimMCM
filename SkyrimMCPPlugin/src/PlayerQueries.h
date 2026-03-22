#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    namespace PlayerQueries {
        json GetPlayerInfo();
        json GetInventory();
        json GetGoldCount();
        json GetActiveEffects();
        json GetEquippedItems();
        json GetKnownSpells();
        json GetKnownShouts();
        json GetSkillLevels();
        json GetPerks();
        json GetAppearance();
        json GetFavorites();
        json GetCharacterBlueprint();
        json IsInCombat();
        json GetMagicResistances();
    }

}
