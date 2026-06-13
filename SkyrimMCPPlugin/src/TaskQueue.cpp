#include "TaskQueue.h"

#include <SKSE/SKSE.h>

#include <atomic>
#include <chrono>
#include <stdexcept>

namespace SkyrimMCP {

    json TaskQueue::RunOnGameThread(std::function<json()> work, int timeoutMs) {
        auto promise = std::make_shared<std::promise<json>>();
        auto future = promise->get_future();

        // Cancellation token: shared with the queued task and checked at task start.
        // Without it, a task that "fails" with a timeout (e.g. the caller gives up
        // during a loading screen, when the game thread is not pumping) would still
        // execute its work() later when the queue resumes — and the client's retry
        // would then run the SAME mutation twice. Setting this flag on timeout makes
        // the not-yet-started task skip its work entirely.
        auto cancelled = std::make_shared<std::atomic<bool>>(false);

        // AddTask accepts std::function<void()> directly
        SKSE::GetTaskInterface()->AddTask([promise, work = std::move(work), cancelled]() {
            if (cancelled->load(std::memory_order_acquire)) {
                // Caller already timed out and abandoned the future — do not run the work.
                return;
            }
            try {
                promise->set_value(work());
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
        if (status == std::future_status::timeout) {
            cancelled->store(true, std::memory_order_release);
            // GUARANTEE / LIMITATION: cancellation only prevents work that has NOT
            // YET STARTED when the timeout fires (the dominant case — a loading
            // screen where the game thread isn't pumping at all — is fully
            // prevented). Work already in-flight on the game thread runs to
            // completion regardless. So a timeout means "maybe ran", NOT
            // "definitely did not run": callers/clients must treat a timed-out
            // WRITE as possibly-applied and must not blindly retry it.
            throw std::runtime_error("Game thread task timed out (work cancelled if not yet started) - game may be loading or paused");
        }

        return future.get();
    }

}
