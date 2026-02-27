#include "PlayState.h"
#include "StateManager.h"
#include "MenuState.h"
#include "../Renderer/D3DContext.h"
#include "../Events/EventManager.h"
#include <windows.h>

void PlayState::OnEnter() {
    OutputDebugStringA("[PlayState] OnEnter\n");
    // TODO: Load level data, khởi tạo EntityManager, spawn player...

    // Ví dụ đăng ký lắng nghe event từ BattleSystem sau này:
    // EventManager::Get().Subscribe("boss_half_health", [this](const EventData& d) {
    //     // Push CutsceneState lên trên PlayState
    //     StateManager::Get().PushState(std::make_unique<CutsceneState>("cutscene_01"));
    // });
}

void PlayState::OnExit() {
    OutputDebugStringA("[PlayState] OnExit\n");
    // TODO: Hủy đăng ký event, save checkpoint...
    // EventManager::Get().Unsubscribe("boss_half_health", mBossListenerID);
}

void PlayState::Update(float dt) {
    // Test: nhấn ESC → quay về MenuState
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        StateManager::Get().ChangeState(std::make_unique<MenuState>());
        return;
    }

    mRotation += dt; // Biến tạm để test deltaTime hoạt động đúng

    // TODO:
    // entityManager.UpdateAll(dt);
    // turnManager.Update(dt);
    // cameraSystem.Update(dt);
}

void PlayState::Render() {
    // TODO: Vẽ entities, terrain, UI...
    // Hiện tại D3DContext::BeginFrame() đã xóa màn hình,
    // mỗi State sẽ thêm màu nền khác nhau trong tương lai.
}
