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
#include "MemoryArchiveState.h"
#include "StateManager.h"
#include "../Audio/AudioManager.h"
#include "../Events/EventManager.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/GameProgress.h"
#include "../Systems/LocalizationManager.h"
#include "../Systems/PartyManager.h"
#include "../Systems/SaveManager.h"
#include "../Systems/Wallet.h"
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
    constexpr float kPanelH = 460.0f;
    constexpr float kRowH = 36.0f;
}

// ------------------------------------------------------------
// Function: CampfireState
// Purpose:
//   Capture the stable campfire id and one-time training reward.
// Why:
//   The overworld campfire object remains SceneGraph-owned, so this state
//   should only receive immutable data needed for menu actions.
// Parameters:
//   campfireId        - Stable id from data/campfires.json.
//   upgradeExpReward - One-time training reward attached to this campfire.
//   playerX/playerY  - Player position to persist if this menu saves a slot.
// ------------------------------------------------------------
CampfireState::CampfireState(std::string campfireId, int upgradeExpReward, float playerX, float playerY)
    : mCampfireId(std::move(campfireId))
    , mUpgradeExpReward(upgradeExpReward)
    , mPlayerX(playerX)
    , mPlayerY(playerY)
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

    const std::string fontPath = LocalizationManager::Get().GetCurrentFontPath();
    mTextRenderer.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        std::wstring(fontPath.begin(), fontPath.end()),
        d3d.GetWidth(), d3d.GetHeight());
    mCurrencyHud.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        std::wstring(fontPath.begin(), fontPath.end()),
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
    mCurrencyHud.Shutdown();
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

// ------------------------------------------------------------
// Function: UpdateSavedOverworldSnapshot
// Purpose:
//   Capture this campfire as the current save/load restore point.
// Why:
//   CampfireState is an overlay, so it cannot rebuild the overworld itself;
//   it writes the stable snapshot that SaveManager serializes into the slot.
// ------------------------------------------------------------
void CampfireState::UpdateSavedOverworldSnapshot()
{
    OverworldProgressSnapshot snapshot{};
    snapshot.sceneId = SaveManager::Get().GetConfig().autoSceneId;
    snapshot.checkpointId = "campfire:" + mCampfireId;
    snapshot.playerX = mPlayerX;
    snapshot.playerY = mPlayerY;
    snapshot.hasPlayerPosition = true;
    GameProgress::Get().SetOverworldSnapshot(snapshot);
}

