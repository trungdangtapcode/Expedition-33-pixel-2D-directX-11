// ============================================================
// File: LineupStateRender.cpp
// Responsibility: Cinematic rendering for Party Lineup screen.
//
// DESIGN: Characters ARE the art. No UI chrome, no dialog boxes.
//
// All visual values come from three data sources:
//   - LineupCameraController: camera zoom, rotation, position, fade alpha
//   - CharacterSlotAnim[i]:   per-character scale, glow alpha, Y offset
//   - FontConfig:             5 semantic font scale categories from JSON
//
// The renderer NEVER computes instant values or uses hardcoded scales.
//
// RENDERING ORDER (3 strict passes, no interleaving):
//   Pass 1 — CircleRenderer:       SDF glow circles
//   Pass 2 — WorldSpriteRenderer:  character sprites (tilted camera)
//   Pass 3 — BattleTextRenderer:   all text (screen space)
// ============================================================
#define NOMINMAX
#include <algorithm>
#include <cstdio>
#include <cmath>

#include "LineupState.h"
#include "../Renderer/D3DContext.h"
#include "../Battle/ItemRegistry.h"
#include "../Battle/ItemData.h"
#include "../Systems/Inventory.h"
#include "../Systems/PartyManager.h"
#include "../Utils/Log.h"

// ============================================================
//  LINEUP VIEW — cinematic character showcase
// ============================================================

