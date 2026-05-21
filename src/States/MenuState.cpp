// ============================================================
// File: MenuState.cpp
// Responsibility: Drive the title menu input and route selected commands.
//
// Transitions:
//   Press Any Button -> reveal the command list.
//   New Game  -> choose a slot, reset durable systems, write that slot,
//                then enter overworld.
//   Continue  -> load the first occupied slot, then enter overworld.
//   Load Slot -> open a visual slot picker backed by SaveManager metadata.
//   Quit      -> request process shutdown through the Win32 message queue.
//
// Lifetime:
//   OnEnter creates title-menu GPU resources and initializes SaveManager.
//   OnExit releases title-menu GPU resources.
//
// Common mistakes:
//   1. Parsing save files here -> bypasses SaveManager validation.
//   2. Mutating party or inventory without saving -> title New Game would
//      diverge from the checkpoint system.
//   3. Rendering directly in Update -> breaks the state render contract.
// ============================================================
#define NOMINMAX
#include "MenuState.h"
#include "OverworldState.h"
#include "StateManager.h"
#include "../Audio/AudioManager.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/GameProgress.h"
#include "../Systems/Inventory.h"
#include "../Systems/PartyManager.h"
#include "../Systems/SaveManager.h"
#include "../Utils/Log.h"
#include <Windows.h>
#include <cstdio>
#include <memory>

namespace
{
    constexpr const char* kLayoutPath = "data/main_menu_layout.json";

    // ------------------------------------------------------------
    // Function: WrapIndex
    // Purpose:
    //   Wrap a cursor into [0, count).
    // Why:
    //   Menu movement should be cyclic and never produce invalid indices.
    // ------------------------------------------------------------
    int WrapIndex(int value, int count)
    {
        if (count <= 0) return 0;
        while (value < 0) value += count;
        while (value >= count) value -= count;
        return value;
    }

    // ------------------------------------------------------------
    // Function: IsStartKeyDown
    // Purpose:
    //   Detect keyboard buttons that should satisfy the press-start prompt.
    // Why:
    //   The title reference asks for a single entry prompt before exposing
    //   the command list, so the first screen should accept common keys.
    // ------------------------------------------------------------
    bool IsStartKeyDown()
    {
        static constexpr int kStartKeys[] =
        {
            VK_RETURN, VK_SPACE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
            'W', 'A', 'S', 'D', 'E', 'F', 'U', 'L', 'C'
        };

        for (int key : kStartKeys)
        {
            if ((GetAsyncKeyState(key) & 0x8000) != 0)
            {
                return true;
            }
        }
        return false;
    }
}

// ------------------------------------------------------------
// Function: MainOptionCount
// Purpose:
//   Convert the sentinel enum value into an integer count.
// Why:
//   Cursor wrapping should remain correct when menu options are added.
// ------------------------------------------------------------
int MenuState::MainOptionCount()
{
    return static_cast<int>(MainOption::Count);
}

// ------------------------------------------------------------
// Function: OnEnter
// Purpose:
//   Initialize save services and title-menu rendering.
// Why:
//   The title screen is the first state and must present save-slot status
//   before allowing Continue or Load Slot.
// ------------------------------------------------------------
void MenuState::OnEnter()
{
    SaveManager::Get().Initialize();

    auto& d3d = D3DContext::Get();
    mRenderer.Initialize(d3d.GetDevice(),
                         d3d.GetContext(),
                         kLayoutPath,
                         d3d.GetWidth(),
                         d3d.GetHeight());

    mPhase = Phase::PressStart;
    mCursor = SaveManager::Get().FindFirstExistingSlot() >= 0
        ? static_cast<int>(MainOption::Continue)
        : static_cast<int>(MainOption::NewGame);
    mSlotCursor = SaveManager::Get().GetActiveSlotIndex();
    if (mSlotCursor < 0 || mSlotCursor >= SaveManager::Get().GetSlotCount())
    {
        mSlotCursor = 0;
    }

    mElapsed = 0.0f;
    mFlashTimer = 0.0f;
    mFlashMessage.clear();
    mUpWasDown = false;
    mDownWasDown = false;
    mEnterWasDown = false;
    mBackWasDown = false;
    mEscapeWasDown = false;
    mAnyStartWasDown = IsStartKeyDown();

    LOG("[MenuState] OnEnter. Press-start title menu ready.");
}

