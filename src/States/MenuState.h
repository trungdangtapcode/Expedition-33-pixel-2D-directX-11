// ============================================================
// File: MenuState.h
// Responsibility: Entry state that routes title-menu commands to gameplay.
//
// Lifetime:
//   Pushed by GameApp after DirectX and audio are initialized.
//   Replaced by OverworldState when the player starts or loads a game.
//
// Save/load:
//   MenuState does not serialize data itself. It delegates save-slot work
//   to SaveManager and only owns the high-level state transition decision.
// ============================================================
#pragma once

#include "IGameState.h"
#include "../UI/TitleMenuRenderer.h"
#include <string>
#include <vector>

class MenuState : public IGameState
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    const char* GetName() const override { return "MenuState"; }

private:
    enum class Phase
    {
        PressStart,
        MainOptions,
        LoadSlots
    };

    enum class MainOption
    {
        NewGame,
        Continue,
        LoadSlot,
        Quit,
        Count
    };

    bool Pressed(int vk, bool& wasDown);
    bool AnyStartPressed();
    void CaptureInputLatches();
    bool IsMainOptionEnabled(MainOption option) const;
    void MoveMainCursor(int direction);
    void MoveSlotCursor(int direction);
    void ActivateMainSelection();
    void ActivateSlotSelection();
    void StartNewGame();
    void ContinueFirstSlot();
    bool LoadSlot(int slotIndex);
    void Flash(const std::string& message);
    TitleMenuRenderState BuildRenderState() const;
    std::vector<TitleMenuOptionView> BuildOptionViews() const;
    std::vector<TitleMenuSlotView> BuildSlotViews() const;
    static const char* MainOptionLabel(MainOption option);
    static int MainOptionCount();

    TitleMenuRenderer mRenderer;
    Phase mPhase = Phase::PressStart;
    int mCursor = 0;
    int mSlotCursor = 0;
    float mElapsed = 0.0f;
    float mFlashTimer = 0.0f;
    std::string mFlashMessage;

    bool mUpWasDown = false;
    bool mDownWasDown = false;
    bool mEnterWasDown = false;
    bool mBackWasDown = false;
    bool mEscapeWasDown = false;
    bool mAnyStartWasDown = false;
};
