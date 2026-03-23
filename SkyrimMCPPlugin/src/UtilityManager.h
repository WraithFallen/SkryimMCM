#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    namespace UtilityManager {
        json ShowNotification(const std::string& message);
        json SearchForms(const std::string& query, const std::string& formTypeFilter, int maxResults);
        json GetLoadedSKSEPlugins();
        json SaveGame(const std::string& saveName);
        json GetLoadOrder();
        json GetModFormIdPrefix(const std::string& modName);
        json LoadSave(const std::string& saveName);
        json LoadMostRecentSave();
    }

}
