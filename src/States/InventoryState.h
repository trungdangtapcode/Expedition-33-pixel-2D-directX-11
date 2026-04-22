// ============================================================
// File: InventoryState.h
// Responsibility: Full tabbed RPG inventory screen with consumable
//                 grid, equipment slots, equipment picker overlay,
//                 and live stat preview.
//
// Layout (centered panel, ~960x600 in screen pixels):
//
//   ┌──────────────────────────────────────────────────────┐
//   │ [Items]  [Equipment]                                 │  tab bar
//   ├────────────────────────────┬─────────────────────────┤
//   │  ITEMS GRID  /  SLOT LIST  │  DETAIL / STAT PREVIEW  │
//   │   (left half)              │   (right half)          │
//   ├────────────────────────────┴─────────────────────────┤
//   │  Verso  HP 100/100  MP 50/50  ATK 25  DEF 10  ...    │  stats footer
//   ├──────────────────────────────────────────────────────┤
//   │  ↑↓: nav  Enter: use/equip  Tab: switch  Esc: close  │  hint footer
//   └──────────────────────────────────────────────────────┘
//
// Three-phase input FSM:
//   ITEMS_GRID        — Items tab; cursor moves over a 4xN grid
//   EQUIPMENT_SLOTS   — Equipment tab; cursor moves over slot rows
//   EQUIPMENT_PICKER  — Overlay opened from EQUIPMENT_SLOTS to pick a
//                       new item for the selected slot.  Right panel
//                       switches to a stat preview showing the delta.
//
// Tab switching:
//   Tab key cycles between Items / Equipment.  Picker is exited via
//   Esc/Backspace, never via Tab (it's a sub-state of Equipment).
//
// Persistence (THE point of this state):
//   Every action read/writes the same singletons the battle uses:
//     - Inventory     for item counts (consume on use, swap on equip)
//     - PartyManager  for current HP/MP and equipment slots
//   Equipment changes here are immediately visible in the next battle
//   because BattleManager::Initialize seeds PlayerCombatant from
//   PartyManager::GetEffectiveVersoStats().
//
// Lifetime:
//   Pushed in   -> OverworldState::Update on 'I' key
//   Popped via  -> Esc / I / Backspace from any phase (picker exits
//                  to slots first, then a second press closes the state)
// ============================================================
#pragma once
#include "IGameState.h"
#include "../Renderer/NineSliceRenderer.h"
#include "../UI/BattleTextRenderer.h"
#include "../UI/ScrollArrowRenderer.h"
#include "../Battle/ItemData.h"   // ItemKind / EquipSlot
#include <DirectXMath.h>           // XMVECTOR for IconTintFor return type
#include <vector>
#include <string>

class InventoryState : public IGameState
{
public:
    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "InventoryState"; }

private:
    // ============================================================
    //  State machine
    // ============================================================
    enum class Tab
    {
        Items,
        Equipment
    };

    enum class Phase
    {
        ItemsGrid,         // navigating consumable grid (Items tab)
        EquipmentSlots,    // navigating equipment slot list (Equipment tab)
        EquipmentPicker    // overlay: picking an item to equip
    };

    // ============================================================
    //  Input handlers (one per Phase)
    // ============================================================
    void HandleItemsInput();
    void HandleSlotsInput();
    void HandlePickerInput();

    // ============================================================
    //  Data refresh
    // ============================================================
    // Re-snapshot consumable list from Inventory.  Called on enter,
    // on tab switch to Items, and after using an item.
    void RefreshConsumables();

    // Re-snapshot the picker list for the current mPickerSlot from
    // Inventory, filtered to items whose equipSlot matches.
    void RefreshPicker();

    // Open the picker overlay for the slot the cursor is currently
    // on.  Snapshots the candidate list and resets mPickerCursor.
    void OpenPicker();

    // ============================================================
    //  Actions
    // ============================================================
    bool TryUseItem(const std::string& itemId);   // consumable use
    bool TryEquip(EquipSlot slot, const std::string& itemId);
    void TryUnequip(EquipSlot slot);
    void Flash(const std::string& msg);

    // ============================================================
    //  Render helpers (one per layout block)
    //
    //  Spread across THREE .cpp files to keep each one under the
    //  300-line CLAUDE.md ceiling:
    //    - InventoryStateRender.cpp        — Render() dispatch + tabs +
    //                                        items grid + equipment slots/picker
    //                                        + stats/hint footers
    //    - InventoryStateDetailPanel.cpp   — RenderDetailPanel only
    //                                        (the most complex single block)
    //    - InventoryState.cpp              — lifecycle / input / actions
    // ============================================================
    void RenderTabs(float panelX, float panelY, float panelW);
    void RenderItemsTab(float leftX, float leftY,
                         float leftW, float leftH);
    void RenderEquipmentTab(float leftX, float leftY,
                             float leftW, float leftH);
    void RenderDetailPanel(float rightX, float rightY,
                            float rightW, float rightH);
    void RenderStatsFooter(float panelX, float footerY,
                            float panelW, float footerH);
    void RenderHintFooter(float panelX, float footerY,
                           float panelW, float footerH);

    // ------------------------------------------------------------
    // IconTintFor: shared color helper used by every render file.
    //
    // Encodes ItemKind / ItemEffectKind as a tint color so the
    // placeholder squares are visually distinguishable until real
    // icon PNGs land (see idea/asset-todo.md §1.1 / §1.4).
    //
    // Promoted from a file-local helper to a static method when
    // RenderDetailPanel was extracted into its own .cpp — anonymous
    // namespace functions can't be shared across translation units.
    // ------------------------------------------------------------
    static DirectX::XMVECTOR IconTintFor(const ItemData* item, float alpha);

    // ============================================================
    //  Renderers
    // ============================================================
    NineSliceRenderer    mDialogBox;
    BattleTextRenderer   mTextRenderer;
    ScrollArrowRenderer  mChevronUp;
    ScrollArrowRenderer  mChevronDown;

    // ============================================================
    //  State
    // ============================================================
    Tab    mTab    = Tab::Items;
    Phase  mPhase  = Phase::ItemsGrid;

    // -- Items tab state --
    std::vector<std::string> mConsumables;
    int                      mItemCursor = 0;

    // -- Equipment tab state --
    int                      mSlotCursor = 0;   // 0..kEquipSlotCount-1

    // -- Picker overlay state --
    EquipSlot                mPickerSlot   = EquipSlot::None;
    std::vector<std::string> mPickerItems;       // filtered to mPickerSlot
    int                      mPickerCursor = 0;
    // The "(unequip)" pseudo-entry is the last index in the picker.
    // Selecting it strips the slot instead of equipping a new item.
    bool PickerCursorIsUnequip() const
    {
        return mPickerCursor == static_cast<int>(mPickerItems.size());
    }

    // -- Flash message (use feedback) --
    std::string  mFlashMessage;
    float        mFlashTimer    = 0.0f;
    static constexpr float kFlashDuration = 2.0f;

    // -- Animation timer (chevrons + future fades) --
    float mElapsed = 0.0f;

    // ============================================================
    //  Input edge trackers — one bool per key, fresh-press semantics
    // ============================================================
    bool mUpWasDown    = false;
    bool mDownWasDown  = false;
    bool mLeftWasDown  = false;
    bool mRightWasDown = false;
    bool mEnterWasDown = false;
    bool mEscWasDown   = false;
    bool mTabWasDown   = false;
    bool mIWasDown     = false;
    bool mBackWasDown  = false;
};