void LineupState::RenderLineupView()
{
    auto& d3d = D3DContext::Get();
    ID3D11DeviceContext* ctx = d3d.GetContext();
    const int W = d3d.GetWidth();
    const int H = d3d.GetHeight();
    const float fW = static_cast<float>(W);
    const float fH = static_cast<float>(H);

    if (mPartySize == 0) return;

    const auto& party = PartyManager::Get().GetActiveParty();
    const float sceneAlpha = mCameraCtrl.GetFadeAlpha();

    // Character positioning: evenly distributed, centered.
    const float spacing = mLineupLayout.characterSpacing;
    const float totalW  = (mPartySize - 1) * spacing;
    const float baseX   = (fW - totalW) * 0.5f;
    const float floorY  = fH * mLineupLayout.floorRatio;

    // Shorthand references to font scales.
    const float fTitle   = mFontConfig.titleScale;
    const float fSection = mFontConfig.sectionScale;
    const float fName    = mFontConfig.nameScale;
    const float fBody    = mFontConfig.bodyScale;
    const float fSmall   = mFontConfig.smallScale;

    // ================================================================
    //  PASS 1 + 2 combined positioning:
    //  Both glow and sprite derive from the SAME world position.
    //  Glow (screen-space) uses WorldToScreen() on the sprite's world pos.
    //  This way glow ALWAYS tracks the sprite regardless of camera state.
    // ================================================================
    Camera2D& cam = mCameraCtrl.GetCamera();

    // Pre-compute world positions and screen positions for all characters.
    struct CharPos {
        float worldX, worldY;   // input to sprite Draw()
        float screenX, screenY; // output of WorldToScreen() — used for glow
    };
    std::vector<CharPos> charPositions(mPartySize);

    for (int i = 0; i < mPartySize; ++i)
    {
        if (i >= static_cast<int>(mSlotAnims.size())) break;
        const float cx = baseX + i * spacing;
        const float spriteY = floorY + mSlotAnims[i].currentOffsetY;

        charPositions[i].worldX = cx - fW * 0.5f;
        charPositions[i].worldY = spriteY - fH * 0.5f;

        // Project through camera to get the actual screen position.
        DirectX::XMFLOAT2 screenPos = cam.WorldToScreen(
            charPositions[i].worldX, charPositions[i].worldY);
        charPositions[i].screenX = screenPos.x;
        charPositions[i].screenY = screenPos.y;
    }

    // ================================================================
    //  PASS 1 — Atmospheric glow (CircleRenderer, screen space)
    //  Glow position DERIVED from sprite's screen position + offset.
    // ================================================================
    for (int i = 0; i < mPartySize; ++i)
    {
        if (i >= static_cast<int>(mSlotAnims.size())) break;

        const float glowAlpha = mSlotAnims[i].currentGlowAlpha * sceneAlpha;
        if (glowAlpha < 0.01f) continue;

        // Glow is positioned relative to where the sprite actually appears.
        const float glowX = charPositions[i].screenX;
        const float glowY = charPositions[i].screenY + mLineupLayout.glowOffsetY;

        float pulse = 0.6f + 0.25f * sinf(mElapsed * 1.8f);
        float baseRadius = mLineupLayout.glowRadius * mSlotAnims[i].currentScale / mLineupLayout.selectedScale;

        mGlow.Draw(ctx, glowX, glowY, baseRadius * 2.2f * pulse,
                   0.08f * glowAlpha, 0.06f * glowAlpha, 0.18f * glowAlpha, W, H);
        mGlow.Draw(ctx, glowX, glowY, baseRadius * 1.4f * pulse,
                   0.1f * glowAlpha, 0.1f * glowAlpha, 0.25f * glowAlpha, W, H);
        mGlow.Draw(ctx, glowX, glowY, baseRadius * 0.8f * pulse,
                   0.12f * glowAlpha, 0.15f * glowAlpha, 0.35f * glowAlpha, W, H);
    }

    // ================================================================
    //  PASS 2 — Character sprites (world space through camera)
    //  Uses the same world positions computed above.
    // ================================================================
    for (int i = 0; i < mPartySize; ++i)
    {
        if (i >= static_cast<int>(mSprites.size()) || !mSprites[i]) continue;
        if (i >= static_cast<int>(mSlotAnims.size())) break;

        float scale = mSlotAnims[i].currentScale;

        // Subtle breathing on selected character.
        if (mSlotAnims[i].currentGlowAlpha > 0.5f)
            scale += 0.06f * sinf(mElapsed * 1.2f) * mSlotAnims[i].currentGlowAlpha;

        mSprites[i]->Draw(ctx, cam,
            charPositions[i].worldX, charPositions[i].worldY,
            scale, false);
    }

    // ================================================================
    //  PASS 3 — Text overlays (screen space)
    // ================================================================
    if (sceneAlpha > 0.1f)
    {
        // Title
        {
            DirectX::XMMATRIX t = DirectX::XMMatrixScaling(fTitle, fTitle, 1.0f);
            mTextRenderer.DrawString(ctx, "Team",
                30.0f / fTitle, 20.0f / fTitle,
                DirectX::XMVectorSet(0.9f, 0.88f, 0.82f, sceneAlpha), t);
        }

        // Subtitle
        {
            DirectX::XMMATRIX t = DirectX::XMMatrixScaling(fSmall, fSmall, 1.0f);
            mTextRenderer.DrawString(ctx, "Party Lineup",
                30.0f / fSmall, (22.0f + fTitle * 22.0f) / fSmall,
                DirectX::XMVectorSet(0.45f, 0.42f, 0.4f, sceneAlpha), t);
        }

        // Character names + level/HP
        for (int i = 0; i < mPartySize; ++i)
        {
            if (i >= static_cast<int>(mSlotAnims.size())) break;

            const float cx = baseX + i * spacing;
            const PartyMember& member = party[i];
            const BattlerStats stats = PartyManager::Get().GetEffectiveStats(i);
            const float textY = floorY + mSlotAnims[i].currentOffsetY + 30.0f;

            // Name — brightness lerps with glow alpha.
            float brightness = 0.5f + 0.5f * mSlotAnims[i].currentGlowAlpha;
            float nameS = fName * (0.7f + 0.3f * mSlotAnims[i].currentGlowAlpha);
            {
                DirectX::XMMATRIX nt = DirectX::XMMatrixScaling(nameS, nameS, 1.0f);
                float r = 0.5f + 0.5f * brightness;
                float g = 0.48f + 0.5f * brightness;
                float b = 0.45f + 0.47f * brightness;
                mTextRenderer.DrawStringCentered(ctx, member.name.c_str(),
                    cx / nameS, textY / nameS,
                    DirectX::XMVectorSet(r, g, b, sceneAlpha), nt);
            }

            // Level + HP
            {
                char infoBuf[64];
                _snprintf_s(infoBuf, sizeof(infoBuf), _TRUNCATE,
                            "Lv.%d  HP %d/%d", stats.level, stats.hp, stats.maxHp);
                float infoS = fSmall * (0.8f + 0.2f * mSlotAnims[i].currentGlowAlpha);
                DirectX::XMMATRIX it = DirectX::XMMatrixScaling(infoS, infoS, 1.0f);

                float dimFactor = 0.3f + 0.7f * mSlotAnims[i].currentGlowAlpha;
                DirectX::XMVECTOR infoColor;
                if (stats.hp >= stats.maxHp)
                    infoColor = DirectX::XMVectorSet(0.85f * dimFactor, 0.75f * dimFactor, 0.4f * dimFactor, sceneAlpha);
                else
                {
                    float ratio = (stats.maxHp > 0) ? static_cast<float>(stats.hp) / stats.maxHp : 0.0f;
                    infoColor = (ratio > 0.5f)
                        ? DirectX::XMVectorSet(0.4f * dimFactor, 0.85f * dimFactor, 0.4f * dimFactor, sceneAlpha)
                        : DirectX::XMVectorSet(1.0f * dimFactor, 0.35f * dimFactor, 0.3f * dimFactor, sceneAlpha);
                }
                mTextRenderer.DrawStringCentered(ctx, infoBuf,
                    cx / infoS, (textY + 28.0f) / infoS, infoColor, it);
            }
        }

        // Navigation arrows
        if (mPartySize > 1)
        {
            float arrowPulse = 0.3f + 0.2f * sinf(mElapsed * 2.5f);
            DirectX::XMVECTOR arrowColor = DirectX::XMVectorSet(0.6f, 0.55f, 0.5f, arrowPulse * sceneAlpha);
            DirectX::XMMATRIX at = DirectX::XMMatrixScaling(fTitle, fTitle, 1.0f);
            mTextRenderer.DrawStringCentered(ctx, "<",
                35.0f / fTitle, (fH * 0.42f) / fTitle, arrowColor, at);
            mTextRenderer.DrawStringCentered(ctx, ">",
                (fW - 35.0f) / fTitle, (fH * 0.42f) / fTitle, arrowColor, at);
        }

        // Bottom hints
        {
            DirectX::XMMATRIX ht = DirectX::XMMatrixScaling(fSmall, fSmall, 1.0f);
            mTextRenderer.DrawStringCentered(ctx,
                "Left/Right: Select       Enter: Equipment       Esc: Close",
                (fW * 0.5f) / fSmall, (fH - 22.0f) / fSmall,
                DirectX::XMVectorSet(0.3f, 0.28f, 0.26f, sceneAlpha * 0.8f), ht);
        }
    }

    // Flash message
    if (mFlashTimer > 0.0f && !mFlashMessage.empty())
    {
        float alpha = (mFlashTimer < 0.5f) ? mFlashTimer * 2.0f : 1.0f;
        alpha *= sceneAlpha;
        DirectX::XMVECTOR fc = DirectX::XMVectorSet(1.0f, 0.92f, 0.5f, alpha);
        mTextRenderer.DrawStringCentered(ctx, mFlashMessage.c_str(),
            fW * 0.5f, fH - 60.0f, fc);
    }
}

