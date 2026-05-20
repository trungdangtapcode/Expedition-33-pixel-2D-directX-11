// ============================================================
// File: CampfireState.cpp
// Responsibility: Implement the modal campfire hub.
//
// Architecture:
//   CampfireState is a pushed overlay state. It uses service singletons
//   for persistent operations and does not own overworld entities.
//
// Common mistakes:
//   1. Saving directly from CheckpointCampfire -> mixes entity rendering
//      with game-state orchestration.
//   2. Mutating party data without SaveManager -> changes disappear after
//      restarting the game.
//   3. Letting the opening U key fall through -> the menu closes on entry.
// ============================================================
#define NOMINMAX
#include "CampfireState.h"
#include "LineupState.h"
#include "StateManager.h"
#include "../Audio/AudioManager.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/GameProgress.h"
#include "../Systems/PartyManager.h"
#include "../Systems/SaveManager.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

namespace
{
    constexpr float kFlashDuration = 2.0f;
    constexpr float kPanelW = 560.0f;
    constexpr float kPanelH = 420.0f;
    constexpr float kRowH = 42.0f;
}

// ------------------------------------------------------------
// Function: CampfireState
// Purpose:
//   Capture the stable campfire id and one-time training reward.
// Why:
//   The overworld campfire object remains SceneGraph-owned, so this state
//   should only receive immutable data needed for menu actions.
// ------------------------------------------------------------
CampfireState::CampfireState(std::string campfireId, int upgradeExpReward)
    : mCampfireId(std::move(campfireId))
    , mUpgradeExpReward(upgradeExpReward)
{
}

// ------------------------------------------------------------
// Function: OnEnter
// Purpose:
//   Initialize the shared RPG dialog assets used by the campfire menu.
// Why:
//   The state owns the renderers while it is on top of the state stack.
// ------------------------------------------------------------
void CampfireState::OnEnter()
{
    auto& d3d = D3DContext::Get();

    mDialogBox.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        L"assets/UI/ui-dialog-box-hd.png",
        "assets/UI/ui-dialog-box-hd.json",
        d3d.GetWidth(), d3d.GetHeight());

    mTextRenderer.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        L"assets/fonts/arial_16.spritefont",
        d3d.GetWidth(), d3d.GetHeight());

    mCursor = 0;
    mFlashMessage.clear();
    mFlashTimer = 0.0f;
    mElapsed = 0.0f;

    // Start as pressed so the U key that opened the menu is absorbed.
    mUpWasDown = true;
    mDownWasDown = true;
    mEnterWasDown = true;
    mEscWasDown = true;
    mBackWasDown = true;
    mUWasDown = true;

    LOG("[CampfireState] Opened campfire menu for '%s'.", mCampfireId.c_str());
}

// ------------------------------------------------------------
// Function: OnExit
// Purpose:
//   Release GPU-backed UI resources before the state is destroyed.
// Why:
//   SpriteBatch, SpriteFont, and nine-slice textures own DirectX resources
//   that should not outlive their state unnecessarily.
// ------------------------------------------------------------
void CampfireState::OnExit()
{
    LOG("[CampfireState] Closed campfire menu for '%s'.", mCampfireId.c_str());
    mTextRenderer.Shutdown();
    mDialogBox.Shutdown();
}

// ------------------------------------------------------------
// Function: Pressed
// Purpose:
//   Convert GetAsyncKeyState into one-press menu semantics.
// Why:
//   Holding a key should not repeatedly move the cursor or fire actions.
// ------------------------------------------------------------
bool CampfireState::Pressed(int vk, bool& wasDown)
{
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool fresh = down && !wasDown;
    wasDown = down;
    return fresh;
}

// ------------------------------------------------------------
// Function: Flash
// Purpose:
//   Display short action feedback inside the menu.
// Why:
//   Campfire actions are immediate, so the player needs confirmation
//   without leaving the modal hub.
// ------------------------------------------------------------
void CampfireState::Flash(const std::string& message)
{
    mFlashMessage = message;
    mFlashTimer = kFlashDuration;
    LOG("[CampfireState] %s", message.c_str());
}

const char* CampfireState::OptionLabel(MenuOption option)
{
    switch (option)
    {
    case MenuOption::Rest:     return "Rest";
    case MenuOption::Save:     return "Save Checkpoint";
    case MenuOption::Load:     return "Load Checkpoint";
    case MenuOption::Training: return "Upgrade Party";
    case MenuOption::Lineup:   return "Lineup";
    case MenuOption::Exit:     return "Exit";
    default:                   return "";
    }
}

