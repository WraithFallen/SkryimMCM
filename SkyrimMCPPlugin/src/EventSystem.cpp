#include "EventSystem.h"
#include "Helpers.h"

#include <format>
#include <unordered_set>

namespace SkyrimMCP {

    EventSystem& EventSystem::GetSingleton() {
        static EventSystem instance;
        return instance;
    }

    void EventSystem::Register() {
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) {
            SKSE::log::error("Failed to get ScriptEventSourceHolder");
            return;
        }

        holder->AddEventSink<RE::TESCombatEvent>(this);
        holder->AddEventSink<RE::TESDeathEvent>(this);
        holder->AddEventSink<RE::TESQuestStageEvent>(this);
        holder->AddEventSink<RE::TESQuestStartStopEvent>(this);
        holder->AddEventSink<RE::TESActorLocationChangeEvent>(this);
        holder->AddEventSink<RE::TESCellFullyLoadedEvent>(this);
        holder->AddEventSink<RE::TESContainerChangedEvent>(this);
        holder->AddEventSink<RE::TESEquipEvent>(this);
        holder->AddEventSink<RE::TESHitEvent>(this);
        holder->AddEventSink<RE::TESSpellCastEvent>(this);
        holder->AddEventSink<RE::TESActivateEvent>(this);
        holder->AddEventSink<RE::TESOpenCloseEvent>(this);
        holder->AddEventSink<RE::TESSleepStopEvent>(this);
        holder->AddEventSink<RE::TESWaitStopEvent>(this);
        holder->AddEventSink<RE::TESLockChangedEvent>(this);
        holder->AddEventSink<RE::TESTrackedStatsEvent>(this);

        try { holder->AddEventSink<RE::TESFastTravelEndEvent>(this); } catch (...) {}

