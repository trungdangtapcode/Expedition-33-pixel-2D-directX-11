// ============================================================
// File: MemoryArchiveState.h
// Responsibility: Campfire-only archive for replaying recovered memory shards.
//
// Owns:
//   UI renderers, loaded archive entry view data, and menu input state.
//
// Lifetime:
//   Pushed in  -> CampfireState when the player chooses Memory Archive.
//   Popped via -> Escape, Backspace, or the Back row behavior.
//
// Important:
//   - Does not grant rewards. Rewards belong to OverworldState collection.
//   - Replays memory dialogue through DialogueState using existing scripts.
//   - Uses GameProgress flags to decide which memories are recovered.
// ============================================================
#pragma once

#include "IGameState.h"
#include "../Renderer/ItemIconRenderer.h"
#include "../Renderer/NineSliceRenderer.h"
#include "../UI/BattleTextRenderer.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

struct MemoryArchiveLayout
{
    float panelWidth = 760.0f;
    float panelHeight = 500.0f;
    float titleY = 34.0f;
    float subtitleY = 78.0f;
    float iconX = 72.0f;
    float iconY = 46.0f;
    float iconSize = 64.0f;
    float listX = 84.0f;
    float listY = 132.0f;
    float listWidth = 360.0f;
    float rowHeight = 46.0f;
    int maxVisibleRows = 7;
    float detailX = 472.0f;
    float detailY = 132.0f;
    float detailWidth = 242.0f;
    float detailHeight = 270.0f;
    float hintY = 456.0f;
    float flashY = 420.0f;
    float flashDuration = 1.6f;
};

class MemoryArchiveState : public IGameState
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    bool ShouldRenderBelow() const override { return true; }
    const char* GetName() const override { return "MemoryArchiveState"; }

private:
    struct Entry
    {
        std::string id;
        std::string displayNameKey;
        std::string displayNameFallback;
        std::string collectedFlag;
        std::string dialoguePath;
    };

    bool Pressed(int vk, bool& wasDown);
    bool LoadLayout(const std::string& path);
    bool LoadEntries(const std::string& path);
    bool LoadIcon(ID3D11Device* device,
                  ID3D11DeviceContext* context,
                  const std::string& path);
    void ClampCursor();
    void MoveCursor(int direction);
    void ActivateSelection();
    void Flash(const std::string& message);
    void RenderEntryList(float panelX, float panelY);
    void RenderDetailPanel(float panelX, float panelY);
    int CollectedCount() const;
    bool IsCollected(const Entry& entry) const;
    std::string EntryName(const Entry& entry) const;

    MemoryArchiveLayout mLayout;
    std::vector<Entry> mEntries;
    NineSliceRenderer mDialogBox;
    BattleTextRenderer mTextRenderer;
    ItemIconRenderer mIconRenderer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mIconSRV;
    int mCursor = 0;
    int mTopIndex = 0;
    float mElapsed = 0.0f;
    std::string mFlashMessage;
    float mFlashTimer = 0.0f;
    int mResizeListenerId = -1;
    bool mUpWasDown = true;
    bool mDownWasDown = true;
    bool mEnterWasDown = true;
    bool mEscWasDown = true;
    bool mBackWasDown = true;
};