// ============================================================
//  CHARACTER VIEW — 3-column design
//
//  LAYOUT (ratio-based — scales with any resolution):
//    LEFT   (4-35%):  Equipment panel
//    CENTER (35-70%): Character sprite + glow + name
//    RIGHT  (72-95%): Stats panel
//    BOTTOM:          Context-sensitive hint footer
//
//  ALL positions are computed from screen dimensions × config ratios.
//  No hardcoded pixel positions — truly resolution-independent.
// ============================================================

void LineupState::RenderCharacterView()
{
    auto& d3d = D3DContext::Get();
    ID3D11DeviceContext* ctx = d3d.GetContext();
    const int W = d3d.GetWidth();
    const int H = d3d.GetHeight();
    const float fW = static_cast<float>(W);
    const float fH = static_cast<float>(H);

    const auto& party = PartyManager::Get().GetActiveParty();
    if (mMemberCursor < 0 || mMemberCursor >= mPartySize) return;

    const PartyMember& member = party[mMemberCursor];
    const BattlerStats stats = PartyManager::Get().GetEffectiveStats(mMemberCursor);

    // Font scale shorthands.
    const float fSection = mFontConfig.sectionScale;
    const float fName    = mFontConfig.nameScale;
    const float fBody    = mFontConfig.bodyScale;
    const float fSmall   = mFontConfig.smallScale;

    // ================================================================
    //  Resolve ratio-based pixel positions from screen dimensions
    // ================================================================
    const float spriteX = fW * mCharLayout.spriteXRatio;
    const float spriteY = fH * mCharLayout.spriteYRatio;
    const float nameY   = fH * mCharLayout.nameYRatio;
    const float slotX   = fW * mCharLayout.slotListXRatio;
    const float slotY   = fH * mCharLayout.slotListYRatio;
    const float statX   = fW * mCharLayout.statXRatio;
    const float statY   = fH * mCharLayout.statYRatio;

    // ================================================================
    //  Shared position: ONE world position, glow derived via WorldToScreen
    // ================================================================
    Camera2D& cam = mCameraCtrl.GetCamera();

    const float sprWorldX = spriteX - fW * 0.5f;
    const float sprWorldY = spriteY - fH * 0.5f;

    // Where does the sprite's pivot (feet) actually appear on screen?
    DirectX::XMFLOAT2 sprScreen = cam.WorldToScreen(sprWorldX, sprWorldY);

    // ================================================================
    //  PASS 1 — Glow behind character (screen space)
    //  Position derived from sprite's actual screen position.
    // ================================================================
    {
        const float glowX = sprScreen.x;
        const float glowY = sprScreen.y + mCharLayout.glowOffsetY;
        const float baseR = mCharLayout.glowRadius;
        float pulse = 0.5f + 0.2f * sinf(mElapsed * 1.5f);

        mGlow.Draw(ctx, glowX, glowY, baseR * 1.2f * pulse,
                   0.08f, 0.07f, 0.2f, W, H);
        mGlow.Draw(ctx, glowX, glowY, baseR * 0.7f * pulse,
                   0.1f, 0.12f, 0.3f, W, H);
    }

    // ================================================================
    //  PASS 2 — Character sprite (world space, same position as above)
    // ================================================================
    if (mMemberCursor < static_cast<int>(mSprites.size()) && mSprites[mMemberCursor])
    {
        float breathe = mCharLayout.spriteScale + 0.06f * sinf(mElapsed * 1.0f);
        mSprites[mMemberCursor]->Draw(ctx, cam, sprWorldX, sprWorldY, breathe, false);
    }

    // ================================================================
    //  PASS 3 — All text (screen space)
    // ================================================================

    // --- CHARACTER NAME (centered above sprite) ---
    {
        DirectX::XMMATRIX nt = DirectX::XMMatrixScaling(fName, fName, 1.0f);
        mTextRenderer.DrawStringCentered(ctx, member.name.c_str(),
            sprScreen.x / fName, nameY / fName,
            DirectX::XMVectorSet(1.0f, 0.98f, 0.92f, 1.0f), nt);
    }

    // --- LEVEL (below name, centered) ---
    {
        char lvlBuf[32];
        _snprintf_s(lvlBuf, sizeof(lvlBuf), _TRUNCATE, "Lv.%d", stats.level);
        DirectX::XMMATRIX lt = DirectX::XMMatrixScaling(fSmall, fSmall, 1.0f);
        mTextRenderer.DrawStringCentered(ctx, lvlBuf,
            sprScreen.x / fSmall, (nameY + fName * 24.0f) / fSmall,
            DirectX::XMVectorSet(0.85f, 0.75f, 0.4f, 1.0f), lt);
    }

    // --- EQUIPMENT PANEL (left column) ---

    // Section header
    {
        DirectX::XMMATRIX tt = DirectX::XMMatrixScaling(fSection, fSection, 1.0f);
        mTextRenderer.DrawString(ctx, "EQUIPMENT",
            slotX / fSection, (slotY - 30.0f) / fSection,
            DirectX::XMVectorSet(0.75f, 0.68f, 0.45f, 1.0f), tt);
    }

    // Slot list
    {
        constexpr EquipSlot slotOrder[kEquipSlotCount] = {
            EquipSlot::Weapon, EquipSlot::Body, EquipSlot::Head, EquipSlot::Accessory
        };

        for (int s = 0; s < kEquipSlotCount; ++s)
        {
            const float rowY = slotY + s * mCharLayout.slotSpacing;
            const bool isSelected = (mPhase == Phase::SlotSelect && s == mSlotCursor);
            const bool isPickerSlot = (mPhase == Phase::EquipPicker && s == mSlotCursor);

            // Slot type label
            DirectX::XMMATRIX labelT = DirectX::XMMatrixScaling(fBody, fBody, 1.0f);
            DirectX::XMVECTOR labelColor = (isSelected || isPickerSlot)
                ? DirectX::XMVectorSet(0.85f, 0.75f, 0.45f, 1.0f)
                : DirectX::XMVectorSet(0.4f, 0.38f, 0.35f, 1.0f);
            mTextRenderer.DrawString(ctx, SlotLabel(slotOrder[s]),
                slotX / fBody, rowY / fBody, labelColor, labelT);

            // Cursor indicator
            if (isSelected)
            {
                float pulse = 0.5f + 0.3f * sinf(mElapsed * 4.0f);
                DirectX::XMVECTOR cursorColor = DirectX::XMVectorSet(0.6f, 0.75f, 1.0f, pulse);
                DirectX::XMMATRIX ct = DirectX::XMMatrixScaling(fBody, fBody, 1.0f);
                mTextRenderer.DrawString(ctx, ">",
                    (slotX - 18.0f) / fBody, rowY / fBody,
                    cursorColor, ct);
            }

            // Equipped item name
            std::string equippedId = PartyManager::Get().GetEquippedItem(mMemberCursor, slotOrder[s]);
            const char* itemName = "- empty -";
            DirectX::XMVECTOR itemColor = DirectX::XMVectorSet(0.3f, 0.28f, 0.25f, 0.6f);

            if (!equippedId.empty())
            {
                const ItemData* item = ItemRegistry::Get().Find(equippedId);
                if (item) {
                    itemName = item->name.c_str();
                    itemColor = (isSelected || isPickerSlot)
                        ? DirectX::XMVectorSet(1.0f, 0.98f, 0.92f, 1.0f)
                        : DirectX::XMVectorSet(0.65f, 0.62f, 0.58f, 1.0f);
                }
            }

            DirectX::XMMATRIX itemT = DirectX::XMMatrixScaling(fBody, fBody, 1.0f);
            mTextRenderer.DrawString(ctx, itemName,
                slotX / fBody, (rowY + 22.0f) / fBody,
                itemColor, itemT);
        }
    }

    // --- STATS PANEL (right column) ---
    RenderStatPanelText(statX, statY, mPhase == Phase::EquipPicker);

    // --- EQUIPMENT PICKER (overlays left column, shifts right) ---
    if (mPhase == Phase::EquipPicker)
        RenderEquipPickerText();

    // --- HINT FOOTER (centered bottom) ---
    {
        const char* hintText = "";
        switch (mPhase)
        {
        case Phase::SlotSelect:
            hintText = "Up/Down: Slot    Left/Right: Character    Enter: Change    Esc: Back";
            break;
        case Phase::EquipPicker:
            hintText = "Up/Down: Item    Enter: Equip    Esc: Cancel";
            break;
        default: break;
        }
        DirectX::XMMATRIX ht = DirectX::XMMatrixScaling(fSmall, fSmall, 1.0f);
        mTextRenderer.DrawStringCentered(ctx, hintText,
            (fW * 0.5f) / fSmall, (fH - 22.0f) / fSmall,
            DirectX::XMVectorSet(0.3f, 0.28f, 0.26f, 1.0f), ht);
    }

    // Flash message
    if (mFlashTimer > 0.0f && !mFlashMessage.empty())
    {
        float alpha = (mFlashTimer < 0.5f) ? mFlashTimer * 2.0f : 1.0f;
        DirectX::XMVECTOR fc = DirectX::XMVectorSet(1.0f, 0.92f, 0.5f, alpha);
        mTextRenderer.DrawStringCentered(ctx, mFlashMessage.c_str(),
            fW * 0.5f, fH - 55.0f, fc);
    }
}

