#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>

#include "Helpers.h"
#include "PipeServer.h"
#include "EventSystem.h"

namespace {
    std::unique_ptr<SkyrimMCP::PipeServer> g_pipeServer;

    void MessageCallback(SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                SKSE::log::info("Game data loaded, starting systems...");

                // Install console output hook (late — after engine is initialized)
                try {
                    SkyrimMCP::Helpers::InstallConsoleHook();
                } catch (...) {
                    SKSE::log::warn("Console output hook failed to install — output capture disabled");
                }

                SkyrimMCP::EventSystem::GetSingleton().Register();
                SKSE::log::info("Event system registered");
                g_pipeServer = std::make_unique<SkyrimMCP::PipeServer>();
                g_pipeServer->Start();
                SKSE::log::info("Pipe server started on \\\\.\\pipe\\SkyrimMCP");
                break;

            case SKSE::MessagingInterface::kPreLoadGame:
                SKSE::log::info("Game loading...");
                break;

            case SKSE::MessagingInterface::kPostLoadGame:
                SKSE::log::info("Game loaded");
                break;
        }
    }

    void InitializeLogging() {
        auto path = SKSE::log::log_directory();
        if (!path) {
            SKSE::stl::report_and_fail("Failed to find SKSE log directory");
        }
        *path /= "SkyrimMCPPlugin.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
    }
}

SKSEPluginInfo(
    .Version = { 1, 0, 0, 0 },
    .Name = "SkyrimMCPPlugin",
    .Author = "SkyrimMCP",
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    InitializeLogging();
    SKSE::log::info("SkyrimMCPPlugin v1.0.0 loading...");

    SKSE::Init(a_skse);

    // VPrint hook is installed later in kDataLoaded callback

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageCallback)) {
        SKSE::log::error("Failed to register messaging listener");
        return false;
    }

    SKSE::log::info("SkyrimMCPPlugin loaded successfully");
    return true;
}
