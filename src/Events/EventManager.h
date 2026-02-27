#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

// ============================================================
// EventManager - Hệ thống sự kiện toàn cục (Observer Pattern)
//
// PATTERN: Singleton + Observer (Publish/Subscribe)
//
// TẠI SAO CẦN:
//   Khi Boss còn 50% HP, BattleState KHÔNG nên gọi thẳng
//   CutsceneSystem::Play("intro_boss"). Điều đó tạo tight coupling.
//   Thay vào đó:
//     BattleState "phát" (Broadcast) event: "boss_half_health"
//     CutsceneSystem đã "đăng ký" (Subscribe) event này trước đó.
//     EventManager tự động gọi callback của CutsceneSystem.
//   → Hai hệ thống không cần biết về nhau!
//
// CÁCH DÙNG:
//   // Đăng ký lắng nghe event (thường ở OnEnter của State):
//   EventManager::Get().Subscribe("boss_half_health", [this](const EventData& d) {
//       this->TriggerCutscene();
//   });
//
//   // Phát event từ bất kỳ đâu:
//   EventManager::Get().Broadcast("boss_half_health");
//
//   // Hủy đăng ký khi State bị destroy (tránh dangling pointer!):
//   EventManager::Get().Unsubscribe("boss_half_health", listenerID);
// ============================================================

// EventData: struct mang dữ liệu đi kèm event (mở rộng tùy ý)
struct EventData {
    std::string name;        // Tên event
    void*       payload = nullptr; // Dữ liệu tùy ý (cast về struct cụ thể nếu cần)
    float       value   = 0.0f;    // Giá trị số đơn giản (HP%, damage, ...)
};

using EventCallback = std::function<void(const EventData&)>;
using ListenerID    = int; // ID để Unsubscribe

class EventManager {
public:
    static EventManager& Get();

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    // Đăng ký callback lắng nghe một event.
    // Trả về ListenerID để dùng khi Unsubscribe.
    ListenerID Subscribe(const std::string& eventName, EventCallback callback);

    // Hủy đăng ký theo ID (gọi ở OnExit của State để tránh memory leak)
    void Unsubscribe(const std::string& eventName, ListenerID id);

    // Phát event, gọi tất cả callbacks đã đăng ký tên event này
    void Broadcast(const std::string& eventName, const EventData& data = {});

    // Hủy toàn bộ listener của một event (dùng khi reset scene)
    void ClearEvent(const std::string& eventName);

    // Hủy toàn bộ listener (dùng khi restart game)
    void ClearAll();

private:
    EventManager() = default;

    struct Listener {
        ListenerID    id;
        EventCallback callback;
    };

    // Map: tên event → danh sách các listener
    std::unordered_map<std::string, std::vector<Listener>> mListeners;

    // Bộ đếm ID tự tăng (đảm bảo mỗi listener có ID duy nhất)
    ListenerID mNextID = 0;
};
