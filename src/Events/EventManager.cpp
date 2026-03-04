#include "EventManager.h"
#include <algorithm>

EventManager& EventManager::Get() {
    static EventManager instance;
    return instance;
}

ListenerID EventManager::Subscribe(const std::string& eventName, EventCallback callback) {
    ListenerID id = mNextID++;
    mListeners[eventName].push_back({ id, std::move(callback) });
    return id;
}

void EventManager::Unsubscribe(const std::string& eventName, ListenerID id) {
    auto it = mListeners.find(eventName);
    if (it == mListeners.end()) return;

    auto& listeners = it->second;
    // Xóa listener có ID tương ứng
    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(),
            [id](const Listener& l) { return l.id == id; }),
        listeners.end()
    );
}

void EventManager::Broadcast(const std::string& eventName, const EventData& data) {
    auto it = mListeners.find(eventName);
    if (it == mListeners.end()) return;

    // Tạo bản sao danh sách listener trước khi gọi
    // (phòng trường hợp callback tự Unsubscribe hoặc Subscribe thêm event mới
    //  gây iterator invalidation)
    auto listenersCopy = it->second;
    EventData eventData = data;
    eventData.name = eventName;

    for (const auto& listener : listenersCopy) {
        listener.callback(eventData);
    }
}

void EventManager::ClearEvent(const std::string& eventName) {
    mListeners.erase(eventName);
}

void EventManager::ClearAll() {
    mListeners.clear();
}
