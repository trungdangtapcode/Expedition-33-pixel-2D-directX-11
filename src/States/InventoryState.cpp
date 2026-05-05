// ============================================================
// File: InventoryState.cpp
// Responsibility: Implement the tabbed RPG inventory screen.
//
// Reads/writes the same Inventory + PartyManager singletons that
// the battle system uses, so every change here is immediately visible
// in the next battle.
//
// File is split into three logical sections:
//   1. Lifecycle + state refresh           (~80 lines)
//   2. Input handlers + actions            (~110 lines)
//   3. Render dispatch                     (~50 lines)
//
// The render helpers themselves live in InventoryStateRender.cpp so
// this file stays under the 300-line CLAUDE.md ceiling.
// ============================================================
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "InventoryState.h"
#include "StateManager.h"
#include "../Renderer/D3DContext.h"
#include "../Battle/ItemRegistry.h"
#include "../Battle/ItemData.h"
#include "../Systems/Inventory.h"
#include "../Systems/PartyManager.h"
#include "../Utils/Log.h"

#include <Windows.h>

// ============================================================
//  Lifecycle
// ============================================================

void InventoryState::OnEnter()
{
    LOG("[InventoryState] OnEnter");

    auto& d3d = D3DContext::Get();

    // Force-load the catalog so item names / descriptions are available
    // even if no battle has been entered yet this session.
    ItemRegistry::Get().EnsureLoaded();

    // Dialog box for backdrop, panel, and row backgrounds — same 9-slice
    // the battle menus use so the visual language stays consistent.
    mDialogBox.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        L"assets/UI/ui-dialog-box-hd.png",
        "assets/UI/ui-dialog-box-hd.json",
        d3d.GetWidth(), d3d.GetHeight());

    mTextRenderer.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        L"assets/fonts/arial_16.spritefont",
        d3d.GetWidth(), d3d.GetHeight());

    // Scroll chevrons — placeholder pointer texture (see asset-todo.md).
    mChevronDown.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        L"assets/UI/enemy-pointer-ui.png",
        d3d.GetWidth(), d3d.GetHeight(), 4.0f, 4.0f);
    mChevronUp.Initialize(
        d3d.GetDevice(), d3d.GetContext(),
        L"assets/UI/enemy-pointer-ui.png",
        d3d.GetWidth(), d3d.GetHeight(), 4.0f, 4.0f);

    RefreshConsumables();
    mTab          = Tab::Items;
    mPhase        = Phase::ItemsGrid;
    mItemCursor   = 0;
    mSlotCursor   = 0;
    mPickerCursor = 0;
    mPickerSlot   = EquipSlot::None;
    mFlashMessage.clear();
    mFlashTimer   = 0.0f;
    mElapsed      = 0.0f;

    // Pre-set every edge tracker to "down" so the keys the player was
    // holding when the state opened (typically 'I') do NOT register as
    // fresh presses on the first frame.
    mUpWasDown    = true;
    mDownWasDown  = true;
    mLeftWasDown  = true;
    mRightWasDown = true;
    mEnterWasDown = true;
    mEscWasDown   = true;
    mTabWasDown   = true;
    mIWasDown     = true;
    mBackWasDown  = true;
}

void InventoryState::OnExit()
{
    LOG("[InventoryState] OnExit");
    mChevronUp.Shutdown();
    mChevronDown.Shutdown();
    mTextRenderer.Shutdown();
    mDialogBox.Shutdown();
}

// ============================================================
//  Data refresh
// ============================================================

void InventoryState::RefreshConsumables()
{
    // Filter Inventory::OwnedIds to ItemKind::Consumable so the Items
    // tab never shows weapons / armor (those live in the Equipment tab).
    mConsumables.clear();
    for (const std::string& id : Inventory::Get().OwnedIds())
    {
        const ItemData* item = ItemRegistry::Get().Find(id);
        if (item && item->kind == ItemKind::Consumable)
            mConsumables.push_back(id);
    }
    if (mItemCursor >= static_cast<int>(mConsumables.size()))
        mItemCursor = (mConsumables.empty()) ? 0 : static_cast<int>(mConsumables.size()) - 1;
    if (mItemCursor < 0) mItemCursor = 0;
}

