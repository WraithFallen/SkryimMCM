#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    namespace CombatAnalysis {
        json GetCombatState();
        json GetDamageStats();
        json GetThreats(float radius);
    }

}