// ------------------------------------------------------------
// Function: OnExit
// Purpose:
//   Release title-menu GPU resources.
// Why:
//   Once the player enters gameplay, this state no longer needs its
//   SpriteBatch, textures, font, or nine-slice resources.
// ------------------------------------------------------------
void MenuState::OnExit()
{
    LOG("[MenuState] OnExit");
    mRenderer.Shutdown();
}

// ------------------------------------------------------------
// Function: Pressed
// Purpose:
//   Convert GetAsyncKeyState into one-shot button presses.
// Why:
//   Holding a key should not repeatedly move the cursor or activate actions.
// ------------------------------------------------------------
bool MenuState::Pressed(int vk, bool& wasDown)
{
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool fresh = down && !wasDown;
    wasDown = down;
    return fresh;
}

// ------------------------------------------------------------
// Function: AnyStartPressed
// Purpose:
//   Convert the press-start key group into one edge-triggered action.
// Why:
//   The title screen should reveal the real menu once, then hand off to
//   normal cursor controls without immediately activating an option.
// ------------------------------------------------------------
bool MenuState::AnyStartPressed()
{
    const bool down = IsStartKeyDown();
    const bool fresh = down && !mAnyStartWasDown;
    mAnyStartWasDown = down;
    return fresh;
}

// ------------------------------------------------------------
// Function: CaptureInputLatches
// Purpose:
//   Synchronize individual button latches with current physical key states.
// Why:
//   If Enter reveals the menu, holding it for one more frame must not also
//   confirm New Game.
// ------------------------------------------------------------
void MenuState::CaptureInputLatches()
{
    mUpWasDown = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    mDownWasDown = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
    mEnterWasDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    mBackWasDown = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;
    mEscapeWasDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
}

// ------------------------------------------------------------
// Function: MainOptionLabel
// Purpose:
//   Return player-facing labels for the main title commands.
// Why:
//   Input and rendering use the same enum order.
// ------------------------------------------------------------
const char* MenuState::MainOptionLabel(MainOption option)
{
    switch (option)
    {
    case MainOption::NewGame:  return "New Game";
    case MainOption::Continue: return "Continue";
    case MainOption::LoadSlot: return "Load Slot";
    case MainOption::Quit:     return "Quit";
    default:                   return "";
    }
}

// ------------------------------------------------------------
// Function: IsMainOptionEnabled
// Purpose:
//   Report whether a main-menu command can currently be activated.
// Why:
//   Continue and Load Slot require at least one existing save slot.
// ------------------------------------------------------------
bool MenuState::IsMainOptionEnabled(MainOption option) const
{
    const bool hasSave = SaveManager::Get().FindFirstExistingSlot() >= 0;
    switch (option)
    {
    case MainOption::Continue:
    case MainOption::LoadSlot:
        return hasSave;
    case MainOption::NewGame:
    case MainOption::Quit:
        return true;
    default:
        return false;
    }
}

// ------------------------------------------------------------
// Function: MoveMainCursor
// Purpose:
//   Move the main-menu cursor, skipping disabled commands.
// Why:
//   Empty save folders should not trap the cursor on unavailable options.
// ------------------------------------------------------------
void MenuState::MoveMainCursor(int direction)
{
    const int count = MainOptionCount();
    int next = mCursor;
    for (int step = 0; step < count; ++step)
    {
        next = WrapIndex(next + direction, count);
        if (IsMainOptionEnabled(static_cast<MainOption>(next)))
        {
            mCursor = next;
            return;
        }
    }
}

