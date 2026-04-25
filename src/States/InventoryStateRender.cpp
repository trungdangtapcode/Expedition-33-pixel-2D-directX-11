// ============================================================
// File: InventoryStateRender.cpp
// Responsibility: Pure draw-call helpers for InventoryState.
//                 Split out from InventoryState.cpp to stay under
//                 the 300-line per-file ceiling.
//
// All methods are screen-space (Identity transform).  No member
// state changes — these only read mTab/mPhase/cursors and submit
// SpriteBatch + text draws.
// ============================================================
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "InventoryState.h"
#include "../Renderer/D3DContext.h"
#include "../Battle/ItemRegistry.h"
#include "../Systems/Inventory.h"
#include "../Systems/PartyManager.h"

// ------------------------------------------------------------
// IconTintFor (static, defined here so InventoryStateDetailPanel.cpp
// can call it via the InventoryState class scope).
//
// Encodes the item's kind / effect as a tint color.  Equipment uses
// material-family colors (bronze/violet/steel/gold) so the equipment
// tab's slot rows scan visually; consumables fall through to the
// effect-kind palette (green=heal, red=damage, etc.) shared with the
// battle item menu so the same item looks the same everywhere.
//
// Goes away when real per-item icon PNGs land — both render files
// will then call SpriteBatch::Draw on item->iconSRV directly.
// ------------------------------------------------------------
DirectX::XMVECTOR InventoryState::IconTintFor(const ItemData* item, float alpha)
{
    if (!item) return DirectX::XMVectorSet(0.6f, 0.6f, 0.6f, alpha);
    switch (item->kind)
    {
    case ItemKind::Weapon:    return DirectX::XMVectorSet(0.85f, 0.55f, 0.30f, alpha); // bronze
    case ItemKind::BodyArmor: return DirectX::XMVectorSet(0.55f, 0.45f, 0.85f, alpha); // violet
    case ItemKind::Helmet:    return DirectX::XMVectorSet(0.60f, 0.60f, 0.70f, alpha); // steel
    case ItemKind::Accessory: return DirectX::XMVectorSet(0.95f, 0.85f, 0.30f, alpha); // gold
    case ItemKind::KeyItem:   return DirectX::XMVectorSet(0.75f, 0.75f, 0.75f, alpha); // grey
    case ItemKind::Consumable: break;
    }
    switch (item->effect)
    {
    case ItemEffectKind::HealHp:
    case ItemEffectKind::FullHeal:    return DirectX::XMVectorSet(0.40f, 0.85f, 0.40f, alpha);
    case ItemEffectKind::HealMp:      return DirectX::XMVectorSet(0.40f, 0.55f, 0.95f, alpha);
    case ItemEffectKind::Revive:      return DirectX::XMVectorSet(0.95f, 0.55f, 0.75f, alpha);
    case ItemEffectKind::RestoreRage: return DirectX::XMVectorSet(1.00f, 0.55f, 0.20f, alpha);
    case ItemEffectKind::DealDamage:  return DirectX::XMVectorSet(0.95f, 0.30f, 0.30f, alpha);
    case ItemEffectKind::StatBuff:    return DirectX::XMVectorSet(0.95f, 0.90f, 0.35f, alpha);
    case ItemEffectKind::Cleanse:     return DirectX::XMVectorSet(0.45f, 0.90f, 0.95f, alpha);
    }
    return DirectX::XMVectorSet(0.6f, 0.6f, 0.6f, alpha);
}

// ------------------------------------------------------------
// RenderTabs
// ------------------------------------------------------------
void InventoryState::RenderTabs(float panelX, float panelY, float panelW)
{
    auto* ctx = D3DContext::Get().GetContext();

    constexpr float kTabW    = 140.0f;
    constexpr float kTabH    = 36.0f;
    constexpr float kTabGap  = 6.0f;
    const     float startX   = panelX + 24.0f;
    const     float startY   = panelY + 8.0f;

    const char* labels[] = { "Items", "Equipment" };
    const Tab   tabs[]   = { Tab::Items, Tab::Equipment };

    for (int i = 0; i < 2; ++i)
    {
        const float tabX = startX + i * (kTabW + kTabGap);
        const bool active = (mTab == tabs[i]);

        DirectX::XMVECTOR tabColor = active
            ? DirectX::XMVectorSet(1.0f, 0.95f, 0.55f, 0.95f)   // active = gold
            : DirectX::XMVectorSet(1.0f, 1.0f,  1.0f,  0.55f);  // inactive = faded

        mDialogBox.Draw(ctx, tabX, startY, kTabW, kTabH,
                        0.5f, DirectX::XMMatrixIdentity(), tabColor);

        DirectX::XMVECTOR textColor = active
            ? DirectX::Colors::Black
            : DirectX::Colors::White;
        mTextRenderer.DrawString(ctx, labels[i],
                                  tabX + 36.0f, startY + 8.0f, textColor);
    }

    // "Tab to switch" hint, top-right of the tab bar.
    mTextRenderer.DrawString(ctx, "[Tab] Switch",
                              panelX + panelW - 140.0f, startY + 12.0f,
                              DirectX::Colors::Gray);
}

