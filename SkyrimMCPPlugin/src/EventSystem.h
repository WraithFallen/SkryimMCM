#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <nlohmann/json.hpp>
#include <mutex>
#include <queue>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    // Collects game events and queues them for push to the pipe client.
    class EventSystem :
        public RE::BSTEventSink<RE::TESCombatEvent>,
        public RE::BSTEventSink<RE::TESDeathEvent>,
        public RE::BSTEventSink<RE::TESQuestStageEvent>,
        public RE::BSTEventSink<RE::TESQuestStartStopEvent>,
        public RE::BSTEventSink<RE::TESActorLocationChangeEvent>,
        public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
        public RE::BSTEventSink<RE::TESContainerChangedEvent>,
        public RE::BSTEventSink<RE::TESEquipEvent>,
        public RE::BSTEventSink<RE::TESHitEvent>,
        public RE::BSTEventSink<RE::TESSpellCastEvent>,
        public RE::BSTEventSink<RE::TESActivateEvent>,
        public RE::BSTEventSink<RE::TESOpenCloseEvent>,
        public RE::BSTEventSink<RE::TESSleepStopEvent>,
        public RE::BSTEventSink<RE::TESWaitStopEvent>,
        public RE::BSTEventSink<RE::TESFastTravelEndEvent>,
        public RE::BSTEventSink<RE::TESLockChangedEvent>,
        public RE::BSTEventSink<RE::TESTrackedStatsEvent>
    {
    public:
        static EventSystem& GetSingleton();

        void Register();
        void Unregister();

        std::vector<json> DrainEvents();
        json GetMutedSummary();  // Returns counts of suppressed events
        bool HasEvents() const;

    protected:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* event, RE::BSTEventSource<RE::TESCombatEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStageEvent* event, RE::BSTEventSource<RE::TESQuestStageEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStartStopEvent* event, RE::BSTEventSource<RE::TESQuestStartStopEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event, RE::BSTEventSource<RE::TESActivateEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESOpenCloseEvent* event, RE::BSTEventSource<RE::TESOpenCloseEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent* event, RE::BSTEventSource<RE::TESSleepStopEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESWaitStopEvent* event, RE::BSTEventSource<RE::TESWaitStopEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESFastTravelEndEvent* event, RE::BSTEventSource<RE::TESFastTravelEndEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESLockChangedEvent* event, RE::BSTEventSource<RE::TESLockChangedEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESTrackedStatsEvent* event, RE::BSTEventSource<RE::TESTrackedStatsEvent>*) override;

    private:
        EventSystem() = default;
        void PushEvent(const std::string& eventType, json data);

        mutable std::mutex _mutex;
        std::queue<json> _eventQueue;
        std::unordered_map<std::string, int> _mutedCounts;
        static constexpr size_t MAX_QUEUE_SIZE = 500;
    };

}