// ------------------------------------------------------------
// Function: ActivateSelection
// Purpose:
//   Execute the currently selected campfire action.
// Why:
//   Keeping action dispatch here keeps Update focused on input edges.
// ------------------------------------------------------------
void CampfireState::ActivateSelection()
{
    const MenuOption option = static_cast<MenuOption>(mCursor);

    switch (option)
    {
    case MenuOption::Rest:
        PartyManager::Get().RestoreFullHP();
        SaveManager::Get().SaveCheckpoint("campfire_rest:" + mCampfireId);
        Flash("Party restored. Checkpoint saved.");
        AudioManager::Get().PlaySfx("ui_confirm");
        break;

    case MenuOption::Save:
        if (SaveManager::Get().SaveCheckpoint("campfire_save:" + mCampfireId))
        {
            Flash("Checkpoint saved.");
            AudioManager::Get().PlaySfx("ui_confirm");
        }
        else
        {
            Flash("Save failed.");
            AudioManager::Get().PlaySfx("battle_no_ap");
        }
        break;

    case MenuOption::Load:
    {
        std::string sceneId;
        if (SaveManager::Get().LoadCheckpoint(&sceneId))
        {
            Flash("Checkpoint loaded.");
            AudioManager::Get().PlaySfx("ui_confirm");
        }
        else
        {
            Flash("No checkpoint found.");
            AudioManager::Get().PlaySfx("battle_no_ap");
        }
        break;
    }

    case MenuOption::Training:
    {
        const std::string flag = "campfire_upgrade:" + mCampfireId;
        PartyManager::Get().RestoreFullHP();

        if (mUpgradeExpReward > 0 && !GameProgress::Get().HasFlag(flag))
        {
            PartyManager::Get().AddExp(mUpgradeExpReward);
            GameProgress::Get().SetFlag(flag);
            SaveManager::Get().SaveCheckpoint("campfire_upgrade:" + mCampfireId);

            char buffer[128]{};
            std::snprintf(buffer, sizeof(buffer), "Party upgraded. +%d EXP.", mUpgradeExpReward);
            Flash(buffer);
            AudioManager::Get().PlaySfx("ui_confirm");
        }
        else
        {
            SaveManager::Get().SaveCheckpoint("campfire_rest:" + mCampfireId);
            Flash("Training already claimed. Party restored.");
            AudioManager::Get().PlaySfx("battle_no_ap");
        }
        break;
    }

    case MenuOption::Lineup:
        AudioManager::Get().PlaySfx("ui_confirm");
        StateManager::Get().PushState(std::make_unique<LineupState>());
        break;

    case MenuOption::Exit:
        AudioManager::Get().PlaySfx("ui_back");
        StateManager::Get().PopState();
        break;

    default:
        break;
    }
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Run campfire menu input and feedback timers.
// Why:
//   While this state is on top, overworld update is paused by the state
//   stack, which gives the menu exclusive control.
// ------------------------------------------------------------
void CampfireState::Update(float dt)
{
    mElapsed += dt;
    if (mFlashTimer > 0.0f)
    {
        mFlashTimer = std::max(0.0f, mFlashTimer - dt);
    }

    if (Pressed(VK_ESCAPE, mEscWasDown) ||
        Pressed(VK_BACK, mBackWasDown) ||
        Pressed('U', mUWasDown))
    {
        AudioManager::Get().PlaySfx("ui_back");
        StateManager::Get().PopState();
        return;
    }

    if (Pressed(VK_UP, mUpWasDown))
    {
        mCursor = (mCursor - 1 + OptionCount()) % OptionCount();
        AudioManager::Get().PlaySfx("ui_navigate");
    }
    if (Pressed(VK_DOWN, mDownWasDown))
    {
        mCursor = (mCursor + 1) % OptionCount();
        AudioManager::Get().PlaySfx("ui_navigate");
    }
    if (Pressed(VK_RETURN, mEnterWasDown))
    {
        ActivateSelection();
    }
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the campfire hub panel and menu text.
// Why:
//   The state is modal, so it renders a compact centered UI surface.
// ------------------------------------------------------------
void CampfireState::Render()
{
    auto& d3d = D3DContext::Get();
    ID3D11DeviceContext* ctx = d3d.GetContext();

    const float screenW = static_cast<float>(d3d.GetWidth());
    const float screenH = static_cast<float>(d3d.GetHeight());
    const float panelX = (screenW - kPanelW) * 0.5f;
    const float panelY = (screenH - kPanelH) * 0.5f;

    const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
    const DirectX::XMVECTOR dim = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.72f);
    const DirectX::XMVECTOR panelColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.96f);

    mDialogBox.Draw(ctx, 0.0f, 0.0f, screenW, screenH, 1.0f, identity, dim);
    mDialogBox.Draw(ctx, panelX, panelY, kPanelW, kPanelH, 1.0f, identity, panelColor);

    mTextRenderer.BeginBatch(ctx);
    mTextRenderer.DrawStringCenteredRaw("Campfire", panelX + kPanelW * 0.5f, panelY + 34.0f,
                                        DirectX::Colors::White, 1.35f, true);
    mTextRenderer.DrawStringCenteredRaw("Rest, save, train, or manage the party.",
                                        panelX + kPanelW * 0.5f, panelY + 76.0f,
                                        DirectX::Colors::LightGray);

    const float listX = panelX + 96.0f;
    const float listY = panelY + 126.0f;
    for (int i = 0; i < OptionCount(); ++i)
    {
        const bool selected = (i == mCursor);
        const float rowY = listY + static_cast<float>(i) * kRowH;

        if (selected)
        {
            const float pulse = 0.86f + 0.14f * std::sin(mElapsed * 7.0f);
            const DirectX::XMVECTOR color = DirectX::XMVectorSet(1.0f, pulse, 0.45f, 1.0f);
            mTextRenderer.DrawStringRaw(">", listX - 34.0f, rowY, color);
            mTextRenderer.DrawStringRaw(OptionLabel(static_cast<MenuOption>(i)), listX, rowY, color);
        }
        else
        {
            mTextRenderer.DrawStringRaw(OptionLabel(static_cast<MenuOption>(i)), listX, rowY,
                                        DirectX::Colors::LightGray);
        }
    }

    if (mFlashTimer > 0.0f && !mFlashMessage.empty())
    {
        mTextRenderer.DrawStringCenteredRaw(mFlashMessage.c_str(),
                                            panelX + kPanelW * 0.5f,
                                            panelY + kPanelH - 68.0f,
                                            DirectX::Colors::PaleGreen);
    }

    mTextRenderer.DrawStringCenteredRaw("Up/Down: choose   Enter: confirm   Esc/U: close",
                                        panelX + kPanelW * 0.5f,
                                        panelY + kPanelH - 34.0f,
                                        DirectX::Colors::Silver);
    mTextRenderer.EndBatch();
}
