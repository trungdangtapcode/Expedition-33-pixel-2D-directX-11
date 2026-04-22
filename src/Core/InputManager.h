// ============================================================
// File: InputManager.h
// Responsibility: A centralized singleton system to track 
//   keyboard input state (down, pressed this frame, released this frame).
// ============================================================
#pragma once

class InputManager {
public:
    static InputManager& Get();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Called once per frame at the start of the game loop
    void Update();

    // Returns true if the key is currently held down
    bool IsKeyDown(int virtualKey) const;

    // Returns true only on the exact frame the key is first pressed
    bool IsKeyPressed(int virtualKey) const;

    // Returns true only on the exact frame the key is released
    bool IsKeyReleased(int virtualKey) const;

private:
    InputManager() = default;

    // We track 256 virtual key codes max.
    bool mCurrentState[256] = { false };
    bool mPreviousState[256] = { false };
};
