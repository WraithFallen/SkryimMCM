#include "TaskQueue.h"

#include <SKSE/SKSE.h>

#include <chrono>
#include <stdexcept>

namespace SkyrimMCP {

    json TaskQueue::RunOnGameThread(std::function<json()> work, int timeoutMs) {
        auto promise = std::make_shared<std::promise<json>>();
        auto future = promise->get_future();

        // AddTask accepts std::function<void()> directly
        SKSE::GetTaskInterface()->AddTask([promise, work = std::move(work)]() {
            try {
                promise->set_value(work());
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
        if (status == std::future_status::timeout) {
            throw std::runtime_error("Game thread task timed out - game may be loading or paused");
        }

        return future.get();
    }

}
