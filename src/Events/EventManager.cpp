// ============================================================
// File: EventManager.cpp
// Responsibility: Implement the global publish/subscribe event bus.
//
// Architecture:
//   States and systems communicate by event name while optional EventData
//   carries payload, numeric values, or a domain-specific name such as a
//   dialogue script id.
//
// Common mistakes:
//   1. Mutating the live listener array while broadcasting invalidates iterators.
//   2. Overwriting EventData::name destroys domain-specific ids sent by callers.
//   3. Forgetting to unsubscribe state-owned callbacks leaves dangling captures.
// ============================================================
#include "EventManager.h"
#include <algorithm>
#include <utility>

EventManager& EventManager::Get()
{
    static EventManager instance;
    return instance;
}

ListenerID EventManager::Subscribe(const std::string& eventName, EventCallback callback)
{
    ListenerID id = mNextID++;
    mListeners[eventName].push_back({ id, std::move(callback) });
    return id;
}

void EventManager::Unsubscribe(const std::string& eventName, ListenerID id)
{
    auto it = mListeners.find(eventName);
    if (it == mListeners.end()) return;

    auto& listeners = it->second;
    // Remove only the requested listener so other subscribers on the same
    // channel keep receiving future broadcasts.
    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(),
            [id](const Listener& listener)
            {
                return listener.id == id;
            }),
        listeners.end());
}

void EventManager::Broadcast(const std::string& eventName, const EventData& data)
{
    auto it = mListeners.find(eventName);
    if (it == mListeners.end()) return;

    // Copy listeners before invoking callbacks because callbacks can subscribe
    // or unsubscribe from the same channel.
    auto listenersCopy = it->second;
    EventData eventData = data;
    if (eventData.name.empty())
    {
        eventData.name = eventName;
    }

    for (const auto& listener : listenersCopy)
    {
        listener.callback(eventData);
    }
}

void EventManager::ClearEvent(const std::string& eventName)
{
    mListeners.erase(eventName);
}

void EventManager::ClearAll()
{
    mListeners.clear();
}