void InventoryState::RefreshPicker()
{
    // Filter to items whose equipSlot matches the slot the picker
    // was opened for.  No "show every equipment item" view — saves
    // the player from being shown swords while equipping a hat.
    mPickerItems.clear();
    for (const std::string& id : Inventory::Get().OwnedIds())
    {
        const ItemData* item = ItemRegistry::Get().Find(id);
        if (item && item->equipSlot == mPickerSlot)
            mPickerItems.push_back(id);
    }
    if (mPickerCursor < 0) mPickerCursor = 0;
    // The list has one extra "(unequip)" pseudo-entry at the end so the
    // cursor can land there to clear the slot — clamp accordingly.
    const int maxCursor = static_cast<int>(mPickerItems.size());  // == "(unequip)" index
    if (mPickerCursor > maxCursor) mPickerCursor = maxCursor;
}

void InventoryState::OpenPicker()
{
    constexpr EquipSlot order[kEquipSlotCount] = {
        EquipSlot::Weapon, EquipSlot::Body, EquipSlot::Head, EquipSlot::Accessory
    };
    if (mSlotCursor < 0 || mSlotCursor >= kEquipSlotCount) return;

    mPickerSlot   = order[mSlotCursor];
    mPickerCursor = 0;
    RefreshPicker();
    mPhase = Phase::EquipmentPicker;
}

void InventoryState::Flash(const std::string& msg)
{
    mFlashMessage = msg;
    mFlashTimer   = kFlashDuration;
    LOG("[InventoryState] %s", msg.c_str());
}

// ============================================================
//  Actions
// ============================================================

bool InventoryState::TryUseItem(const std::string& id)
{
    const ItemData* item = ItemRegistry::Get().Find(id);
    if (!item) { Flash("Unknown item: " + id); return false; }
    BattlerStats copy = PartyManager::Get().GetMemberStats(mMemberIndex);
    const int hpBefore = copy.hp;
    const int mpBefore = copy.mp;
    char buf[128]{};

    switch (item->effect)
    {
    case ItemEffectKind::HealHp:
        if (copy.hp >= copy.maxHp) { Flash("HP is already full."); return false; }
        copy.hp += item->amount;
        copy.ClampHp();
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Used %s (+%d HP)",
                    item->name.c_str(), copy.hp - hpBefore);
        break;
    case ItemEffectKind::HealMp:
        if (copy.mp >= copy.maxMp) { Flash("MP is already full."); return false; }
        copy.mp += item->amount;
        if (copy.mp > copy.maxMp) copy.mp = copy.maxMp;
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Used %s (+%d MP)",
                    item->name.c_str(), copy.mp - mpBefore);
        break;
    case ItemEffectKind::FullHeal:
        if (copy.hp >= copy.maxHp && copy.mp >= copy.maxMp)
        { Flash("HP and MP are already full."); return false; }
        copy.hp = copy.maxHp;
        copy.mp = copy.maxMp;
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Used %s (full restore)", item->name.c_str());
        break;
    case ItemEffectKind::RestoreRage:
        Flash("Rage only matters during battle.");
        return false;
    case ItemEffectKind::Revive:
    case ItemEffectKind::DealDamage:
    case ItemEffectKind::StatBuff:
    case ItemEffectKind::Cleanse:
        Flash("Cannot use this outside of battle.");
        return false;
    }

    // Persist + consume.  Stats first so a hypothetical crash leaves
    // the player healed but with the item intact (favors the player).
    PartyManager::Get().SetMemberStats(mMemberIndex, copy);
    Inventory::Get().Remove(id, 1);
    Flash(buf);
    RefreshConsumables();
    return true;
}

bool InventoryState::TryEquip(EquipSlot slot, const std::string& itemId)
{
    if (PartyManager::Get().EquipItem(mMemberIndex, slot, itemId))
    {
        const ItemData* item = ItemRegistry::Get().Find(itemId);
        Flash(std::string("Equipped ") + (item ? item->name : itemId));
        return true;
    }
    Flash("Could not equip that item.");
    return false;
}

void InventoryState::TryUnequip(EquipSlot slot)
{
    std::string equippedId = PartyManager::Get().GetEquippedItem(mMemberIndex, slot);
    if (equippedId.empty()) { Flash("Nothing to unequip."); return; }
    PartyManager::Get().UnequipItem(mMemberIndex, slot);
    const ItemData* item = ItemRegistry::Get().Find(equippedId);
    Flash(std::string("Unequipped ") + (item ? item->name : equippedId));
}

// ============================================================
//  Update — top-level dispatch
// ============================================================

