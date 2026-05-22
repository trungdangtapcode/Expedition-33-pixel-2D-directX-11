// ============================================================
// File: InventoryStateDetailPanel.cpp
// Responsibility: Render the right-side detail / stat-preview panel
//                 for InventoryState.  Extracted from
//                 InventoryStateRender.cpp so neither file exceeds
//                 the 300-line CLAUDE.md ceiling.
//
// The panel has three modes, selected by (mTab, mPhase):
//   1. Items tab          → hovered consumable's icon + name +
//                           owned count + description
//   2. Equipment slots    → currently equipped item details +
//                           per-stat bonus list (color-coded)
//   3. Equipment picker   → STAT PREVIEW comparing current effective
//                           stats vs hypothetical with-this-item stats,
//                           every stat row green-tinted on bonus and
//                           red on penalty
//
// All math goes through PartyManager::PreviewEffectiveStats so the
// preview rule is identical to what the actual equip will produce —
// there is no chance of UI / simulation drift.
// ============================================================
#define NOMINMAX
#include <algorithm>
#include <cstdio>
#include <string>

#include "InventoryState.h"
#include "../Renderer/D3DContext.h"
#include "../Battle/ItemRegistry.h"
#include "../Battle/ItemIconCache.h"
#include "../Systems/Inventory.h"
#include "../Systems/LocalizationManager.h"
#include "../Systems/PartyManager.h"

namespace
{
    std::string LocalizedSlotLabel(EquipSlot slot)
    {
        switch (slot)
        {
        case EquipSlot::Weapon:    return LocalizationManager::Get().Text("equip.slot.weapon");
        case EquipSlot::Body:      return LocalizationManager::Get().Text("equip.slot.body");
        case EquipSlot::Head:      return LocalizationManager::Get().Text("equip.slot.head");
        case EquipSlot::Accessory: return LocalizationManager::Get().Text("equip.slot.accessory");
        case EquipSlot::None:      return LocalizationManager::Get().Text("equip.slot.none");
        }
        return LocalizationManager::Get().Text("equip.slot.none");
    }
}

