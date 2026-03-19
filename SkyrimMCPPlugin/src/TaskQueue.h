#pragma once

#include <functional>
#include <future>

#include <nlohmann/json.hpp>

namespace SkyrimMCP {

    using json = nlohmann::json;

    // Dispatches work to the game's main thread via SKSE task interface
    // and returns results via std::future
    class TaskQueue {
    public:
        // Submit work to the game thread. Blocks the calling thread until complete.
        // Throws on timeout (default 5 seconds).
        static json RunOnGameThread(std::function<json()> work, int timeoutMs = 5000);
    };

}