std::string CampfireState::OptionLabel(MenuOption option)
{
    switch (option)
    {
    case MenuOption::Rest:     return LocalizationManager::Get().Text("campfire.rest");
    case MenuOption::Save:     return LocalizationManager::Get().Text("campfire.save_slot");
    case MenuOption::Load:     return LocalizationManager::Get().Text("campfire.load_slot");
    case MenuOption::MemoryArchive: return LocalizationManager::Get().Text("campfire.memory_archive");
    case MenuOption::Training: return LocalizationManager::Get().Text("campfire.upgrade_party");
    case MenuOption::Lineup:   return LocalizationManager::Get().Text("campfire.lineup");
    case MenuOption::Exit:     return LocalizationManager::Get().Text("campfire.exit");
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
        UpdateSavedOverworldSnapshot();
        SaveManager::Get().SaveCheckpoint("campfire_rest:" + mCampfireId);
        Flash(LocalizationManager::Get().Text("campfire.flash.party_restored"));
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

    case MenuOption::MemoryArchive:
        AudioManager::Get().PlaySfx("ui_confirm");
        StateManager::Get().PushState(std::make_unique<MemoryArchiveState>());
        break;

    case MenuOption::Training:
    {
        const std::string flag = "campfire_upgrade:" + mCampfireId;
        PartyManager::Get().RestoreFullHP();

        if (mUpgradeExpReward > 0 && !GameProgress::Get().HasFlag(flag))
        {
            PartyManager::Get().AddExp(mUpgradeExpReward);
            GameProgress::Get().SetFlag(flag);
            UpdateSavedOverworldSnapshot();
            SaveManager::Get().SaveCheckpoint("campfire_upgrade:" + mCampfireId);

            char buffer[128]{};
            std::snprintf(buffer, sizeof(buffer), "%d", mUpgradeExpReward);
            Flash(LocalizationManager::Get().Format(
                "campfire.flash.party_upgraded",
                { { "exp", buffer } }));
            AudioManager::Get().PlaySfx("ui_confirm");
        }
        else
        {
            UpdateSavedOverworldSnapshot();
            SaveManager::Get().SaveCheckpoint("campfire_rest:" + mCampfireId);
            Flash(LocalizationManager::Get().Text("campfire.flash.training_claimed"));
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
        UpdateSavedOverworldSnapshot();
        if (SaveManager::Get().SaveCheckpointToSlot(
                mSlotCursor, "campfire_save:" + mCampfireId))
        {
            char buffer[96]{};
            std::snprintf(buffer, sizeof(buffer), "%d", mSlotCursor + 1);
            Flash(LocalizationManager::Get().Format(
                "campfire.flash.saved_slot",
                { { "index", buffer } }));
            AudioManager::Get().PlaySfx("ui_confirm");
            mPhase = Phase::MainMenu;
        }
        else
        {
            Flash(LocalizationManager::Get().Text("campfire.flash.save_failed"));
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
            std::snprintf(buffer, sizeof(buffer), "%d", mSlotCursor + 1);
            Flash(LocalizationManager::Get().Format(
                "campfire.flash.loaded_slot",
                { { "index", buffer } }));
            AudioManager::Get().PlaySfx("ui_confirm");
            EventManager::Get().Broadcast("checkpoint_loaded", {});
            StateManager::Get().PopState();
            return;
        }
        else
        {
            Flash(LocalizationManager::Get().Text("campfire.flash.slot_empty"));
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
            const std::string label = OptionLabel(static_cast<MenuOption>(i));
            mTextRenderer.DrawStringRaw(">", listX - 34.0f, rowY, color);
            mTextRenderer.DrawStringRaw(label.c_str(), listX, rowY, color);
        }
        else
        {
            const std::string label = OptionLabel(static_cast<MenuOption>(i));
            mTextRenderer.DrawStringRaw(label.c_str(), listX, rowY,
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
            const std::string checkpoint = info.checkpointId.empty()
                ? (info.reason.empty() ? LocalizationManager::Get().Text("menu.saved_game") : info.reason)
                : info.checkpointId;
            const std::string lead = info.leadMemberId.empty()
                ? LocalizationManager::Get().Text("common.party")
                : LocalizationManager::Get().TextOrFallback("party.member." + info.leadMemberId, info.leadMemberId);
            if (info.hasPlayerPosition)
            {
                char xText[32]{};
                char yText[32]{};
                char positionText[64]{};
                std::snprintf(xText, sizeof(xText), "%.0f", info.playerX);
                std::snprintf(yText, sizeof(yText), "%.0f", info.playerY);
                std::snprintf(positionText, sizeof(positionText), "%s, %s", xText, yText);
                const std::string text = LocalizationManager::Get().Format(
                    "campfire.slot_with_position",
                    {
                        { "index", std::to_string(i + 1) },
                        { "label", lead },
                        { "party", lead },
                        { "x", xText },
                        { "y", yText },
                        { "lead", lead },
                        { "level", std::to_string(info.leadLevel) },
                        { "checkpoint", checkpoint },
                        { "position", positionText }
                    });
                std::snprintf(label, sizeof(label), "%s", text.c_str());
            }
            else
            {
                const std::string text = LocalizationManager::Get().Format(
                    "campfire.slot",
                    {
                        { "index", std::to_string(i + 1) },
                        { "label", lead },
                        { "party", lead },
                        { "lead", lead },
                        { "level", std::to_string(info.leadLevel) },
                        { "checkpoint", checkpoint }
                    });
                std::snprintf(label, sizeof(label), "%s", text.c_str());
            }
        }
        else
        {
            const std::string text = LocalizationManager::Get().Format(
                "campfire.slot_empty",
                { { "index", std::to_string(i + 1) } });
            std::snprintf(label, sizeof(label), "%s", text.c_str());
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
    mCurrencyHud.SetScreenSize(d3d.GetWidth(), d3d.GetHeight());
    mCurrencyHud.RenderCampfirePanel(ctx, Wallet::Get().GetCoins(), panelX, panelY);

    mTextRenderer.BeginBatch(ctx);
    const std::string title = LocalizationManager::Get().Text("campfire.title");
    mTextRenderer.DrawStringCenteredRaw(title.c_str(), panelX + kPanelW * 0.5f, panelY + 34.0f,
                                        DirectX::Colors::White, 1.35f, true);
    std::string subtitle = LocalizationManager::Get().Text("campfire.subtitle_main");
    if (mPhase == Phase::SaveSlotSelect) subtitle = LocalizationManager::Get().Text("campfire.subtitle_save");
    if (mPhase == Phase::LoadSlotSelect) subtitle = LocalizationManager::Get().Text("campfire.subtitle_load");
    mTextRenderer.DrawStringCenteredRaw(subtitle.c_str(),
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

    const std::string hint = (mPhase == Phase::MainMenu)
        ? LocalizationManager::Get().Text("campfire.hint_main")
        : LocalizationManager::Get().Text("campfire.hint_slot");
    mTextRenderer.DrawStringCenteredRaw(hint.c_str(),
                                        panelX + kPanelW * 0.5f,
                                        panelY + kPanelH - 34.0f,
                                        DirectX::Colors::Silver);
    mTextRenderer.EndBatch();
}