// ============================================================
//  Equipment picker text (overlays left column, shifted right)
// ============================================================

void LineupState::RenderEquipPickerText()
{
    auto& d3d = D3DContext::Get();
    ID3D11DeviceContext* ctx = d3d.GetContext();
    const float fW = static_cast<float>(d3d.GetWidth());
    const float fH = static_cast<float>(d3d.GetHeight());

    const float fSection = mFontConfig.sectionScale;
    const float fBody    = mFontConfig.bodyScale;
    const float fSmall   = mFontConfig.smallScale;

    const int total = static_cast<int>(mPickerItems.size()) + 1;
    const int maxVisible = mCharLayout.pickerMaxVisible;
    const int visibleCount = (std::min)(total, maxVisible);

    // Picker positioned to the right of equipment slots (inside left column area)
    const float pX = fW * mCharLayout.pickerXRatio + 280.0f;
    const float pY = fH * mCharLayout.pickerYRatio;

    // Section header
    {
        DirectX::XMMATRIX ht = DirectX::XMMatrixScaling(fSection, fSection, 1.0f);
        mTextRenderer.DrawString(ctx, "SELECT ITEM",
            pX / fSection, (pY - 28.0f) / fSection,
            DirectX::XMVectorSet(0.75f, 0.68f, 0.45f, 1.0f), ht);
    }

    for (int vi = 0; vi < visibleCount && (mPickerScroll + vi) < total; ++vi)
    {
        const int idx = mPickerScroll + vi;
        const float rowY = pY + vi * mCharLayout.pickerItemHeight;
        const bool isCursor = (idx == mPickerCursor);

        DirectX::XMMATRIX tt = DirectX::XMMatrixScaling(fBody, fBody, 1.0f);

        // Cursor indicator
        if (isCursor)
        {
            float pulse = 0.5f + 0.3f * sinf(mElapsed * 4.0f);
            DirectX::XMVECTOR cursorColor = DirectX::XMVectorSet(0.6f, 0.75f, 1.0f, pulse);
            mTextRenderer.DrawString(ctx, ">",
                (pX - 16.0f) / fBody, rowY / fBody, cursorColor, tt);
        }

        if (idx == static_cast<int>(mPickerItems.size()))
        {
            DirectX::XMVECTOR uC = isCursor
                ? DirectX::XMVectorSet(1.0f, 0.4f, 0.35f, 1.0f)
                : DirectX::XMVectorSet(0.4f, 0.25f, 0.22f, 0.7f);
            mTextRenderer.DrawString(ctx, "(unequip)",
                pX / fBody, rowY / fBody, uC, tt);
        }
        else
        {
            const std::string& itemId = mPickerItems[idx];
            const ItemData* item = ItemRegistry::Get().Find(itemId);
            const char* name = item ? item->name.c_str() : itemId.c_str();

            DirectX::XMVECTOR nameColor = isCursor
                ? DirectX::XMVectorSet(1.0f, 0.98f, 0.92f, 1.0f)
                : DirectX::XMVectorSet(0.5f, 0.48f, 0.45f, 1.0f);
            mTextRenderer.DrawString(ctx, name,
                pX / fBody, rowY / fBody, nameColor, tt);

            // Stat bonuses on hover
            if (item && isCursor)
            {
                char bonusBuf[128] = {};
                char* p = bonusBuf;
                int remaining = sizeof(bonusBuf);
                auto appendStat = [&](const char* label, int val) {
                    if (val != 0 && remaining > 0) {
                        int written = _snprintf_s(p, remaining, _TRUNCATE,
                            "%s%s%d  ", val > 0 ? "+" : "", label, val);
                        if (written > 0) { p += written; remaining -= written; }
                    }
                };
                appendStat("ATK", item->bonusAtk);
                appendStat("DEF", item->bonusDef);
                appendStat("MATK", item->bonusMatk);
                appendStat("MDEF", item->bonusMdef);
                appendStat("SPD", item->bonusSpd);
                appendStat("HP", item->bonusMaxHp);

                if (bonusBuf[0] != '\0')
                {
                    DirectX::XMMATRIX bt = DirectX::XMMatrixScaling(fSmall, fSmall, 1.0f);
                    mTextRenderer.DrawString(ctx, bonusBuf,
                        pX / fSmall, (rowY + 22.0f) / fSmall,
                        DirectX::XMVectorSet(0.4f, 0.8f, 0.4f, 0.9f), bt);
                }
            }
        }
    }
}