void InventoryState::Update(float dt)
{
    mElapsed += dt;
    mChevronUp.Update(dt);
    mChevronDown.Update(dt);

    if (mFlashTimer > 0.0f)
    {
        mFlashTimer -= dt;
        if (mFlashTimer < 0.0f) mFlashTimer = 0.0f;
    }

    // Esc / I / Backspace = close (or exit picker overlay first).
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    const bool escPressed  = pressed(VK_ESCAPE, mEscWasDown);
    const bool iPressed    = pressed('I',       mIWasDown);
    const bool backPressed = pressed(VK_BACK,   mBackWasDown);
    if (escPressed || iPressed || backPressed)
    {
        // From the picker, Esc/Backspace returns to the slot list.
        // From any other phase, it closes the inventory entirely.
        if (mPhase == Phase::EquipmentPicker)
        {
            mPhase = Phase::EquipmentSlots;
            return;
        }
        StateManager::Get().PopState();
        return;
    }

    // Tab key cycles between Items and Equipment top-level tabs.
    // Disabled inside the picker (which is a sub-state of Equipment).
    const bool tabPressed = pressed(VK_TAB, mTabWasDown);
    if (tabPressed && mPhase != Phase::EquipmentPicker)
    {
        mTab   = (mTab == Tab::Items) ? Tab::Equipment : Tab::Items;
        mPhase = (mTab == Tab::Items) ? Phase::ItemsGrid : Phase::EquipmentSlots;
        if (mTab == Tab::Items) RefreshConsumables();
        return;
    }

    // Q / E cycle the target party member (across all phases).
    // Not available inside the picker — the picker is for a specific member's slot.
    if (mPhase != Phase::EquipmentPicker)
    {
        const bool qPressed = pressed('Q', mQWasDown);
        const bool ePressed = pressed('E', mEWasDown);
        const int partySize = static_cast<int>(PartyManager::Get().GetActiveParty().size());
        if (partySize > 1)
        {
            if (qPressed)
            {
                mMemberIndex = (mMemberIndex - 1 + partySize) % partySize;
                const auto& party = PartyManager::Get().GetActiveParty();
                Flash(std::string("Target: ") + party[mMemberIndex].name);
            }
            if (ePressed)
            {
                mMemberIndex = (mMemberIndex + 1) % partySize;
                const auto& party = PartyManager::Get().GetActiveParty();
                Flash(std::string("Target: ") + party[mMemberIndex].name);
            }
        }
    }
    else
    {
        // Consume Q/E to prevent stale edges.
        pressed('Q', mQWasDown);
        pressed('E', mEWasDown);
    }

    switch (mPhase)
    {
    case Phase::ItemsGrid:        HandleItemsInput();  break;
    case Phase::EquipmentSlots:   HandleSlotsInput();  break;
    case Phase::EquipmentPicker:  HandlePickerInput(); break;
    }
}

// ============================================================
//  Phase-specific input handlers
// ============================================================

void InventoryState::HandleItemsInput()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    const int n = static_cast<int>(mConsumables.size());
    if (n == 0) return;

    // Grid is 4 columns wide; Up/Down jump by 4 (one row), Left/Right by 1.
    constexpr int kCols = 4;
    if (pressed(VK_LEFT,  mLeftWasDown))  mItemCursor = (mItemCursor - 1 + n) % n;
    if (pressed(VK_RIGHT, mRightWasDown)) mItemCursor = (mItemCursor + 1) % n;
    if (pressed(VK_UP,    mUpWasDown))
    {
        const int next = mItemCursor - kCols;
        mItemCursor = (next >= 0) ? next : mItemCursor;
    }
    if (pressed(VK_DOWN,  mDownWasDown))
    {
        const int next = mItemCursor + kCols;
        mItemCursor = (next < n) ? next : mItemCursor;
    }

    if (pressed(VK_RETURN, mEnterWasDown))
    {
        TryUseItem(mConsumables[mItemCursor]);
    }
}

void InventoryState::HandleSlotsInput()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    if (pressed(VK_UP,   mUpWasDown))
        mSlotCursor = (mSlotCursor - 1 + kEquipSlotCount) % kEquipSlotCount;
    if (pressed(VK_DOWN, mDownWasDown))
        mSlotCursor = (mSlotCursor + 1) % kEquipSlotCount;

    // Left/Right cycle party member (same as Q/E but more discoverable)
    {
        const int partySize = static_cast<int>(PartyManager::Get().GetActiveParty().size());
        if (partySize > 1)
        {
            if (pressed(VK_LEFT,  mLeftWasDown))
            {
                mMemberIndex = (mMemberIndex - 1 + partySize) % partySize;
                const auto& party = PartyManager::Get().GetActiveParty();
                Flash(std::string("Target: ") + party[mMemberIndex].name);
            }
            if (pressed(VK_RIGHT, mRightWasDown))
            {
                mMemberIndex = (mMemberIndex + 1) % partySize;
                const auto& party = PartyManager::Get().GetActiveParty();
                Flash(std::string("Target: ") + party[mMemberIndex].name);
            }
        }
    }

    if (pressed(VK_RETURN, mEnterWasDown))
    {
        OpenPicker();
    }
}

