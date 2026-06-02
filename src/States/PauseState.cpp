// ============================================================
// File: PauseState.cpp
// Responsibility: Implement the overworld pause overlay flow.
//
// Architecture:
//   PauseState is a transparent overlay in StateManager. It updates alone,
//   while the overworld beneath it is rendered but not advanced.
//
// Common mistakes:
//   1. Returning to title by popping once leaves OverworldState alive.
//   2. Allowing equipment access here bypasses campfire-only restrictions.
//   3. Using gameplay dt for fade animation stops time while paused.
// ============================================================
#define NOMINMAX
#include "PauseState.h"
#include "ExpeditionJournalState.h"
#include "MenuState.h"
#include "StateManager.h"
#include "../Audio/AudioManager.h"
#include "../Core/InputManager.h"
#include "../Core/TimeSystem.h"
#include "../Events/EventManager.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/Log.h"
#include <Windows.h>
#include <algorithm>
#include <memory>

namespace
{
    constexpr const char* kLayoutPath = "data/pause_menu_layout.json";

    bool ConfirmPressed(const InputManager& input)
    {
        return input.IsKeyPressed(VK_RETURN) ||
               input.IsKeyPressed(VK_SPACE);
    }

    bool BackPressed(const InputManager& input)
    {
        return input.IsKeyPressed(VK_ESCAPE) ||
               input.IsKeyPressed(VK_BACK);
    }
}

void PauseState::OnEnter()
{
    auto& d3d = D3DContext::Get();
    mRenderer.Initialize(d3d.GetDevice(),
                         d3d.GetContext(),
                         kLayoutPath,
                         d3d.GetWidth(),
                         d3d.GetHeight());

    mResizeListenerId = EventManager::Get().Subscribe("window_resized",
        [this](const EventData&)
        {
            mRenderer.SetScreenSize(D3DContext::Get().GetWidth(),
                                    D3DContext::Get().GetHeight());
        });

    TimeSystem::Get().SetGameplayPaused(true);
    mPhase = Phase::Main;
    mCursor = 0;
    mConfirmCursor = 1;
    mElapsed = 0.0f;
    mFadeTimer = 0.0f;
    mBackInputArmed = false;

    LOG("[PauseState] Pause menu opened.");
}

void PauseState::OnExit()
{
    if (mResizeListenerId != -1)
    {
        EventManager::Get().Unsubscribe("window_resized", mResizeListenerId);
        mResizeListenerId = -1;
    }

    TimeSystem::Get().SetGameplayPaused(false);
    mRenderer.Shutdown();
    LOG("[PauseState] Pause menu closed.");
}

void PauseState::Update(float)
{
    const float uiDt = TimeSystem::Get().GetUIClock().GetDeltaTime();
    mElapsed += uiDt;

    if (IsExiting())
    {
        CompleteExitIfReady(uiDt);
        return;
    }

    const InputManager& input = InputManager::Get();
    const bool backPressed = ConsumeOpeningBackKeys()
        ? false
        : BackPressed(input);

    if (mPhase == Phase::Main)
    {
        if (input.IsKeyPressed(VK_UP) || input.IsKeyPressed('W'))
        {
            MoveMainCursor(-1);
        }
        else if (input.IsKeyPressed(VK_DOWN) || input.IsKeyPressed('S'))
        {
            MoveMainCursor(1);
        }
        else if (ConfirmPressed(input))
        {
            ActivateMainSelection();
        }
        else if (backPressed)
        {
            AudioManager::Get().PlaySfx(mRenderer.GetBackSfxId());
            StateManager::Get().PopState();
        }
        return;
    }

    if (input.IsKeyPressed(VK_LEFT) ||
        input.IsKeyPressed(VK_RIGHT) ||
        input.IsKeyPressed(VK_UP) ||
        input.IsKeyPressed(VK_DOWN) ||
        input.IsKeyPressed('A') ||
        input.IsKeyPressed('D') ||
        input.IsKeyPressed('W') ||
        input.IsKeyPressed('S'))
    {
        ToggleConfirmCursor();
    }
    else if (ConfirmPressed(input))
    {
        ActivateConfirmSelection();
    }
    else if (backPressed)
    {
        CancelConfirmOrResume();
    }
}

void PauseState::Render()
{
    mRenderer.Render(D3DContext::Get().GetContext(), BuildRenderState());
}

void PauseState::MoveMainCursor(int direction)
{
    const int count = MainOptionCount();
    if (count <= 0) return;

    mCursor = (mCursor + direction + count) % count;
    AudioManager::Get().PlaySfx(mRenderer.GetNavigateSfxId());
}