// ============================================================
//  Stat panel text (right column)
// ============================================================

void LineupState::RenderStatPanelText(float x, float y, bool showPreview)
{
    auto& d3d = D3DContext::Get();
    ID3D11DeviceContext* ctx = d3d.GetContext();

    if (mMemberCursor < 0 || mMemberCursor >= mPartySize) return;

    const float fSection = mFontConfig.sectionScale;
    const float fBody    = mFontConfig.bodyScale;

    const BattlerStats stats = PartyManager::Get().GetEffectiveStats(mMemberCursor);

    // Section title
    {
        DirectX::XMMATRIX tt = DirectX::XMMatrixScaling(fSection, fSection, 1.0f);
        mTextRenderer.DrawString(ctx, "STATS",
            (x + 10.0f) / fSection, (y - 28.0f) / fSection,
            DirectX::XMVectorSet(0.75f, 0.68f, 0.45f, 1.0f), tt);
    }

    struct StatRow { const char* label; int value; };
    StatRow rows[] = {
        { "HP",    stats.hp },
        { "MaxHP", stats.maxHp },
        { "MP",    stats.mp },
        { "MaxMP", stats.maxMp },
        { "ATK",   stats.atk },
        { "DEF",   stats.def },
        { "MATK",  stats.matk },
        { "MDEF",  stats.mdef },
        { "SPD",   stats.spd },
    };
    const int numRows = sizeof(rows) / sizeof(rows[0]);
    const float rowH = 28.0f;

    // Preview stats
    BattlerStats previewStats = stats;
    bool hasPreview = false;
    if (showPreview && !PickerCursorIsUnequip() &&
        mPickerCursor >= 0 && mPickerCursor < static_cast<int>(mPickerItems.size()))
    {
        previewStats = PartyManager::Get().PreviewEffectiveStats(
            mMemberCursor, mPickerSlot, mPickerItems[mPickerCursor]);
        hasPreview = true;
    }

    StatRow previewRows[] = {
        { "HP",    previewStats.hp },
        { "MaxHP", previewStats.maxHp },
        { "MP",    previewStats.mp },
        { "MaxMP", previewStats.maxMp },
        { "ATK",   previewStats.atk },
        { "DEF",   previewStats.def },
        { "MATK",  previewStats.matk },
        { "MDEF",  previewStats.mdef },
        { "SPD",   previewStats.spd },
    };

    DirectX::XMMATRIX tr = DirectX::XMMatrixScaling(fBody, fBody, 1.0f);

    for (int r = 0; r < numRows; ++r)
    {
        const float ry = y + r * rowH;

        // Label
        mTextRenderer.DrawString(ctx, rows[r].label,
            (x + 10.0f) / fBody, ry / fBody,
            DirectX::XMVectorSet(0.45f, 0.42f, 0.4f, 1.0f), tr);

        // Value
        char valBuf[32];
        _snprintf_s(valBuf, sizeof(valBuf), _TRUNCATE, "%d", rows[r].value);
        mTextRenderer.DrawString(ctx, valBuf,
            (x + 130.0f) / fBody, ry / fBody,
            DirectX::XMVectorSet(0.85f, 0.82f, 0.78f, 1.0f), tr);

        // Preview delta
        if (hasPreview)
        {
            int delta = previewRows[r].value - rows[r].value;
            if (delta != 0)
            {
                char deltaBuf[32];
                _snprintf_s(deltaBuf, sizeof(deltaBuf), _TRUNCATE,
                            "%s%d", delta > 0 ? "+" : "", delta);
                DirectX::XMVECTOR deltaColor = (delta > 0)
                    ? DirectX::XMVectorSet(0.3f, 0.9f, 0.3f, 1.0f)
                    : DirectX::XMVectorSet(1.0f, 0.3f, 0.25f, 1.0f);
                mTextRenderer.DrawString(ctx, deltaBuf,
                    (x + 200.0f) / fBody, ry / fBody, deltaColor, tr);
            }
        }
    }
}