void InventoryState::HandlePickerInput()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    // Picker entries: 0..N-1 are items, index N is the "(unequip)" pseudo-row.
    const int total = static_cast<int>(mPickerItems.size()) + 1;

    if (pressed(VK_UP,   mUpWasDown))
        mPickerCursor = (mPickerCursor - 1 + total) % total;
    if (pressed(VK_DOWN, mDownWasDown))
        mPickerCursor = (mPickerCursor + 1) % total;

    if (pressed(VK_RETURN, mEnterWasDown))
    {
        if (PickerCursorIsUnequip())
        {
            TryUnequip(mPickerSlot);
        }
        else
        {
            TryEquip(mPickerSlot, mPickerItems[mPickerCursor]);
        }
        // Refresh the candidate list (an equip moves the item out of
        // inventory; an unequip moves the previous one in).  Stay in
        // picker so the player can chain swaps without re-navigating.
        RefreshPicker();
        // After a successful equip the cursor may now be past the end
        // of a shrunken list — clamp.
        const int newTotal = static_cast<int>(mPickerItems.size()) + 1;
        if (mPickerCursor >= newTotal) mPickerCursor = newTotal - 1;
    }
}

// ============================================================
//  Render dispatch
//
//  Render helpers are intentionally split across this file and
//  InventoryStateRender.cpp because:
//    - This .cpp would otherwise blow past the 300-line CLAUDE.md ceiling
//    - The split is along a clean responsibility line: this file handles
//      lifecycle/input/actions, the other handles pure draw calls
// ============================================================

void InventoryState::Render()
{
    auto& d3d = D3DContext::Get();

    const float screenW = static_cast<float>(d3d.GetWidth());
    const float screenH = static_cast<float>(d3d.GetHeight());

    // Centered panel with deliberate generous size — RPG inventories
    // need breathing room for grid + detail panel + stats footer.
    constexpr float kPanelW = 960.0f;
    constexpr float kPanelH = 600.0f;
    const float panelX = (screenW - kPanelW) * 0.5f;
    const float panelY = (screenH - kPanelH) * 0.5f;

    // ---- Backdrop ----
    // Translucent black quad covering the entire screen so the
    // overworld stays dimly visible underneath as a "paused" cue.
    {
        DirectX::XMVECTOR dim = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.65f);
        mDialogBox.Draw(d3d.GetContext(), 0.0f, 0.0f,
                        screenW, screenH, 1.0f,
                        DirectX::XMMatrixIdentity(), dim);
    }

    // ---- Main panel ----
    {
        DirectX::XMVECTOR boxColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.96f);
        mDialogBox.Draw(d3d.GetContext(), panelX, panelY,
                        kPanelW, kPanelH, 1.0f,
                        DirectX::XMMatrixIdentity(), boxColor);
    }

    // ---- Layout subdivisions ----
    constexpr float kTabH      = 48.0f;
    constexpr float kFooterH   = 60.0f;
    constexpr float kHintH     = 30.0f;
    constexpr float kPad       = 16.0f;

    const float contentY = panelY + kTabH;
    const float contentH = kPanelH - kTabH - kFooterH - kHintH;

    const float leftW  = kPanelW * 0.55f - kPad * 1.5f;
    const float rightX = panelX + leftW + kPad * 2.0f;
    const float rightW = kPanelW - leftW - kPad * 3.0f;

    RenderTabs(panelX, panelY, kPanelW);

    if (mTab == Tab::Items)
    {
        RenderItemsTab(panelX + kPad, contentY + kPad, leftW, contentH - kPad * 2);
    }
    else
    {
        RenderEquipmentTab(panelX + kPad, contentY + kPad, leftW, contentH - kPad * 2);
    }

    RenderDetailPanel(rightX, contentY + kPad, rightW, contentH - kPad * 2);

    const float footerY = panelY + kPanelH - kFooterH - kHintH;
    RenderStatsFooter(panelX, footerY, kPanelW, kFooterH);
    RenderHintFooter (panelX, panelY + kPanelH - kHintH, kPanelW, kHintH);
}
