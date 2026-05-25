// ============================================================
// File: DialogueState.h
// Responsibility: Run a linear story dialogue overlay.
//
// Lifetime:
//   Pushed in  -> OverworldState when the player talks to an NPC.
//   Popped via -> Dialogue completion or optional back cancellation.
//
// Important:
//   - Renders below gameplay through ShouldRenderBelow().
//   - Loads dialogue scripts through DialogueManager.
//   - Persists completion through GameProgress flags.
// ============================================================
#pragma once

#include "IGameState.h"
#include "../Systems/DialogueManager.h"
#include "../UI/DialogueRenderer.h"
#include <string>

class DialogueState : public IGameState
{
public:
    explicit DialogueState(std::string scriptPath);

    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    bool ShouldRenderBelow() const override { return true; }
    const char* GetName() const override { return "DialogueState"; }

private:
    void AdvanceOrReveal();
    void CompleteDialogue();
    DialogueRenderState BuildRenderState() const;
    bool CurrentLineComplete() const;
    std::size_t CurrentLineGlyphCount() const;

    std::string mScriptPath;
    DialogueScript mScript;
    DialogueRenderer mRenderer;
    int mLineIndex = 0;
    float mVisibleGlyphs = 0.0f;
    float mElapsed = 0.0f;
    int mResizeListenerId = -1;
    bool mReady = false;
    bool mPendingClose = false;
};
