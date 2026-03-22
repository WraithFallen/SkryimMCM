#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    namespace WorldManager {
        json Teleport(const std::string& cellId);
        json GetWeather();
        json SetWeather(const std::string& weatherFormIdOrName);
        json ListWeathers();
        json GetCellInfo();
        json GetNearbyObjects(float radius, const std::string& typeFilter);
        json UnlockDoor(const std::string& refFormIdHex);
        json LockDoor(const std::string& refFormIdHex, int lockLevel);
        json GetContainerInventory(const std::string& refFormIdHex);
        json DiscoverAllMapMarkers();
        json GetGameTime();
        json SetGameTime(float hours);
        json ToggleGodMode();
        json ToggleCollision();
    }

}