// ------------------------------------------------------------
// RenderItemsTab — 4-column grid of consumables
// ------------------------------------------------------------
void InventoryState::RenderItemsTab(float leftX, float leftY,
                                      float leftW, float leftH)
{
    auto* ctx = D3DContext::Get().GetContext();

    if (mConsumables.empty())
    {
        mTextRenderer.DrawString(ctx, "(no consumables)",
                                  leftX, leftY + leftH * 0.5f,
                                  DirectX::Colors::Gray);
        return;
    }

    // ------------------------------------------------------------
    // Grid layout + sliding window
    //
    //   - Fixed 4 columns; visible window is kVisibleRows tall (3 rows
    //     => 12 cells visible, matching the screenshot the player saw).
    //   - When the inventory has more than 12 consumables, the window
    //     scrolls to keep the cursor's row in the middle whenever possible
    //     (same centering rule the battle item menu uses).
    //   - Cells outside the window are NOT submitted to the GPU — this
    //     keeps draw-call count constant regardless of inventory size.
    //   - A scrollbar (track + thumb) plus up/down chevron sprites are
    //     drawn alongside the grid when the list overflows the window.
    //
    // Why centering rather than "thumb-follows" scroll:
    //   Centering makes Up/Down feel symmetric and predictable.  The
    //   battle item menu uses the same rule, so muscle memory transfers.
    // ------------------------------------------------------------
    constexpr int   kCols        = 4;
    constexpr int   kVisibleRows = 3;       // 3 × 4 = 12 cells visible
    constexpr float kCellGap     = 10.0f;
    const     float cellSize     = (leftW - kCellGap * (kCols - 1)) / kCols;

    const int n         = static_cast<int>(mConsumables.size());
    const int totalRows = (n + kCols - 1) / kCols;            // ceil(n / cols)
    const int cursorRow = mItemCursor / kCols;

    // Sliding window: clamp(cursorRow - center, 0, totalRows - kVisibleRows)
    int firstRow = cursorRow - (kVisibleRows / 2);
    const int maxFirst = (std::max)(0, totalRows - kVisibleRows);
    if (firstRow < 0)        firstRow = 0;
    if (firstRow > maxFirst) firstRow = maxFirst;
    const int lastRow = (std::min)(totalRows, firstRow + kVisibleRows);  // exclusive

    // Helper alpha used by every cell so the scrollbar / chevrons match.
    constexpr float kAlpha = 0.95f;

    for (int row = firstRow; row < lastRow; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            const int i = row * kCols + col;
            if (i >= n) break;   // partial last row — fewer than kCols cells

            // Position uses the WINDOW row, not the absolute row, so
            // visible content always starts at leftY regardless of scroll.
            const int   rowInWindow = row - firstRow;
            const float cellX = leftX + col * (cellSize + kCellGap);
            const float cellY = leftY + rowInWindow * (cellSize + kCellGap);

            const ItemData* item    = ItemRegistry::Get().Find(mConsumables[i]);
            const int       count   = Inventory::Get().GetCount(mConsumables[i]);
            const bool      hovered = (i == mItemCursor && mPhase == Phase::ItemsGrid);

            DirectX::XMVECTOR cellColor = hovered
                ? DirectX::XMVectorSet(1.0f, 0.95f, 0.55f, kAlpha)
                : DirectX::XMVectorSet(1.0f, 1.0f,  1.0f,  0.78f);
            mDialogBox.Draw(ctx, cellX, cellY, cellSize, cellSize,
                            0.4f, DirectX::XMMatrixIdentity(), cellColor);

            // Icon placeholder — small inset square tinted by effect kind.
            const float iconPad  = cellSize * 0.18f;
            const float iconSize = cellSize - iconPad * 2.0f - 14.0f;
            mDialogBox.Draw(ctx, cellX + iconPad, cellY + iconPad,
                            iconSize, iconSize, 0.3f,
                            DirectX::XMMatrixIdentity(),
                            IconTintFor(item, kAlpha));

            char countStr[16];
            _snprintf_s(countStr, sizeof(countStr), _TRUNCATE, "x%d", count);
            mTextRenderer.DrawString(ctx, countStr,
                                      cellX + cellSize * 0.55f,
                                      cellY + cellSize - 18.0f,
                                      hovered ? DirectX::Colors::Black : DirectX::Colors::White);
        }
    }

    // ------------------------------------------------------------
    // Scroll affordances (only when content overflows the window).
    //
    //   - Up chevron sprite above the grid when firstRow > 0
    //   - Down chevron sprite below the grid when lastRow < totalRows
    //   - Vertical scrollbar (track + thumb) on the right edge:
    //       track height = full grid height
    //       thumb height = (kVisibleRows / totalRows) * trackHeight
    //                      (clamped to a minimum so it stays visible)
    //       thumb pos    = (firstRow / (totalRows - kVisibleRows)) * remainder
    //
    // Skipped entirely when totalRows <= kVisibleRows because there's
    // nothing to indicate — drawing them anyway is visual noise.
    // ------------------------------------------------------------
    if (totalRows > kVisibleRows)
    {
        const float gridHeight  = kVisibleRows * cellSize + (kVisibleRows - 1) * kCellGap;
        const float gridRight   = leftX + leftW;

        // Chevron size derived from cellSize so it scales with the grid.
        const float chevronSize = cellSize * 0.42f;
        const int   srcW        = mChevronDown.GetWidth();
        const float chevronScale = (srcW > 0) ? chevronSize / static_cast<float>(srcW) : 1.0f;
        const float chevronX    = leftX + leftW * 0.5f;
        DirectX::XMVECTOR chevronColor = DirectX::XMVectorSet(0.95f, 0.95f, 0.95f, kAlpha);

        if (firstRow > 0)
        {
            mChevronUp.Draw(ctx, chevronX, leftY - chevronSize * 0.5f,
                            true, chevronScale,
                            DirectX::XMMatrixIdentity(), chevronColor);
        }
        if (lastRow < totalRows)
        {
            mChevronDown.Draw(ctx, chevronX, leftY + gridHeight + chevronSize * 0.5f,
                              false, chevronScale,
                              DirectX::XMMatrixIdentity(), chevronColor);
        }

        // Scrollbar — thin vertical strip flush against the right edge.
        const float trackW = 8.0f;
        const float trackX = gridRight + 4.0f;
        const float trackY = leftY;
        const float trackH = gridHeight;

        DirectX::XMVECTOR trackColor =
            DirectX::XMVectorSet(0.30f, 0.30f, 0.30f, kAlpha * 0.85f);
        mDialogBox.Draw(ctx, trackX, trackY, trackW, trackH,
                        0.5f, DirectX::XMMatrixIdentity(), trackColor);

        // Thumb proportions (browser scrollbar semantics).
        const float ratio = static_cast<float>(kVisibleRows) / static_cast<float>(totalRows);
        const float thumbH    = (std::max)(trackH * ratio, cellSize * 0.30f);
        const int   denom     = (std::max)(1, totalRows - kVisibleRows);
        const float progress  = static_cast<float>(firstRow) / static_cast<float>(denom);
        const float thumbY    = trackY + (trackH - thumbH) * progress;

        DirectX::XMVECTOR thumbColor =
            DirectX::XMVectorSet(0.95f, 0.85f, 0.30f, kAlpha);
        mDialogBox.Draw(ctx, trackX, thumbY, trackW, thumbH,
                        0.5f, DirectX::XMMatrixIdentity(), thumbColor);
    }
}