// ------------------------------------------------------------
// Function: MoveSlotCursor
// Purpose:
//   Move through every configured save slot.
// Why:
//   Empty slots still need to be visible so the player understands why a
//   load request cannot be completed.
// ------------------------------------------------------------
void MenuState::MoveSlotCursor(int direction)
{
    mSlotCursor = WrapIndex(mSlotCursor + direction, SaveManager::Get().GetSlotCount());
}

// ------------------------------------------------------------
// Function: Flash
// Purpose:
//   Show short title-menu feedback.
// Why:
//   Disabled commands and invalid slot loads need clear confirmation without
//   leaving the current screen.
// ------------------------------------------------------------
void MenuState::Flash(const std::string& message)
{
    mFlashMessage = message;
    mFlashTimer = mRenderer.GetFlashDuration();
    LOG("[MenuState] %s", message.c_str());
}

// ------------------------------------------------------------
// Function: FindPreferredNewGameSlot
// Purpose:
//   Pick the first empty save slot for the New Game slot picker.
// Why:
//   A slot system should encourage creating a new save in an empty slot
//   instead of always overwriting Slot 1.
// ------------------------------------------------------------
int MenuState::FindPreferredNewGameSlot() const
{
    const int slotCount = SaveManager::Get().GetSlotCount();
    for (int i = 0; i < slotCount; ++i)
    {
        if (!SaveManager::Get().SlotExists(i))
        {
            return i;
        }
    }

    const int activeSlot = SaveManager::Get().GetActiveSlotIndex();
    return (activeSlot >= 0 && activeSlot < slotCount) ? activeSlot : 0;
}

// ------------------------------------------------------------
// Function: StartNewGame
// Purpose:
//   Reset durable systems and create the selected save slot.
// Why:
//   New Game should start from a clean party, inventory, and world-progress
//   baseline before entering the overworld.
// ------------------------------------------------------------
void MenuState::StartNewGame(int slotIndex)
{
    AudioManager::Get().PlaySfx("ui_confirm");

    PartyManager::Get().ResetToDefaults();
    Inventory::Get().ResetToDefaults();
    GameProgress::Get().Reset();
    SaveManager::Get().SaveCheckpointToSlot(slotIndex, "new_game");

    StateManager::Get().ChangeState(std::make_unique<OverworldState>());
}

// ------------------------------------------------------------
// Function: ContinueFirstSlot
// Purpose:
//   Load the first occupied save slot.
// Why:
//   Continue should be a fast path for players who do not need to choose a
//   specific slot.
// ------------------------------------------------------------
void MenuState::ContinueFirstSlot()
{
    const int firstSlot = SaveManager::Get().FindFirstExistingSlot();
    if (firstSlot < 0)
    {
        AudioManager::Get().PlaySfx("battle_no_ap");
        Flash("No save slot exists.");
        return;
    }

    LoadSlot(firstSlot);
}

// ------------------------------------------------------------
// Function: LoadSlot
// Purpose:
//   Restore one save slot and enter the saved scene.
// Why:
//   SaveManager remains the authority for validation and durable state
//   restoration; MenuState only performs the state transition after success.
// ------------------------------------------------------------
bool MenuState::LoadSlot(int slotIndex)
{
    std::string sceneId;
    if (!SaveManager::Get().LoadCheckpointFromSlot(slotIndex, &sceneId))
    {
        AudioManager::Get().PlaySfx("battle_no_ap");
        Flash("That slot is empty.");
        return false;
    }

    AudioManager::Get().PlaySfx("ui_confirm");
    if (sceneId != "overworld")
    {
        LOG("[MenuState] Save scene '%s' is not implemented yet; loading overworld fallback.",
            sceneId.c_str());
    }

    StateManager::Get().ChangeState(std::make_unique<OverworldState>());
    return true;
}