void PauseState::ActivateMainSelection()
{
    AudioManager::Get().PlaySfx(mRenderer.GetConfirmSfxId());

    const MainOption option = static_cast<MainOption>(mCursor);
    if (option == MainOption::Resume)
    {
        StateManager::Get().PopState();
        return;
    }

    if (option == MainOption::ExpeditionJournal)
    {
        StateManager::Get().PushState(std::make_unique<ExpeditionJournalState>());
        return;
    }

    mConfirmCursor = 1;
    if (option == MainOption::ReturnToTitle)
    {
        mPhase = Phase::ConfirmReturnToTitle;
        return;
    }

    if (option == MainOption::QuitGame)
    {
        mPhase = Phase::ConfirmQuit;
    }
}

void PauseState::ToggleConfirmCursor()
{
    mConfirmCursor = 1 - mConfirmCursor;
    AudioManager::Get().PlaySfx(mRenderer.GetNavigateSfxId());
}

void PauseState::ActivateConfirmSelection()
{
    if (mConfirmCursor == 1)
    {
        CancelConfirmOrResume();
        return;
    }

    AudioManager::Get().PlaySfx(mRenderer.GetConfirmSfxId());
    if (mPhase == Phase::ConfirmReturnToTitle)
    {
        BeginExit(Phase::ExitingToTitle);
        return;
    }

    if (mPhase == Phase::ConfirmQuit)
    {
        BeginExit(Phase::ExitingQuit);
    }
}

void PauseState::CancelConfirmOrResume()
{
    AudioManager::Get().PlaySfx(mRenderer.GetBackSfxId());
    mPhase = Phase::Main;
    mConfirmCursor = 1;
}

void PauseState::BeginExit(Phase exitPhase)
{
    mPhase = exitPhase;
    mFadeTimer = 0.0f;
}

void PauseState::CompleteExitIfReady(float uiDt)
{
    mFadeTimer += uiDt;

    const float duration = std::max(0.01f, mRenderer.GetFadeDuration());
    if (mFadeTimer < duration) return;

    if (mPhase == Phase::ExitingToTitle)
    {
        StateManager::Get().ChangeState(std::make_unique<MenuState>());
        return;
    }

    if (mPhase == Phase::ExitingQuit)
    {
        TimeSystem::Get().SetGameplayPaused(false);
        PostQuitMessage(0);
    }
}

bool PauseState::ConsumeOpeningBackKeys()
{
    if (mBackInputArmed) return false;

    const InputManager& input = InputManager::Get();
    if (!input.IsKeyDown(VK_ESCAPE) && !input.IsKeyDown(VK_BACK))
    {
        mBackInputArmed = true;
    }

    return true;
}

PauseMenuRenderState PauseState::BuildRenderState() const
{
    PauseMenuRenderState state{};
    state.options = BuildOptionViews();
    state.cursor = mCursor;
    state.confirmCursor = mConfirmCursor;
    state.elapsed = mElapsed;

    if (mPhase == Phase::ConfirmReturnToTitle)
    {
        state.confirming = true;
        state.confirmMessage = LocalizationManager::Get().Text("pause.confirm_title");
    }
    else if (mPhase == Phase::ConfirmQuit)
    {
        state.confirming = true;
        state.confirmMessage = LocalizationManager::Get().Text("pause.confirm_quit");
    }

    if (IsExiting())
    {
        const float duration = std::max(0.01f, mRenderer.GetFadeDuration());
        state.fadeAlpha = std::min(1.0f, mFadeTimer / duration);
    }

    return state;
}

std::vector<PauseMenuOptionView> PauseState::BuildOptionViews() const
{
    std::vector<PauseMenuOptionView> views;
    views.reserve(MainOptionCount());
    for (int i = 0; i < MainOptionCount(); ++i)
    {
        PauseMenuOptionView view{};
        view.label = MainOptionLabel(static_cast<MainOption>(i));
        views.push_back(view);
    }
    return views;
}

std::string PauseState::MainOptionLabel(MainOption option)
{
    switch (option)
    {
    case MainOption::Resume:
        return LocalizationManager::Get().Text("pause.resume");
    case MainOption::ExpeditionJournal:
        return LocalizationManager::Get().Text("pause.expedition_journal");
    case MainOption::ReturnToTitle:
        return LocalizationManager::Get().Text("pause.return_to_title");
    case MainOption::QuitGame:
        return LocalizationManager::Get().Text("pause.quit_game");
    case MainOption::Count:
        break;
    }

    return std::string();
}

int PauseState::MainOptionCount()
{
    return static_cast<int>(MainOption::Count);
}

bool PauseState::IsExiting() const
{
    return mPhase == Phase::ExitingToTitle || mPhase == Phase::ExitingQuit;
}