// ------------------------------------------------------------
// RenderEquipmentTab — slot list (4 rows) OR picker overlay
// ------------------------------------------------------------
void InventoryState::RenderEquipmentTab(float leftX, float leftY,
                                          float leftW, float leftH)
{
    auto* ctx = D3DContext::Get().GetContext();

    constexpr EquipSlot order[kEquipSlotCount] = {
        EquipSlot::Weapon, EquipSlot::Body, EquipSlot::Head, EquipSlot::Accessory
    };

    if (mPhase == Phase::EquipmentPicker)
    {
        // ---- Picker overlay ----
        char header[64];
        _snprintf_s(header, sizeof(header), _TRUNCATE, "Choose %s:", SlotLabel(mPickerSlot));
        mTextRenderer.DrawString(ctx, header, leftX, leftY,
                                  DirectX::Colors::Yellow);

        const float rowH          = 36.0f;
        const float rowGap        = 6.0f;
        const float headerH       = 28.0f;
        const int   itemCount     = static_cast<int>(mPickerItems.size());
        const int   totalRows     = itemCount + 1; // +1 for "(unequip slot)"
        const int   visibleRows   = (std::max)(1, static_cast<int>((leftH - headerH + rowGap) / (rowH + rowGap)));
        const int   maxFirst      = (std::max)(0, totalRows - visibleRows);

        int firstRow = mPickerCursor - (visibleRows / 2);
        if (firstRow < 0)        firstRow = 0;
        if (firstRow > maxFirst) firstRow = maxFirst;
        const int lastRow = (std::min)(totalRows, firstRow + visibleRows); // exclusive

        for (int row = firstRow; row < lastRow; ++row)
        {
            const float rowY = leftY + headerH + (row - firstRow) * (rowH + rowGap);
            const bool  hovered = (row == mPickerCursor);
            const bool  isUnequipRow = (row == itemCount);

            const ItemData* item = (!isUnequipRow && row < itemCount)
                ? ItemRegistry::Get().Find(mPickerItems[row])
                : nullptr;

            DirectX::XMVECTOR rowColor;
            if (isUnequipRow)
            {
                rowColor = hovered
                    ? DirectX::XMVectorSet(0.85f, 0.55f, 0.55f, 0.95f)
                    : DirectX::XMVectorSet(0.4f,  0.4f,  0.4f,  0.55f);
            }
            else
            {
                rowColor = hovered
                    ? DirectX::XMVectorSet(1.0f, 0.95f, 0.55f, 0.95f)
                    : DirectX::XMVectorSet(1.0f, 1.0f,  1.0f,  0.78f);
            }
            mDialogBox.Draw(ctx, leftX, rowY, leftW, rowH,
                            0.45f, DirectX::XMMatrixIdentity(), rowColor);

            if (!isUnequipRow)
            {
                mDialogBox.Draw(ctx, leftX + 6.0f, rowY + 6.0f,
                                rowH - 12.0f, rowH - 12.0f, 0.3f,
                                DirectX::XMMatrixIdentity(),
                                IconTintFor(item, 0.95f));

                const std::string label = item ? item->name : mPickerItems[row];
                mTextRenderer.DrawString(ctx, label.c_str(),
                                          leftX + rowH + 4.0f, rowY + 8.0f,
                                          hovered ? DirectX::Colors::Black : DirectX::Colors::White);
            }
            else
            {
                mTextRenderer.DrawString(ctx, "(unequip slot)",
                                          leftX + 12.0f, rowY + 8.0f,
                                          hovered ? DirectX::Colors::White : DirectX::Colors::Gray);
            }
        }

        if (totalRows > visibleRows)
        {
            const float listHeight   = visibleRows * rowH + (visibleRows - 1) * rowGap;
            const float chevronSize  = rowH * 0.75f;
            const int   srcW         = mChevronDown.GetWidth();
            const float chevronScale = (srcW > 0) ? chevronSize / static_cast<float>(srcW) : 1.0f;
            const float chevronX     = leftX + leftW * 0.5f;

            if (firstRow > 0)
            {
                mChevronUp.Draw(ctx, chevronX, leftY + headerH - chevronSize * 0.55f,
                                true, chevronScale,
                                DirectX::XMMatrixIdentity(),
                                DirectX::XMVectorSet(0.95f, 0.95f, 0.95f, 0.95f));
            }
            if (lastRow < totalRows)
            {
                mChevronDown.Draw(ctx, chevronX, leftY + headerH + listHeight + chevronSize * 0.55f,
                                  false, chevronScale,
                                  DirectX::XMMatrixIdentity(),
                                  DirectX::XMVectorSet(0.95f, 0.95f, 0.95f, 0.95f));
            }

            const float trackW    = 8.0f;
            const float trackX    = leftX + leftW + 4.0f;
            const float trackY    = leftY + headerH;
            const float trackH    = listHeight;
            const float ratio     = static_cast<float>(visibleRows) / static_cast<float>(totalRows);
            const float thumbH    = (std::max)(trackH * ratio, rowH * 0.8f);
            const int   denom     = (std::max)(1, totalRows - visibleRows);
            const float progress  = static_cast<float>(firstRow) / static_cast<float>(denom);
            const float thumbY    = trackY + (trackH - thumbH) * progress;

            mDialogBox.Draw(ctx, trackX, trackY, trackW, trackH,
                            0.5f, DirectX::XMMatrixIdentity(),
                            DirectX::XMVectorSet(0.30f, 0.30f, 0.30f, 0.80f));
            mDialogBox.Draw(ctx, trackX, thumbY, trackW, thumbH,
                            0.5f, DirectX::XMMatrixIdentity(),
                            DirectX::XMVectorSet(0.95f, 0.85f, 0.30f, 0.95f));
        }
        return;
    }

    // ---- Slot list ----
    mTextRenderer.DrawString(ctx, "VERSO  - Equipment", leftX, leftY,
                              DirectX::Colors::Yellow);

    const float rowH = 56.0f;
    const float rowGap = 8.0f;

    for (int i = 0; i < kEquipSlotCount; ++i)
    {
        const float rowY = leftY + 28.0f + i * (rowH + rowGap);
        const bool hovered = (i == mSlotCursor);

        DirectX::XMVECTOR rowColor = hovered
            ? DirectX::XMVectorSet(1.0f, 0.95f, 0.55f, 0.95f)
            : DirectX::XMVectorSet(1.0f, 1.0f,  1.0f,  0.78f);
        mDialogBox.Draw(ctx, leftX, rowY, leftW, rowH,
                        0.45f, DirectX::XMMatrixIdentity(), rowColor);

        std::string equippedId = PartyManager::Get().GetEquippedItem(0, order[i]);
        const ItemData* item = equippedId.empty() ? nullptr : ItemRegistry::Get().Find(equippedId);

        // Slot label
        char slotText[64];
        _snprintf_s(slotText, sizeof(slotText), _TRUNCATE, "%s:", SlotLabel(order[i]));
        mTextRenderer.DrawString(ctx, slotText,
                                  leftX + 12.0f, rowY + 8.0f,
                                  hovered ? DirectX::Colors::Black : DirectX::Colors::White);

        // Equipped item name (or "(empty)")
        const char* itemName = item ? item->name.c_str() : "(empty)";
        mTextRenderer.DrawString(ctx, itemName,
                                  leftX + 130.0f, rowY + 8.0f,
                                  hovered ? DirectX::Colors::Black : DirectX::Colors::Gray);

        // Inline icon swatch on the right edge
        if (item)
        {
            mDialogBox.Draw(ctx, leftX + leftW - rowH, rowY + 6.0f,
                            rowH - 12.0f, rowH - 12.0f, 0.3f,
                            DirectX::XMMatrixIdentity(),
                            IconTintFor(item, 0.95f));
        }
    }
}