void InventoryState::RenderDetailPanel(float rightX, float rightY,
                                         float rightW, float rightH)
{
    auto* ctx = D3DContext::Get().GetContext();

    // Panel background — translucent so the dark backdrop shows through.
    DirectX::XMVECTOR panelColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.55f);
    mDialogBox.Draw(ctx, rightX, rightY, rightW, rightH,
                    0.55f, DirectX::XMMatrixIdentity(), panelColor);

    constexpr float kPad = 14.0f;
    float lineY = rightY + kPad;
    auto  nextLine = [&](float dy = 22.0f) { lineY += dy; };

    // ===========================================================
    //  MODE 1 — Items tab
    // ===========================================================
    if (mTab == Tab::Items)
    {
        if (mConsumables.empty())
        {
            const std::string text = LocalizationManager::Get().Text("inventory.no_items");
            mTextRenderer.DrawString(ctx, text.c_str(),
                                      rightX + kPad, lineY, DirectX::Colors::Gray);
            return;
        }
        const std::string& id = mConsumables[mItemCursor];
        const ItemData* item  = ItemRegistry::Get().Find(id);
        if (!item) return;

        // Big icon swatch — colored frame always (effect-kind color
        // reads at a glance), real PNG overlaid when art is available.
        const float bigIcon = 96.0f;
        const float bx      = rightX + kPad;
        const float by      = lineY;
        mDialogBox.Draw(ctx, bx, by, bigIcon, bigIcon,
                        0.6f, DirectX::XMMatrixIdentity(),
                        IconTintFor(item, 0.95f));
        if (auto* srv = ItemIconCache::Get().GetIcon(item))
        {
            const float inset = bigIcon * 0.10f;
            mIconRenderer.Draw(ctx, srv,
                               bx + inset, by + inset,
                               bigIcon - inset * 2.0f, bigIcon - inset * 2.0f,
                               DirectX::XMMatrixIdentity(),
                               DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.95f));
        }

        mTextRenderer.DrawString(ctx, item->name.c_str(),
                                  rightX + kPad + bigIcon + 14.0f, lineY,
                                  DirectX::Colors::Yellow);

        // Owned count beneath the name so the player can see at a glance
        // how many they have without going back to the grid.
        const std::string ownText = LocalizationManager::Get().Format("common.owned", {
            { "count", std::to_string(Inventory::Get().GetCount(id)) }
        });
        mTextRenderer.DrawString(ctx, ownText.c_str(),
                                  rightX + kPad + bigIcon + 14.0f, lineY + 24.0f,
                                  DirectX::Colors::White);

        lineY += bigIcon + 14.0f;
        mTextRenderer.DrawString(ctx, item->description.c_str(),
                                  rightX + kPad, lineY, DirectX::Colors::White);
        return;
    }

    // ===========================================================
    //  MODE 3 — Equipment picker (STAT PREVIEW)
    //
    //  Drawn before the slot-list mode because the picker overlay
    //  takes precedence whenever it is active.
    // ===========================================================
    if (mPhase == Phase::EquipmentPicker)
    {
        const std::string statPreview = LocalizationManager::Get().Text("inventory.stat_preview");
        mTextRenderer.DrawString(ctx, statPreview.c_str(),
                                  rightX + kPad, lineY, DirectX::Colors::Yellow);
        nextLine(28.0f);

        // Current effective stats (base + currently-equipped bonuses).
        // Used as the LEFT side of every "current -> preview" row.
        BattlerStats current = PartyManager::Get().GetEffectiveStats(mMemberIndex);

        // Resolve the hypothetical id under the picker cursor.  Empty
        // string means "(unequip)" pseudo-row OR an empty list — both
        // are valid inputs to PreviewEffectiveStats.
        const std::string previewId = PickerCursorIsUnequip()
            ? std::string{}
            : (mPickerItems.empty() ? std::string{} : mPickerItems[mPickerCursor]);

        // PreviewEffectiveStats runs the SAME fold the real Equip path
        // would run — there is no chance of preview vs simulation drift.
        const BattlerStats preview = PartyManager::Get().PreviewEffectiveStats(mMemberIndex, mPickerSlot, previewId);

        // Helper: draw "label  current -> preview (delta)" with a color
        // hint.  No-change rows print in white without arrows so the
        // player's eye lands only on rows that actually changed.
        auto drawStatRow = [&](const char* label, int cur, int next)
        {
            const int delta = next - cur;
            char line[64];
            if (delta == 0)
            {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "%-8s %d", label, cur);
                mTextRenderer.DrawString(ctx, line,
                                          rightX + kPad, lineY, DirectX::Colors::White);
            }
            else
            {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "%-8s %d -> %d  (%+d)",
                            label, cur, next, delta);
                DirectX::XMVECTOR color = (delta > 0)
                    ? DirectX::Colors::LightGreen
                    : DirectX::Colors::Red;
                mTextRenderer.DrawString(ctx, line, rightX + kPad, lineY, color);
            }
            nextLine(22.0f);
        };

        drawStatRow("MaxHP", current.maxHp, preview.maxHp);
        drawStatRow("MaxMP", current.maxMp, preview.maxMp);
        drawStatRow("ATK",   current.atk,   preview.atk);
        drawStatRow("DEF",   current.def,   preview.def);
        drawStatRow("MATK",  current.matk,  preview.matk);
        drawStatRow("MDEF",  current.mdef,  preview.mdef);
        drawStatRow("SPD",   current.spd,   preview.spd);

        nextLine(8.0f);
        const ItemData* hoverItem = previewId.empty()
            ? nullptr
            : ItemRegistry::Get().Find(previewId);
        if (hoverItem)
        {
            mTextRenderer.DrawString(ctx, hoverItem->description.c_str(),
                                      rightX + kPad, lineY, DirectX::Colors::Gray);
        }
        else if (PickerCursorIsUnequip())
        {
            const std::string removesText = LocalizationManager::Get().Text("inventory.removes_current_item");
            mTextRenderer.DrawString(ctx, removesText.c_str(),
                                      rightX + kPad, lineY, DirectX::Colors::Gray);
        }
        return;
    }

    // ===========================================================
    //  MODE 2 — Equipment slot list (no picker active)
    // ===========================================================
    constexpr EquipSlot order[kEquipSlotCount] = {
        EquipSlot::Weapon, EquipSlot::Body, EquipSlot::Head, EquipSlot::Accessory
    };
    const EquipSlot   slot = order[mSlotCursor];
    const std::string equippedId = PartyManager::Get().GetEquippedItem(mMemberIndex, slot);
    const ItemData*   item = equippedId.empty() ? nullptr : ItemRegistry::Get().Find(equippedId);

    const std::string slotHeader = LocalizationManager::Get().Format("inventory.slot_header", {
        { "slot", LocalizedSlotLabel(slot) }
    });
    mTextRenderer.DrawString(ctx, slotHeader.c_str(), rightX + kPad, lineY,
                              DirectX::Colors::Yellow);
    nextLine(28.0f);

    if (!item)
    {
        const std::string emptyText = LocalizationManager::Get().Text("inventory.nothing_equipped");
        mTextRenderer.DrawString(ctx, emptyText.c_str(),
                                  rightX + kPad, lineY, DirectX::Colors::Gray);
        nextLine();
        const std::string chooseText = LocalizationManager::Get().Text("inventory.choose_item");
        mTextRenderer.DrawString(ctx, chooseText.c_str(),
                                  rightX + kPad, lineY, DirectX::Colors::Gray);
        return;
    }

    mTextRenderer.DrawString(ctx, item->name.c_str(),
                              rightX + kPad, lineY, DirectX::Colors::White);
    nextLine();
    mTextRenderer.DrawString(ctx, item->description.c_str(),
                              rightX + kPad, lineY, DirectX::Colors::Gray);
    nextLine(28.0f);

    // Inline bonus list — only non-zero rows so a sword with only ATK +5
    // shows one line, not a wall of zeros.
    auto drawBonus = [&](const char* label, int v)
    {
        if (v == 0) return;
        char buf[32];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s %+d", label, v);
        DirectX::XMVECTOR color = (v > 0) ? DirectX::Colors::LightGreen : DirectX::Colors::Red;
        mTextRenderer.DrawString(ctx, buf, rightX + kPad, lineY, color);
        nextLine(20.0f);
    };
    drawBonus("MaxHP", item->bonusMaxHp);
    drawBonus("MaxMP", item->bonusMaxMp);
    drawBonus("ATK",   item->bonusAtk);
    drawBonus("DEF",   item->bonusDef);
    drawBonus("MATK",  item->bonusMatk);
    drawBonus("MDEF",  item->bonusMdef);
    drawBonus("SPD",   item->bonusSpd);
}
