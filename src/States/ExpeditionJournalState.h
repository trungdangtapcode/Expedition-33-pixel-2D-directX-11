// ============================================================
// File: ExpeditionJournalState.h
// Responsibility: Read-only route and memory journal overlay.
//
// Owns:
//   UI renderers, generated journal icon SRV, parsed journal rows,
//   and local menu input state.
//
// Lifetime:
//   Pushed in  -> CampfireState or PauseState when the player opens Journal.
//   Popped via -> Escape or Backspace.
//
// Important:
//   - Reads objective and memory data, but never mutates progress.
//   - Progress status is derived from GameProgress flags only.
//   - Does not expose equipment or save/load actions, so it cannot bypass
//     campfire-only restrictions.
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

struct ExpeditionJournalLayout
{
    std::string iconPath = "assets/UI/expedition_journal_icon.png";
    float panelWidth = 900.0f;
    float panelHeight = 560.0f;
    float titleY = 34.0f;
    float subtitleY = 76.0f;
    float iconX = 64.0f;
    float iconY = 42.0f;
    float iconSize = 64.0f;
    float tabY = 112.0f;
    float tabGap = 180.0f;
    float listX = 72.0f;
    float listY = 158.0f;
    float listWidth = 390.0f;
    float rowHeight = 42.0f;
    int maxVisibleRows = 8;
    float detailX = 500.0f;
    float detailY = 158.0f;
    float detailWidth = 320.0f;
    float detailHeight = 318.0f;
    float detailBodyY = 74.0f;
    float detailLineHeight = 25.0f;
    float hintY = 510.0f;
    float dimAlpha = 0.54f;
    float panelAlpha = 0.96f;
    float detailPanelAlpha = 0.72f;
    float rowHighlightAlpha = 0.44f;
    float accentR = 0.92f;
    float accentG = 0.76f;
    float accentB = 0.42f;
    float textR = 0.92f;
    float textG = 0.90f;
    float textB = 0.84f;
    float mutedR = 0.70f;
    float mutedG = 0.70f;
    float mutedB = 0.66f;
    float lockedR = 0.42f;
    float lockedG = 0.42f;
    float lockedB = 0.42f;
    float completeR = 0.48f;
    float completeG = 0.82f;
    float completeB = 0.58f;
};

class ExpeditionJournalState : public IGameState
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    bool ShouldRenderBelow() const override { return true; }
    const char* GetName() const override { return "ExpeditionJournalState"; }

private:
    enum class Tab
    {
        Route,
        Memories,
        Count
    };

    enum class RowStatus
    {
        Locked,
        Current,
        Complete
    };

    struct ObjectiveRow
    {
        std::string id;
        std::string titleKey;
        std::string titleFallback;
        std::string bodyKey;
        std::string bodyFallback;
        std::vector<std::string> requiresFlags;
        std::vector<std::string> blockedByFlags;
        RowStatus status = RowStatus::Locked;
    };

    struct MemoryRow
    {
        std::string id;
        std::string displayNameKey;
        std::string displayNameFallback;
        std::string collectedFlag;
        RowStatus status = RowStatus::Locked;
    };

    bool Pressed(int vk, bool& wasDown);
    bool LoadLayout(const std::string& path);
    bool LoadObjectives(const std::string& path);
    bool LoadMemories(const std::string& path);
    bool LoadIcon(ID3D11Device* device,
                  ID3D11DeviceContext* context,
                  const std::string& path);
    void RefreshProgress();
    void MoveCursor(int direction);
    void SwitchTab(int direction);
    void ClampCursor();
    int CurrentRowCount() const;
    int CompletedObjectiveCount() const;
    int RecoveredMemoryCount() const;
    bool RequirementsMet(const std::vector<std::string>& flags) const;
    bool AnyFlagSet(const std::vector<std::string>& flags) const;
    std::string ObjectiveTitle(const ObjectiveRow& row) const;
    std::string ObjectiveBody(const ObjectiveRow& row) const;
    std::string MemoryName(const MemoryRow& row) const;
    std::string MemoryStatusLabel(RowStatus status) const;
    std::string StatusLabel(RowStatus status) const;
    DirectX::XMVECTOR StatusColor(RowStatus status) const;
    DirectX::XMVECTOR MakeColor(float r, float g, float b, float a = 1.0f) const;
    std::vector<std::string> WrapText(const std::string& text,
                                      float maxWidth,
                                      float scale) const;
    void DrawWrappedText(const std::vector<std::string>& lines,
                         float x,
                         float y,
                         DirectX::FXMVECTOR color,
                         float scale);
    void RenderTabs(float panelX, float panelY);
    void RenderRouteList(float panelX, float panelY);
    void RenderMemoryList(float panelX, float panelY);
    void RenderDetailPanel(float panelX, float panelY);

    ExpeditionJournalLayout mLayout;
    std::vector<ObjectiveRow> mObjectives;
    std::vector<MemoryRow> mMemories;
    NineSliceRenderer mDialogBox;
    BattleTextRenderer mTextRenderer;
    ItemIconRenderer mIconRenderer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mIconSRV;
    Tab mTab = Tab::Route;
    int mCursor = 0;
    int mTopIndex = 0;
    float mElapsed = 0.0f;
    int mResizeListenerId = -1;
    bool mUpWasDown = true;
    bool mDownWasDown = true;
    bool mLeftWasDown = true;
    bool mRightWasDown = true;
    bool mEscWasDown = true;
    bool mBackWasDown = true;
};