// ------------------------------------------------------------
// RenderDetailPanel lives in InventoryStateDetailPanel.cpp — it was
// extracted to keep this file under the 300-line CLAUDE.md ceiling.
// ------------------------------------------------------------

// ------------------------------------------------------------
// RenderStatsFooter — single line of effective Verso stats.
// Always shows EFFECTIVE values (with current equipment), not base.
// ------------------------------------------------------------
void InventoryState::RenderStatsFooter(float panelX, float footerY,
                                         float panelW, float footerH)
{
    auto* ctx = D3DContext::Get().GetContext();
    const BattlerStats s = PartyManager::Get().GetEffectiveStats(0);

    char line[256];
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                "VERSO   HP %d/%d   MP %d/%d   ATK %d   DEF %d   MATK %d   MDEF %d   SPD %d",
                s.hp, s.maxHp, s.mp, s.maxMp,
                s.atk, s.def, s.matk, s.mdef, s.spd);
    mTextRenderer.DrawString(ctx, line,
                              panelX + 24.0f, footerY + footerH * 0.30f,
                              DirectX::Colors::White);
}

// ------------------------------------------------------------
// RenderHintFooter — bottom hint line + flash message.
// ------------------------------------------------------------
void InventoryState::RenderHintFooter(float panelX, float footerY,
                                        float panelW, float footerH)
{
    auto* ctx = D3DContext::Get().GetContext();

    if (mFlashTimer > 0.0f && !mFlashMessage.empty())
    {
        const float alpha = (std::min)(mFlashTimer / kFlashDuration, 1.0f);
        DirectX::XMVECTOR color = DirectX::XMVectorSet(1.0f, 0.95f, 0.45f, alpha);
        mTextRenderer.DrawString(ctx, mFlashMessage.c_str(),
                                  panelX + 24.0f, footerY + footerH * 0.20f, color);
        return;
    }

    const char* hint = nullptr;
    switch (mPhase)
    {
    case Phase::ItemsGrid:
        hint = "Arrows: navigate   Enter: use   Tab: switch tab   Esc/I: close";
        break;
    case Phase::EquipmentSlots:
        hint = "Up/Down: select slot   Enter: change item   Tab: switch tab   Esc/I: close";
        break;
    case Phase::EquipmentPicker:
        hint = "Up/Down: pick item   Enter: equip   Esc/Backspace: cancel";
        break;
    }
    if (hint)
    {
        mTextRenderer.DrawString(ctx, hint,
                                  panelX + 24.0f, footerY + footerH * 0.20f,
                                  DirectX::Colors::Gray);
    }
}
