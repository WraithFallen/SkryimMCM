#include "UIManager.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace SkyrimMCP::UIManager {

    using json = nlohmann::json;

    json GetMenuState() {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return {{"error", "UI not available"}};

        json result;
        result["gameIsPaused"] = ui->GameIsPaused();

        // Check common menus
        static const char* menus[] = {
            "Console", "InventoryMenu", "MagicMenu", "MapMenu",
            "StatsMenu", "ContainerMenu", "DialogueMenu", "BarterMenu",
            "FavoritesMenu", "Journal Menu", "Book Menu", "LockpickingMenu",
            "Loading Menu", "MessageBoxMenu", "TweenMenu", "Sleep/Wait Menu",
            "Crafting Menu", "Training Menu", "RaceSex Menu"
        };

        json openMenus = json::array();
        for (auto* menu : menus) {
            if (ui->IsMenuOpen(menu)) {
                openMenus.push_back(menu);
            }
        }
        result["openMenus"] = openMenus;
        result["menuCount"] = openMenus.size();

        return result;
    }

}
