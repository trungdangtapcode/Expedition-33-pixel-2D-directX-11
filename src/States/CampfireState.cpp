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
//   4. Loading a slot by reading JSON here -> bypasses SaveManager validation.
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
    mSlotCursor = SaveManager::Get().GetActiveSlotIndex();
    mPhase = Phase::MainMenu;
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
    case MenuOption::Save:     return "Save Slot";
    case MenuOption::Load:     return "Load Slot";
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
        Flash("Party restored. Active slot saved.");
        AudioManager::Get().PlaySfx("ui_confirm");
        break;

    case MenuOption::Save:
        mPhase = Phase::SaveSlotSelect;
        mSlotCursor = SaveManager::Get().GetActiveSlotIndex();
        AudioManager::Get().PlaySfx("ui_confirm");
        break;

    case MenuOption::Load:
        mPhase = Phase::LoadSlotSelect;
        mSlotCursor = SaveManager::Get().GetActiveSlotIndex();
        AudioManager::Get().PlaySfx("ui_confirm");
        break;

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
            Flash("Training claimed. Active slot saved.");
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
// Function: ActivateSlotSelection
// Purpose:
//   Save to or load from the highlighted numbered slot.
// Why:
//   The campfire menu is the player's explicit save point, so slot choice
//   belongs in this UI while serialization remains in SaveManager.
// ------------------------------------------------------------
void CampfireState::ActivateSlotSelection()
{
    if (mPhase == Phase::SaveSlotSelect)
    {
        if (SaveManager::Get().SaveCheckpointToSlot(
                mSlotCursor, "campfire_save:" + mCampfireId))
        {
            char buffer[96]{};
            std::snprintf(buffer, sizeof(buffer), "Saved to Slot %d.", mSlotCursor + 1);
            Flash(buffer);
            AudioManager::Get().PlaySfx("ui_confirm");
            mPhase = Phase::MainMenu;
        }
        else
        {
            Flash("Save failed.");
            AudioManager::Get().PlaySfx("battle_no_ap");
        }
        return;
    }

    if (mPhase == Phase::LoadSlotSelect)
    {
        std::string sceneId;
        if (SaveManager::Get().LoadCheckpointFromSlot(mSlotCursor, &sceneId))
        {
            char buffer[96]{};
            std::snprintf(buffer, sizeof(buffer), "Loaded Slot %d.", mSlotCursor + 1);
            Flash(buffer);
            AudioManager::Get().PlaySfx("ui_confirm");
            mPhase = Phase::MainMenu;
        }
        else
        {
            Flash("That slot is empty.");
            AudioManager::Get().PlaySfx("battle_no_ap");
        }
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

    const bool escPressed = Pressed(VK_ESCAPE, mEscWasDown);
    const bool backPressed = Pressed(VK_BACK, mBackWasDown);
    const bool closePressed = Pressed('U', mUWasDown);

    if (closePressed)
    {
        AudioManager::Get().PlaySfx("ui_back");
        StateManager::Get().PopState();
        return;
    }

    if (escPressed || backPressed)
    {
        AudioManager::Get().PlaySfx("ui_back");
        if (mPhase == Phase::MainMenu)
        {
            StateManager::Get().PopState();
        }
        else
        {
            mPhase = Phase::MainMenu;
        }
        return;
    }

    const int cursorCount = (mPhase == Phase::MainMenu)
        ? OptionCount()
        : SaveManager::Get().GetSlotCount();

    if (Pressed(VK_UP, mUpWasDown))
    {
        if (mPhase == Phase::MainMenu)
        {
            mCursor = (mCursor - 1 + cursorCount) % cursorCount;
        }
        else
        {
            mSlotCursor = (mSlotCursor - 1 + cursorCount) % cursorCount;
        }
        AudioManager::Get().PlaySfx("ui_navigate");
    }
    if (Pressed(VK_DOWN, mDownWasDown))
    {
        if (mPhase == Phase::MainMenu)
        {
            mCursor = (mCursor + 1) % cursorCount;
        }
        else
        {
            mSlotCursor = (mSlotCursor + 1) % cursorCount;
        }
        AudioManager::Get().PlaySfx("ui_navigate");
    }
    if (Pressed(VK_RETURN, mEnterWasDown))
    {
        if (mPhase == Phase::MainMenu)
        {
            ActivateSelection();
        }
        else
        {
            ActivateSlotSelection();
        }
    }
}

// ------------------------------------------------------------
// Function: RenderMainMenu
// Purpose:
//   Draw the top-level campfire action list.
// Why:
//   The main menu remains separate from slot selection so save/load can
//   display richer slot metadata without crowding the hub.
// ------------------------------------------------------------
void CampfireState::RenderMainMenu(float panelX, float panelY)
{
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
}

// ------------------------------------------------------------
// Function: RenderSlotMenu
// Purpose:
//   Draw numbered save slots with basic metadata.
// Why:
//   The player must see which slots are empty before confirming save/load.
// ------------------------------------------------------------
void CampfireState::RenderSlotMenu(float panelX, float panelY)
{
    const float listX = panelX + 88.0f;
    const float listY = panelY + 128.0f;
    const int slotCount = SaveManager::Get().GetSlotCount();

    for (int i = 0; i < slotCount; ++i)
    {
        const SaveSlotInfo info = SaveManager::Get().GetSlotInfo(i);
        const bool selected = (i == mSlotCursor);
        const float rowY = listY + static_cast<float>(i) * kRowH;

        char label[192]{};
        if (info.exists)
        {
            std::snprintf(label, sizeof(label), "Slot %d - %s Lv %d - %s",
                          i + 1,
                          info.leadMemberId.empty() ? "party" : info.leadMemberId.c_str(),
                          info.leadLevel,
                          info.reason.empty() ? "saved game" : info.reason.c_str());
        }
        else
        {
            std::snprintf(label, sizeof(label), "Slot %d - Empty", i + 1);
        }

        if (selected)
        {
            const float pulse = 0.86f + 0.14f * std::sin(mElapsed * 7.0f);
            const DirectX::XMVECTOR color = DirectX::XMVectorSet(1.0f, pulse, 0.45f, 1.0f);
            mTextRenderer.DrawStringRaw(">", listX - 34.0f, rowY, color);
            mTextRenderer.DrawStringRaw(label, listX, rowY, color);
        }
        else
        {
            mTextRenderer.DrawStringRaw(label, listX, rowY,
                                        info.exists ? DirectX::Colors::LightGray
                                                    : DirectX::Colors::Gray);
        }
    }
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the campfire hub panel and the active menu phase.
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
    const char* subtitle = "Rest, save, train, or manage the party.";
    if (mPhase == Phase::SaveSlotSelect) subtitle = "Choose a slot to overwrite.";
    if (mPhase == Phase::LoadSlotSelect) subtitle = "Choose a slot to load.";
    mTextRenderer.DrawStringCenteredRaw(subtitle,
                                        panelX + kPanelW * 0.5f, panelY + 76.0f,
                                        DirectX::Colors::LightGray);

    if (mPhase == Phase::MainMenu)
    {
        RenderMainMenu(panelX, panelY);
    }
    else
    {
        RenderSlotMenu(panelX, panelY);
    }

    if (mFlashTimer > 0.0f && !mFlashMessage.empty())
    {
        mTextRenderer.DrawStringCenteredRaw(mFlashMessage.c_str(),
                                            panelX + kPanelW * 0.5f,
                                            panelY + kPanelH - 68.0f,
                                            DirectX::Colors::PaleGreen);
    }

    const char* hint = (mPhase == Phase::MainMenu)
        ? "Up/Down: choose   Enter: confirm   Esc/U: close"
        : "Up/Down: choose slot   Enter: confirm   Esc: back   U: close";
    mTextRenderer.DrawStringCenteredRaw(hint,
                                        panelX + kPanelW * 0.5f,
                                        panelY + kPanelH - 34.0f,
                                        DirectX::Colors::Silver);
    mTextRenderer.EndBatch();
}
