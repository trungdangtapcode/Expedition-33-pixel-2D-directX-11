// ============================================================
// File: EventManager.h
// Responsibility: Declare the global publish/subscribe event bus.
//
// Owns:
//   Listener tables keyed by event name.
//
// Lifetime:
//   Created in  -> EventManager::Get() on first use.
//   Destroyed in -> Process shutdown.
//
// Important:
//   - Subscribers receive a ListenerID and must unsubscribe during teardown.
//   - Event payloads are caller-owned; callbacks must not store payload pointers
//     unless the payload lifetime is explicitly longer than the broadcast.
//   - EventData::name may carry either the event name or a domain-specific id.
//
// Common mistakes:
//   1. Leaving state-owned callbacks subscribed after OnExit() -> dangling
//      captures when the next broadcast fires.
//   2. Mutating listener vectors while iterating them -> iterator invalidation.
//   3. Treating void* payload as owned memory -> double-free or stale pointer.
// ============================================================
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct EventData
{
    std::string name;
    void* payload = nullptr;
    float value = 0.0f;
};

using EventCallback = std::function<void(const EventData&)>;
using ListenerID = int;

class EventManager
{
public:
    static EventManager& Get();

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    ListenerID Subscribe(const std::string& eventName, EventCallback callback);
    void Unsubscribe(const std::string& eventName, ListenerID id);
    void Broadcast(const std::string& eventName, const EventData& data = {});
    void ClearEvent(const std::string& eventName);
    void ClearAll();

private:
    EventManager() = default;

    struct Listener
    {
        ListenerID id = -1;
        EventCallback callback;
    };

    bool IsListenerStillSubscribed(const std::string& eventName, ListenerID id) const;

    std::unordered_map<std::string, std::vector<Listener>> mListeners;
    ListenerID mNextID = 0;
};