// ------------------------------------------------------------
// Function: ActivateMainSelection
// Purpose:
//   Execute the currently highlighted main-menu command.
// Why:
//   Update remains focused on input edges while command behavior stays here.
// ------------------------------------------------------------
void MenuState::ActivateMainSelection()
{
    const MainOption option = static_cast<MainOption>(mCursor);
    if (!IsMainOptionEnabled(option))
    {
        AudioManager::Get().PlaySfx("battle_no_ap");
        Flash("No save slot exists.");
        return;
    }

    switch (option)
    {
    case MainOption::NewGame:
        mPhase = Phase::NewGameSlots;
        mSlotCursor = FindPreferredNewGameSlot();
        AudioManager::Get().PlaySfx("ui_confirm");
        break;
    case MainOption::Continue:
        ContinueFirstSlot();
        break;
    case MainOption::LoadSlot:
        mPhase = Phase::LoadSlots;
        mSlotCursor = SaveManager::Get().GetActiveSlotIndex();
        if (mSlotCursor < 0 || mSlotCursor >= SaveManager::Get().GetSlotCount())
        {
            mSlotCursor = 0;
        }
        AudioManager::Get().PlaySfx("ui_confirm");
        break;
    case MainOption::Quit:
        AudioManager::Get().PlaySfx("ui_back");
        PostQuitMessage(0);
        break;
    default:
        break;
    }
}