        SKSE::log::info("Event system registered for 17 event types");
    }

    void EventSystem::Unregister() {
        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) return;

        holder->RemoveEventSink<RE::TESCombatEvent>(this);
        holder->RemoveEventSink<RE::TESDeathEvent>(this);
        holder->RemoveEventSink<RE::TESQuestStageEvent>(this);
        holder->RemoveEventSink<RE::TESQuestStartStopEvent>(this);
        holder->RemoveEventSink<RE::TESActorLocationChangeEvent>(this);
        holder->RemoveEventSink<RE::TESCellFullyLoadedEvent>(this);
        holder->RemoveEventSink<RE::TESContainerChangedEvent>(this);
        holder->RemoveEventSink<RE::TESEquipEvent>(this);
        holder->RemoveEventSink<RE::TESHitEvent>(this);
        holder->RemoveEventSink<RE::TESSpellCastEvent>(this);
        holder->RemoveEventSink<RE::TESActivateEvent>(this);
        holder->RemoveEventSink<RE::TESOpenCloseEvent>(this);
        holder->RemoveEventSink<RE::TESSleepStopEvent>(this);
        holder->RemoveEventSink<RE::TESWaitStopEvent>(this);
        holder->RemoveEventSink<RE::TESLockChangedEvent>(this);
        holder->RemoveEventSink<RE::TESTrackedStatsEvent>(this);
        try { holder->RemoveEventSink<RE::TESFastTravelEndEvent>(this); } catch (...) {}
    }

    // Known framework quests that fire events constantly
    static const std::unordered_set<std::string> SUPPRESSED_QUEST_FORMIDS = {
        "FEBD3801",  // NFF Quest Alias NPC — cycles start/stop dozens of times
        "000F9075",  // Framework quest — cycles start/stop and stage 0 constantly
        "F30C44FC",  // Follower Object Awareness — cycles start/stop repeatedly
    };

    // Event types that are always muted (too noisy to report individually)
    static const std::unordered_set<std::string> MUTED_EVENT_TYPES = {
        "cell_loaded",  // 50+ events per fast travel as worldspace streams in
    };

    static bool IsMutedEvent(const std::string& eventType, const json& data) {
        if (MUTED_EVENT_TYPES.count(eventType)) return true;
        if (eventType == "quest_start_stop" || eventType == "quest_stage") {
            auto formId = data.value("questFormId", "");
            if (SUPPRESSED_QUEST_FORMIDS.count(formId)) return true;
        }
        return false;
    }

    void EventSystem::PushEvent(const std::string& eventType, json data) {
        bool muted = IsMutedEvent(eventType, data);

        std::lock_guard lock(_mutex);

        // Track muted event counts
        if (muted) {
            std::string key = data.value("questFormId", eventType);
            _mutedCounts[key]++;
            return;  // Don't add to main queue
        }

        if (_eventQueue.size() >= MAX_QUEUE_SIZE) {
            _eventQueue.pop();
        }
        json event;
        event["type"] = "event";
        event["event"] = eventType;
        event["data"] = data;
        _eventQueue.push(event);
    }

    std::vector<json> EventSystem::DrainEvents() {
        std::lock_guard lock(_mutex);
        std::vector<json> events;
        while (!_eventQueue.empty()) {
            events.push_back(_eventQueue.front());
            _eventQueue.pop();
        }
        return events;
    }

    json EventSystem::GetMutedSummary() {
        std::lock_guard lock(_mutex);
        json summary = json::object();
        int total = 0;
        for (auto& [key, count] : _mutedCounts) {
            summary[key] = count;
            total += count;
        }
        _mutedCounts.clear();
        return {{"muted", summary}, {"mutedTotal", total}};
    }

    bool EventSystem::HasEvents() const {
        std::lock_guard lock(_mutex);
        return !_eventQueue.empty();
    }

    // Use shared helpers from Helpers.h
    using Helpers::GetRefName;
    using Helpers::GetFormName;

    // ==================== Event Handlers ====================

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESCombatEvent* event, RE::BSTEventSource<RE::TESCombatEvent>*) {
        if (!event || !event->actor) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (event->actor.get() != player) return RE::BSEventNotifyControl::kContinue;

        std::string state;
        switch (event->newState.get()) {
            case RE::ACTOR_COMBAT_STATE::kNone: state = "ended"; break;
            case RE::ACTOR_COMBAT_STATE::kCombat: state = "started"; break;
            case RE::ACTOR_COMBAT_STATE::kSearching: state = "searching"; break;
        }
        PushEvent("combat", {{"state", state}, {"target", event->targetActor ? GetRefName(event->targetActor.get()) : "none"}});
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*) {
        if (!event || !event->actorDying) return RE::BSEventNotifyControl::kContinue;
        PushEvent("death", {
            {"actor", GetRefName(event->actorDying.get())},
            {"actorFormId", std::format("{:08X}", event->actorDying->GetFormID())},
            {"killer", event->actorKiller ? GetRefName(event->actorKiller.get()) : "none"},
            {"dead", event->dead}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESQuestStageEvent* event, RE::BSTEventSource<RE::TESQuestStageEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        PushEvent("quest_stage", {
            {"questFormId", std::format("{:08X}", event->formID)},
            {"questName", GetFormName(event->formID)},
            {"stage", event->stage}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESQuestStartStopEvent* event, RE::BSTEventSource<RE::TESQuestStartStopEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        PushEvent("quest_start_stop", {
            {"questFormId", std::format("{:08X}", event->formID)},
            {"questName", GetFormName(event->formID)},
            {"started", event->started}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESActorLocationChangeEvent* event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) {
        if (!event || !event->actor) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (event->actor.get() != player) return RE::BSEventNotifyControl::kContinue;

        PushEvent("location_change", {
            {"oldLocation", event->oldLoc ? event->oldLoc->GetName() : "none"},
            {"newLocation", event->newLoc ? event->newLoc->GetName() : "none"}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESCellFullyLoadedEvent* event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) {
        if (!event || !event->cell) return RE::BSEventNotifyControl::kContinue;
        PushEvent("cell_loaded", {
            {"cellName", event->cell->GetName() ? event->cell->GetName() : "unnamed"},
            {"cellFormId", std::format("{:08X}", event->cell->GetFormID())}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESContainerChangedEvent* event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        if (event->newContainer != 0x14 && event->oldContainer != 0x14) return RE::BSEventNotifyControl::kContinue;

        PushEvent("inventory_change", {
            {"item", GetFormName(event->baseObj)},
            {"itemFormId", std::format("{:08X}", event->baseObj)},
            {"count", event->itemCount},
            {"gained", event->newContainer == 0x14}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) {
        if (!event || !event->actor) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (event->actor.get() != player) return RE::BSEventNotifyControl::kContinue;

        PushEvent("equip", {
            {"item", GetFormName(event->baseObject)},
            {"itemFormId", std::format("{:08X}", event->baseObject)},
            {"equipped", event->equipped}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*) {
        if (!event || !event->target) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();

        auto* target = event->target.get();
        auto* attacker = event->cause ? event->cause.get() : nullptr;

        // Determine relationship to player for clarity
        bool targetIsPlayer = (target == player);
        bool attackerIsPlayer = (attacker == player);

        // Check if target or attacker is a teammate/follower
        bool targetIsTeammate = false;
        bool attackerIsTeammate = false;
        if (target && !targetIsPlayer) {
            auto* targetActor = target->As<RE::Actor>();
            if (targetActor) targetIsTeammate = targetActor->IsPlayerTeammate();
        }
        if (attacker && !attackerIsPlayer) {
            auto* attackerActor = attacker->As<RE::Actor>();
            if (attackerActor) attackerIsTeammate = attackerActor->IsPlayerTeammate();
        }

        // Only track hits involving the player or their followers
        if (!targetIsPlayer && !attackerIsPlayer && !targetIsTeammate && !attackerIsTeammate)
            return RE::BSEventNotifyControl::kContinue;

        // Build a clear description of who hit whom
        std::string targetRole = targetIsPlayer ? "player" : (targetIsTeammate ? "follower" : "other");
        std::string attackerRole = attackerIsPlayer ? "player" : (attackerIsTeammate ? "follower" : "other");

        PushEvent("hit", {
            {"targetName", GetRefName(target)},
            {"targetRefId", std::format("{:08X}", target->GetFormID())},
            {"targetRole", targetRole},
            {"attackerName", attacker ? GetRefName(attacker) : "none"},
            {"attackerRefId", attacker ? std::format("{:08X}", attacker->GetFormID()) : ""},
            {"attackerRole", attackerRole},
            {"weaponFormId", std::format("{:08X}", event->source)},
            {"weaponName", GetFormName(event->source)}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESSpellCastEvent* event, RE::BSTEventSource<RE::TESSpellCastEvent>*) {
        if (!event || !event->object) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (event->object.get() != player) return RE::BSEventNotifyControl::kContinue;

        PushEvent("spell_cast", {
            {"spell", GetFormName(event->spell)},
            {"spellFormId", std::format("{:08X}", event->spell)}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESActivateEvent* event, RE::BSTEventSource<RE::TESActivateEvent>*) {
        if (!event || !event->actionRef) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (event->actionRef.get() != player) return RE::BSEventNotifyControl::kContinue;

        PushEvent("activate", {
            {"object", event->objectActivated ? GetRefName(event->objectActivated.get()) : "unknown"},
            {"objectFormId", event->objectActivated ? std::format("{:08X}", event->objectActivated->GetFormID()) : ""}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESOpenCloseEvent* event, RE::BSTEventSource<RE::TESOpenCloseEvent>*) {
        if (!event || !event->ref) return RE::BSEventNotifyControl::kContinue;
        PushEvent("open_close", {
            {"object", GetRefName(event->ref.get())},
            {"opened", event->opened}
        });
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESSleepStopEvent* event, RE::BSTEventSource<RE::TESSleepStopEvent>*) {
        PushEvent("sleep_ended", {{"interrupted", event ? event->interrupted : false}});
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESWaitStopEvent* event, RE::BSTEventSource<RE::TESWaitStopEvent>*) {
        PushEvent("wait_ended", {{"interrupted", event ? event->interrupted : false}});
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESFastTravelEndEvent* event, RE::BSTEventSource<RE::TESFastTravelEndEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        PushEvent("fast_travel", {{"travelTimeHours", event->fastTravelEndHours}});
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESLockChangedEvent* event, RE::BSTEventSource<RE::TESLockChangedEvent>*) {
        if (!event || !event->lockedObject) return RE::BSEventNotifyControl::kContinue;
        PushEvent("lock_changed", {{"object", GetRefName(event->lockedObject.get())}});
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventSystem::ProcessEvent(const RE::TESTrackedStatsEvent* event, RE::BSTEventSource<RE::TESTrackedStatsEvent>*) {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        PushEvent("stat_change", {{"stat", event->stat.c_str()}, {"value", event->value}});
        return RE::BSEventNotifyControl::kContinue;
    }

}
