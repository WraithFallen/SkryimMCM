#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    namespace NPCManager {
        json GetNearbyNPCs(float radius);
        json GetActorInfo(const std::string& formIdHex);
        json GetNPCDetailedInfo(const std::string& refFormIdHex);
        json GetNPCInventory(const std::string& refFormIdHex);
        json GetFollowers();
        json GetDetectionLevel(const std::string& refFormIdHex);
        json MoveActorTo(const std::string& actorFormIdHex, const std::string& targetCellOrRefId);
        json SetRelationshipRank(const std::string& actorFormIdHex, int rank);
        json KillActor(const std::string& refFormIdHex);
        json GetCrosshairRef();
        json SetActorValue(const std::string& attribute, float value);
        json SetActorValueOn(const std::string& actorFormIdHex, const std::string& attribute, float value);
        json GetPlayerFactions();
    }

}
