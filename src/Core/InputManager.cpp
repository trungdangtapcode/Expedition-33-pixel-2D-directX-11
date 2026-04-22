// ============================================================
// File: InputManager.cpp
// ============================================================
#include "InputManager.h"
#include <windows.h>

InputManager& InputManager::Get() {
    static InputManager instance;
    return instance;
}

void InputManager::Update() {
    // Shift current state to previous state
    for (int i = 0; i < 256; ++i) {
        mPreviousState[i] = mCurrentState[i];
    }
    
    // Poll the current state of the standard keyboard
    for (int i = 0; i < 256; ++i) {
        // GetAsyncKeyState returns a short; MSB dictates if currently down
        mCurrentState[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }
}

bool InputManager::IsKeyDown(int virtualKey) const {
    if (virtualKey < 0 || virtualKey > 255) return false;
    return mCurrentState[virtualKey];
}

bool InputManager::IsKeyPressed(int virtualKey) const {
    if (virtualKey < 0 || virtualKey > 255) return false;
    return mCurrentState[virtualKey] && !mPreviousState[virtualKey];
}

bool InputManager::IsKeyReleased(int virtualKey) const {
    if (virtualKey < 0 || virtualKey > 255) return false;
    return !mCurrentState[virtualKey] && mPreviousState[virtualKey];
}
