// ============================================================
// File: PauseState.h
// Responsibility: Overworld-only pause menu overlay.
//
// Lifetime:
//   Pushed in  -> OverworldState when ESC is pressed while idle.
//   Popped via -> Resume, ESC, or Backspace.
//
// Important:
//   - This state renders over the preserved overworld state.
//   - It never exposes inventory, lineup, save, load, or equipment changes.
//   - Return to title is a full stack replacement through StateManager.
// ============================================================
#pragma once

#include "IGameState.h"
#include "../UI/PauseMenuRenderer.h"
#include <string>
#include <vector>

class PauseState : public IGameState
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    bool ShouldRenderBelow() const override { return true; }
    const char* GetName() const override { return "PauseState"; }

private:
    enum class Phase
    {
        Main,
        ConfirmReturnToTitle,
        ConfirmQuit,
        ExitingToTitle,
        ExitingQuit
    };

    enum class MainOption
    {
        Resume,
        ExpeditionJournal,
        ReturnToTitle,
        QuitGame,
        Count
    };

    void MoveMainCursor(int direction);
    void ActivateMainSelection();
    void ToggleConfirmCursor();
    void ActivateConfirmSelection();
    void CancelConfirmOrResume();
    void BeginExit(Phase exitPhase);
    void CompleteExitIfReady(float uiDt);
    bool ConsumeOpeningBackKeys();
    PauseMenuRenderState BuildRenderState() const;
    std::vector<PauseMenuOptionView> BuildOptionViews() const;
    static std::string MainOptionLabel(MainOption option);
    static int MainOptionCount();
    bool IsExiting() const;

    PauseMenuRenderer mRenderer;
    Phase mPhase = Phase::Main;
    int mCursor = 0;
    int mConfirmCursor = 1;
    float mElapsed = 0.0f;
    float mFadeTimer = 0.0f;
    bool mBackInputArmed = false;
    int mResizeListenerId = -1;
};