// ------------------------------------------------------------
// Function: ActivateSlotSelection
// Purpose:
//   Create or load the currently highlighted save slot.
// Why:
//   Slot metadata can be displayed by the renderer, but loading stays in
//   MenuState through SaveManager.
// ------------------------------------------------------------
void MenuState::ActivateSlotSelection()
{
    if (mPhase == Phase::NewGameSlots)
    {
        StartNewGame(mSlotCursor);
        return;
    }

    LoadSlot(mSlotCursor);
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Run title-menu input, timers, and phase changes.
// Why:
//   The active state exclusively receives Update from StateManager.
// ------------------------------------------------------------
void MenuState::Update(float dt)
{
    mElapsed += dt;
    if (mFlashTimer > 0.0f)
    {
        mFlashTimer -= dt;
        if (mFlashTimer < 0.0f)
        {
            mFlashTimer = 0.0f;
        }
    }

    const bool backPressed = Pressed(VK_BACK, mBackWasDown);
    const bool escapePressed = Pressed(VK_ESCAPE, mEscapeWasDown);
    if (mPhase == Phase::PressStart)
    {
        if (AnyStartPressed())
        {
            mPhase = Phase::MainOptions;
            CaptureInputLatches();
            AudioManager::Get().PlaySfx("ui_confirm");
        }
        return;
    }

    if ((backPressed || escapePressed) &&
        (mPhase == Phase::NewGameSlots || mPhase == Phase::LoadSlots))
    {
        mPhase = Phase::MainOptions;
        AudioManager::Get().PlaySfx("ui_back");
        return;
    }

    if (backPressed && mPhase == Phase::MainOptions)
    {
        mPhase = Phase::PressStart;
        CaptureInputLatches();
        AudioManager::Get().PlaySfx("ui_back");
        return;
    }

    if (Pressed(VK_UP, mUpWasDown))
    {
        if (mPhase == Phase::MainOptions)
        {
            MoveMainCursor(-1);
        }
        else
        {
            MoveSlotCursor(-1);
        }
        AudioManager::Get().PlaySfx("ui_navigate");
    }

    if (Pressed(VK_DOWN, mDownWasDown))
    {
        if (mPhase == Phase::MainOptions)
        {
            MoveMainCursor(1);
        }
        else
        {
            MoveSlotCursor(1);
        }
        AudioManager::Get().PlaySfx("ui_navigate");
    }

    if (Pressed(VK_RETURN, mEnterWasDown))
    {
        if (mPhase == Phase::MainOptions)
        {
            ActivateMainSelection();
        }
        else
        {
            ActivateSlotSelection();
        }
    }
}

// ------------------------------------------------------------
// Function: BuildOptionViews
// Purpose:
//   Convert main-menu command state into renderer view data.
// Why:
//   The renderer should not know about SaveManager or title command enums.
// ------------------------------------------------------------
std::vector<TitleMenuOptionView> MenuState::BuildOptionViews() const
{
    std::vector<TitleMenuOptionView> options;
    options.reserve(static_cast<size_t>(MainOptionCount()));
    for (int i = 0; i < MainOptionCount(); ++i)
    {
        const MainOption option = static_cast<MainOption>(i);
        TitleMenuOptionView view{};
        view.label = MainOptionLabel(option);
        view.enabled = IsMainOptionEnabled(option);
        options.push_back(view);
    }
    return options;
}

// ------------------------------------------------------------
// Function: BuildSlotViews
// Purpose:
//   Convert SaveManager slot metadata into renderer view data.
// Why:
//   The title renderer stays read-only and does not parse save files.
// ------------------------------------------------------------
std::vector<TitleMenuSlotView> MenuState::BuildSlotViews() const
{
    std::vector<TitleMenuSlotView> slots;
    const std::vector<SaveSlotInfo> infos = SaveManager::Get().GetSlotInfos();
    slots.reserve(infos.size());

    const int activeSlot = SaveManager::Get().GetActiveSlotIndex();
    for (const SaveSlotInfo& info : infos)
    {
        TitleMenuSlotView view{};

        char primary[64]{};
        std::snprintf(primary, sizeof(primary), "Slot %d%s",
                      info.slotIndex + 1,
                      info.slotIndex == activeSlot ? "  Active" : "");
        view.primary = primary;
        view.exists = info.exists;
        view.active = (info.slotIndex == activeSlot);

        if (info.exists)
        {
            const char* lead = info.leadMemberId.empty()
                ? "Party"
                : info.leadMemberId.c_str();
            const char* reason = info.reason.empty()
                ? "saved game"
                : info.reason.c_str();

            char secondary[192]{};
            std::snprintf(secondary, sizeof(secondary), "%s Lv %d - %s",
                          lead, info.leadLevel, reason);
            view.secondary = secondary;
        }
        else
        {
            view.secondary = "Empty";
        }

        slots.push_back(view);
    }

    return slots;
}

// ------------------------------------------------------------
// Function: BuildRenderState
// Purpose:
//   Gather all read-only title-menu data for the renderer.
// Why:
//   Keeping a single render-state struct prevents the renderer from reaching
//   back into gameplay systems.
// ------------------------------------------------------------
TitleMenuRenderState MenuState::BuildRenderState() const
{
    TitleMenuRenderState state{};
    if (mPhase == Phase::PressStart)
    {
        state.phase = TitleMenuVisualPhase::PressStart;
    }
    else if (mPhase == Phase::MainOptions)
    {
        state.phase = TitleMenuVisualPhase::MainOptions;
    }
    else if (mPhase == Phase::NewGameSlots)
    {
        state.phase = TitleMenuVisualPhase::NewGameSlots;
    }
    else
    {
        state.phase = TitleMenuVisualPhase::LoadSlots;
    }
    state.options = BuildOptionViews();
    state.slots = BuildSlotViews();
    state.cursor = mCursor;
    state.slotCursor = mSlotCursor;
    state.elapsed = mElapsed;
    state.flashMessage = mFlashMessage;

    if (mFlashTimer > 0.0f)
    {
        const float fadeWindow = 0.25f;
        state.flashAlpha = (mFlashTimer < fadeWindow)
            ? (mFlashTimer / fadeWindow)
            : 1.0f;
    }

    return state;
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the authored title menu.
// Why:
//   D3DContext clears the back buffer; the state then owns all visible
//   title-screen draw calls.
// ------------------------------------------------------------
void MenuState::Render()
{
    auto& d3d = D3DContext::Get();
    mRenderer.SetScreenSize(d3d.GetWidth(), d3d.GetHeight());
    mRenderer.Render(d3d.GetContext(), BuildRenderState());
}
