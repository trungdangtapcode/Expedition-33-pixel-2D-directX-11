#include "MenuState.h"
#include "StateManager.h"
#include "PlayState.h"
#include "../Events/EventManager.h"
#include <windows.h>  // GetAsyncKeyState

void MenuState::OnEnter() {
    // TODO: Load font, tạo UI button, nhạc nền menu...
    OutputDebugStringA("[MenuState] OnEnter\n");
}

void MenuState::OnExit() {
    // TODO: Unload tài nguyên của menu...
    OutputDebugStringA("[MenuState] OnExit\n");
}

void MenuState::Update(float dt) {
    // Nhấn ENTER → chuyển sang PlayState
    // TODO: Thay bằng hệ thống Input sau
    if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
        StateManager::Get().ChangeState(std::make_unique<PlayState>());
    }
}

void MenuState::Render() {
    // TODO: Vẽ background, logo, danh sách button bằng SpriteBatch/SpriteFont
    // Hiện tại: màn hình xanh đậm từ D3DContext::BeginFrame() là đủ
}
