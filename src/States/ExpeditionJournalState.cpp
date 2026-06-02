// ============================================================
// File: ExpeditionJournalState.cpp
// Responsibility: Implement the read-only expedition journal.
//
// Architecture:
//   ExpeditionJournalState is a screen overlay that converts current durable
//   progress flags into player-facing route and memory status. It does not own
//   objectives, story events, or rewards; it only reads the same data already
//   used by ObjectiveDirector and memory shard spawning.
//
// Common mistakes:
//   1. Adding separate quest booleans here would desync save/load.
//   2. Replaying memory collection logic here would duplicate rewards.
//   3. Letting PauseState expose equipment through this overlay would bypass
//      the campfire-only equipment rule.
// ============================================================
#define NOMINMAX
#include "ExpeditionJournalState.h"
#include "StateManager.h"
#include "../Audio/AudioManager.h"
#include "../Events/EventManager.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/GameProgress.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <WICTextureLoader.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace
{
    std::filesystem::path ResolveReadablePath(const std::string& path)
    {
        namespace fs = std::filesystem;

        const fs::path direct(path);
        if (fs::exists(direct)) return direct;

        const fs::path parent = fs::path("..") / path;
        if (fs::exists(parent)) return parent;

        return direct;
    }

    bool ReadTextFile(const std::filesystem::path& path, std::string& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    std::string ReadString(const std::string& src,
                           const std::string& key,
                           const std::string& fallback = "")
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }

    float ReadFloat(const std::string& src,
                    const std::string& key,
                    float fallback)
    {
        return JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(src, key),
            fallback);
    }

    int ReadInt(const std::string& src,
                const std::string& key,
                int fallback)
    {
        return JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(src, key),
            fallback);
    }
}

// ------------------------------------------------------------
// Function: OnEnter
// Purpose:
//   Initialize GPU resources and load the current journal data.
// Why:
//   The journal can be opened from multiple states, so it must be self-contained
//   and rebuild progress rows from durable flags every time it appears.
// ------------------------------------------------------------
void ExpeditionJournalState::OnEnter()
{
    auto& d3d = D3DContext::Get();

    LoadLayout("data/expedition_journal_layout.json");
    LoadObjectives("data/objectives.json");
    LoadMemories("data/overworld_memory_shards.json");
    RefreshProgress();
    ClampCursor();

    mDialogBox.Initialize(
        d3d.GetDevice(),
        d3d.GetContext(),
        L"assets/UI/ui-dialog-box-hd.png",
        "assets/UI/ui-dialog-box-hd.json",
        d3d.GetWidth(),
        d3d.GetHeight());

    const std::string fontPath = LocalizationManager::Get().GetCurrentFontPath();
    mTextRenderer.Initialize(
        d3d.GetDevice(),
        d3d.GetContext(),
        std::wstring(fontPath.begin(), fontPath.end()),
        d3d.GetWidth(),
        d3d.GetHeight());

    mIconRenderer.Initialize(d3d.GetDevice(), d3d.GetContext(), d3d.GetWidth(), d3d.GetHeight());
    LoadIcon(d3d.GetDevice(), d3d.GetContext(), mLayout.iconPath);

    mResizeListenerId = EventManager::Get().Subscribe("window_resized",
        [this](const EventData&)
        {
            const int width = D3DContext::Get().GetWidth();
            const int height = D3DContext::Get().GetHeight();
            mDialogBox.SetScreenSize(width, height);
            mTextRenderer.SetScreenSize(width, height);
            mIconRenderer.SetScreenSize(width, height);
        });

    mElapsed = 0.0f;
    LOG("[ExpeditionJournalState] Opened with %d objective rows and %d memory rows.",
        static_cast<int>(mObjectives.size()),
        static_cast<int>(mMemories.size()));
}

// ------------------------------------------------------------
// Function: OnExit
// Purpose:
//   Release journal resources and event subscriptions.
// Why:
//   Window resize callbacks capture this state and must not survive it.
// ------------------------------------------------------------
void ExpeditionJournalState::OnExit()
{
    if (mResizeListenerId != -1)
    {
        EventManager::Get().Unsubscribe("window_resized", mResizeListenerId);
        mResizeListenerId = -1;
    }

    mIconSRV.Reset();
    mIconRenderer.Shutdown();
    mTextRenderer.Shutdown();
    mDialogBox.Shutdown();
    LOG("[ExpeditionJournalState] Closed.");
}

// ------------------------------------------------------------
// Function: Pressed
// Purpose:
//   Convert raw keyboard state into one-press menu input.
// Why:
//   Holding a key should not repeatedly scroll or switch tabs.
// ------------------------------------------------------------
bool ExpeditionJournalState::Pressed(int vk, bool& wasDown)
{
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool fresh = down && !wasDown;
    wasDown = down;
    return fresh;
}

// ------------------------------------------------------------
// Function: LoadLayout
// Purpose:
//   Read journal presentation values from JSON.
// Why:
//   Panel sizing, row spacing, and theme colors need tuning without rebuilding.
// ------------------------------------------------------------
bool ExpeditionJournalState::LoadLayout(const std::string& path)
{
    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[ExpeditionJournalState] WARNING: Missing layout '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    mLayout.iconPath = ReadString(src, "iconPath", mLayout.iconPath);
    mLayout.panelWidth = ReadFloat(src, "panelWidth", mLayout.panelWidth);
    mLayout.panelHeight = ReadFloat(src, "panelHeight", mLayout.panelHeight);
    mLayout.titleY = ReadFloat(src, "titleY", mLayout.titleY);
    mLayout.subtitleY = ReadFloat(src, "subtitleY", mLayout.subtitleY);
    mLayout.iconX = ReadFloat(src, "iconX", mLayout.iconX);
    mLayout.iconY = ReadFloat(src, "iconY", mLayout.iconY);
    mLayout.iconSize = ReadFloat(src, "iconSize", mLayout.iconSize);
    mLayout.tabY = ReadFloat(src, "tabY", mLayout.tabY);
    mLayout.tabGap = ReadFloat(src, "tabGap", mLayout.tabGap);
    mLayout.listX = ReadFloat(src, "listX", mLayout.listX);
    mLayout.listY = ReadFloat(src, "listY", mLayout.listY);
    mLayout.listWidth = ReadFloat(src, "listWidth", mLayout.listWidth);
    mLayout.rowHeight = ReadFloat(src, "rowHeight", mLayout.rowHeight);
    mLayout.maxVisibleRows = std::max(1, ReadInt(src, "maxVisibleRows", mLayout.maxVisibleRows));
    mLayout.detailX = ReadFloat(src, "detailX", mLayout.detailX);
    mLayout.detailY = ReadFloat(src, "detailY", mLayout.detailY);
    mLayout.detailWidth = ReadFloat(src, "detailWidth", mLayout.detailWidth);
    mLayout.detailHeight = ReadFloat(src, "detailHeight", mLayout.detailHeight);
    mLayout.detailBodyY = ReadFloat(src, "detailBodyY", mLayout.detailBodyY);
    mLayout.detailLineHeight = ReadFloat(src, "detailLineHeight", mLayout.detailLineHeight);
    mLayout.hintY = ReadFloat(src, "hintY", mLayout.hintY);
    mLayout.dimAlpha = ReadFloat(src, "dimAlpha", mLayout.dimAlpha);
    mLayout.panelAlpha = ReadFloat(src, "panelAlpha", mLayout.panelAlpha);
    mLayout.detailPanelAlpha = ReadFloat(src, "detailPanelAlpha", mLayout.detailPanelAlpha);
    mLayout.rowHighlightAlpha = ReadFloat(src, "rowHighlightAlpha", mLayout.rowHighlightAlpha);
    mLayout.accentR = ReadFloat(src, "accentR", mLayout.accentR);
    mLayout.accentG = ReadFloat(src, "accentG", mLayout.accentG);
    mLayout.accentB = ReadFloat(src, "accentB", mLayout.accentB);
    mLayout.textR = ReadFloat(src, "textR", mLayout.textR);
    mLayout.textG = ReadFloat(src, "textG", mLayout.textG);
    mLayout.textB = ReadFloat(src, "textB", mLayout.textB);
    mLayout.mutedR = ReadFloat(src, "mutedR", mLayout.mutedR);
    mLayout.mutedG = ReadFloat(src, "mutedG", mLayout.mutedG);
    mLayout.mutedB = ReadFloat(src, "mutedB", mLayout.mutedB);
    mLayout.lockedR = ReadFloat(src, "lockedR", mLayout.lockedR);
    mLayout.lockedG = ReadFloat(src, "lockedG", mLayout.lockedG);
    mLayout.lockedB = ReadFloat(src, "lockedB", mLayout.lockedB);
    mLayout.completeR = ReadFloat(src, "completeR", mLayout.completeR);
    mLayout.completeG = ReadFloat(src, "completeG", mLayout.completeG);
    mLayout.completeB = ReadFloat(src, "completeB", mLayout.completeB);
    return true;
}

// ------------------------------------------------------------
// Function: LoadObjectives
// Purpose:
//   Load ordered objective rows from the same data as ObjectiveDirector.
// Why:
//   The journal should expose the route spine without inventing separate quest
//   data that could diverge from the HUD objective.
// ------------------------------------------------------------
bool ExpeditionJournalState::LoadObjectives(const std::string& path)
{
    mObjectives.clear();

    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[ExpeditionJournalState] WARNING: Missing objective data '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "objectives");

    for (const std::string& objectSrc : objects)
    {
        ObjectiveRow row{};
        row.id = ReadString(objectSrc, "id");
        row.titleKey = ReadString(objectSrc, "titleKey");
        row.titleFallback = ReadString(objectSrc, "title", "Objective");
        row.bodyKey = ReadString(objectSrc, "bodyKey");
        row.bodyFallback = ReadString(objectSrc, "body", "");
        row.requiresFlags = JsonLoader::detail::ExtractStringArray(objectSrc, "requiresFlags");
        row.blockedByFlags = JsonLoader::detail::ExtractStringArray(objectSrc, "blockedByFlags");

        if (row.id.empty())
        {
            LOG("[ExpeditionJournalState] WARNING: Skipping objective without id.");
            continue;
        }
        mObjectives.push_back(std::move(row));
    }

    return !mObjectives.empty();
}

// ------------------------------------------------------------
// Function: LoadMemories
// Purpose:
//   Load memory rows from the same shard data spawned in the overworld.
// Why:
//   The journal should summarize lore collection progress without duplicating
//   memory archive authoring.
// ------------------------------------------------------------
bool ExpeditionJournalState::LoadMemories(const std::string& path)
{
    mMemories.clear();

    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[ExpeditionJournalState] WARNING: Missing memory shard data '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "shards");

    for (const std::string& objectSrc : objects)
    {
        MemoryRow row{};
        row.id = ReadString(objectSrc, "id");
        row.displayNameKey = ReadString(objectSrc, "displayNameKey");
        row.displayNameFallback = ReadString(objectSrc, "displayName", "Memory");
        row.collectedFlag = ReadString(objectSrc, "collectedFlag");

        if (row.id.empty() || row.collectedFlag.empty())
        {
            LOG("[ExpeditionJournalState] WARNING: Skipping invalid memory row.");
            continue;
        }
        mMemories.push_back(std::move(row));
    }

    return !mMemories.empty();
}

// ------------------------------------------------------------
// Function: LoadIcon
// Purpose:
//   Load the generated journal icon into a shader resource view.
// Why:
//   Keeping the path in layout data lets future journal art swap without code.
// ------------------------------------------------------------
bool ExpeditionJournalState::LoadIcon(ID3D11Device* device,
                                      ID3D11DeviceContext* context,
                                      const std::string& path)
{
    const std::filesystem::path resolved = ResolveReadablePath(path);
    const HRESULT hr = DirectX::CreateWICTextureFromFile(
        device,
        context,
        resolved.wstring().c_str(),
        nullptr,
        mIconSRV.ReleaseAndGetAddressOf());

    if (FAILED(hr))
    {
        LOG("[ExpeditionJournalState] WARNING: Failed to load icon '%s'.", path.c_str());
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// Function: RefreshProgress
// Purpose:
//   Convert durable flags into row statuses.
// Why:
//   Objective and memory data stay static, while GameProgress determines what
//   the player has actually done in the current save slot.
// ------------------------------------------------------------
void ExpeditionJournalState::RefreshProgress()
{
    bool currentAssigned = false;
    for (ObjectiveRow& row : mObjectives)
    {
        const bool complete = AnyFlagSet(row.blockedByFlags);
        const bool available = RequirementsMet(row.requiresFlags);
        if (complete)
        {
            row.status = RowStatus::Complete;
        }
        else if (available && !currentAssigned)
        {
            row.status = RowStatus::Current;
            currentAssigned = true;
        }
        else
        {
            row.status = RowStatus::Locked;
        }
    }

    for (MemoryRow& row : mMemories)
    {
        row.status = GameProgress::Get().HasFlag(row.collectedFlag)
            ? RowStatus::Complete
            : RowStatus::Locked;
    }
}

// ------------------------------------------------------------
// Function: MoveCursor
// Purpose:
//   Move the selected row in the active tab with wraparound.
// Why:
//   Route and memory tabs can have different row counts.
// ------------------------------------------------------------
void ExpeditionJournalState::MoveCursor(int direction)
{
    const int count = CurrentRowCount();
    if (count <= 0) return;

    mCursor = (mCursor + direction + count) % count;
    ClampCursor();
    AudioManager::Get().PlaySfx("ui_navigate");
}

// ------------------------------------------------------------
// Function: SwitchTab
// Purpose:
//   Change between route and memory pages.
// Why:
//   The journal groups navigational progress separately from lore collection.
// ------------------------------------------------------------
void ExpeditionJournalState::SwitchTab(int direction)
{
    const int tabCount = static_cast<int>(Tab::Count);
    int value = static_cast<int>(mTab);
    value = (value + direction + tabCount) % tabCount;
    mTab = static_cast<Tab>(value);
    mCursor = 0;
    mTopIndex = 0;
    ClampCursor();
    AudioManager::Get().PlaySfx("ui_navigate");
}

// ------------------------------------------------------------
// Function: ClampCursor
// Purpose:
//   Keep the cursor and visible window inside the active row list.
// Why:
//   Switching tabs or changing data can alter the valid row range.
// ------------------------------------------------------------
void ExpeditionJournalState::ClampCursor()
{
    const int count = CurrentRowCount();
    if (count <= 0)
    {
        mCursor = 0;
        mTopIndex = 0;
        return;
    }

    mCursor = std::max(0, std::min(mCursor, count - 1));
    if (mCursor < mTopIndex) mTopIndex = mCursor;
    if (mCursor >= mTopIndex + mLayout.maxVisibleRows)
    {
        mTopIndex = mCursor - mLayout.maxVisibleRows + 1;
    }
    mTopIndex = std::max(0, std::min(mTopIndex, count - 1));
}

int ExpeditionJournalState::CurrentRowCount() const
{
    return mTab == Tab::Route
        ? static_cast<int>(mObjectives.size())
        : static_cast<int>(mMemories.size());
}

int ExpeditionJournalState::CompletedObjectiveCount() const
{
    int count = 0;
    for (const ObjectiveRow& row : mObjectives)
    {
        if (row.status == RowStatus::Complete) ++count;
    }
    return count;
}

int ExpeditionJournalState::RecoveredMemoryCount() const
{
    int count = 0;
    for (const MemoryRow& row : mMemories)
    {
        if (row.status == RowStatus::Complete) ++count;
    }
    return count;
}

bool ExpeditionJournalState::RequirementsMet(const std::vector<std::string>& flags) const
{
    for (const std::string& flag : flags)
    {
        if (!GameProgress::Get().HasFlag(flag)) return false;
    }
    return true;
}

bool ExpeditionJournalState::AnyFlagSet(const std::vector<std::string>& flags) const
{
    for (const std::string& flag : flags)
    {
        if (GameProgress::Get().HasFlag(flag)) return true;
    }
    return false;
}

std::string ExpeditionJournalState::ObjectiveTitle(const ObjectiveRow& row) const
{
    return LocalizationManager::Get().TextOrFallback(row.titleKey, row.titleFallback);
}

std::string ExpeditionJournalState::ObjectiveBody(const ObjectiveRow& row) const
{
    return LocalizationManager::Get().TextOrFallback(row.bodyKey, row.bodyFallback);
}

std::string ExpeditionJournalState::MemoryName(const MemoryRow& row) const
{
    return LocalizationManager::Get().TextOrFallback(
        row.displayNameKey,
        row.displayNameFallback);
}

std::string ExpeditionJournalState::MemoryStatusLabel(RowStatus status) const
{
    if (status == RowStatus::Complete)
    {
        return LocalizationManager::Get().Text("journal.status.recovered");
    }
    return StatusLabel(status);
}

std::string ExpeditionJournalState::StatusLabel(RowStatus status) const
{
    switch (status)
    {
    case RowStatus::Current:
        return LocalizationManager::Get().Text("journal.status.current");
    case RowStatus::Complete:
        return LocalizationManager::Get().Text("journal.status.complete");
    case RowStatus::Locked:
    default:
        return LocalizationManager::Get().Text("journal.status.locked");
    }
}

DirectX::XMVECTOR ExpeditionJournalState::StatusColor(RowStatus status) const
{
    switch (status)
    {
    case RowStatus::Current:
        return MakeColor(mLayout.accentR, mLayout.accentG, mLayout.accentB, 1.0f);
    case RowStatus::Complete:
        return MakeColor(mLayout.completeR, mLayout.completeG, mLayout.completeB, 1.0f);
    case RowStatus::Locked:
    default:
        return MakeColor(mLayout.lockedR, mLayout.lockedG, mLayout.lockedB, 1.0f);
    }
}

DirectX::XMVECTOR ExpeditionJournalState::MakeColor(float r, float g, float b, float a) const
{
    return DirectX::XMVectorSet(r, g, b, a);
}

std::vector<std::string> ExpeditionJournalState::WrapText(
    const std::string& text,
    float maxWidth,
    float scale) const
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string word;
    std::string line;

    while (stream >> word)
    {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (mTextRenderer.MeasureStringRaw(candidate.c_str(), scale).x <= maxWidth)
        {
            line = candidate;
            continue;
        }

        if (!line.empty())
        {
            lines.push_back(line);
            line = word;
        }
        else
        {
            lines.push_back(word);
        }
    }

    if (!line.empty()) lines.push_back(line);
    return lines;
}

void ExpeditionJournalState::DrawWrappedText(const std::vector<std::string>& lines,
                                             float x,
                                             float y,
                                             DirectX::FXMVECTOR color,
                                             float scale)
{
    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
    {
        mTextRenderer.DrawStringRawScaled(
            lines[static_cast<std::size_t>(i)].c_str(),
            x,
            y + static_cast<float>(i) * mLayout.detailLineHeight,
            color,
            scale,
            false);
    }
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Run journal timers and input.
// Why:
//   The overlay is read-only, so input only closes, scrolls, or switches tabs.
// ------------------------------------------------------------
void ExpeditionJournalState::Update(float dt)
{
    mElapsed += dt;

    if (Pressed(VK_ESCAPE, mEscWasDown) || Pressed(VK_BACK, mBackWasDown))
    {
        AudioManager::Get().PlaySfx("ui_back");
        StateManager::Get().PopState();
        return;
    }

    if (Pressed(VK_UP, mUpWasDown)) MoveCursor(-1);
    if (Pressed(VK_DOWN, mDownWasDown)) MoveCursor(1);
    if (Pressed(VK_LEFT, mLeftWasDown)) SwitchTab(-1);
    if (Pressed(VK_RIGHT, mRightWasDown)) SwitchTab(1);
}

void ExpeditionJournalState::RenderTabs(float panelX, float panelY)
{
    const float centerX = panelX + mLayout.panelWidth * 0.5f;
    const float routeX = centerX - mLayout.tabGap * 0.5f;
    const float memoryX = centerX + mLayout.tabGap * 0.5f;
    const DirectX::XMVECTOR active = MakeColor(mLayout.accentR, mLayout.accentG, mLayout.accentB, 1.0f);
    const DirectX::XMVECTOR inactive = MakeColor(mLayout.mutedR, mLayout.mutedG, mLayout.mutedB, 0.86f);

    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("journal.tab.route").c_str(),
        routeX,
        panelY + mLayout.tabY,
        mTab == Tab::Route ? active : inactive,
        1.0f,
        true);
    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("journal.tab.memories").c_str(),
        memoryX,
        panelY + mLayout.tabY,
        mTab == Tab::Memories ? active : inactive,
        1.0f,
        true);
}

void ExpeditionJournalState::RenderRouteList(float panelX, float panelY)
{
    const int visibleEnd = std::min(
        static_cast<int>(mObjectives.size()),
        mTopIndex + mLayout.maxVisibleRows);

    for (int i = mTopIndex; i < visibleEnd; ++i)
    {
        const ObjectiveRow& row = mObjectives[static_cast<std::size_t>(i)];
        const bool selected = i == mCursor;
        const float rowX = panelX + mLayout.listX;
        const float rowY = panelY + mLayout.listY +
            static_cast<float>(i - mTopIndex) * mLayout.rowHeight;
        const DirectX::XMVECTOR labelColor = row.status == RowStatus::Locked
            ? MakeColor(mLayout.lockedR, mLayout.lockedG, mLayout.lockedB, 1.0f)
            : MakeColor(mLayout.textR, mLayout.textG, mLayout.textB, 1.0f);

        if (selected)
        {
            mTextRenderer.DrawStringRaw(">", rowX - 34.0f, rowY, StatusColor(row.status));
        }

        mTextRenderer.DrawStringRaw(ObjectiveTitle(row).c_str(), rowX, rowY, labelColor);
        mTextRenderer.DrawStringRaw(MemoryStatusLabel(row.status).c_str(),
                                    rowX + mLayout.listWidth - 118.0f,
                                    rowY,
                                    StatusColor(row.status));
    }
}

void ExpeditionJournalState::RenderMemoryList(float panelX, float panelY)
{
    const int visibleEnd = std::min(
        static_cast<int>(mMemories.size()),
        mTopIndex + mLayout.maxVisibleRows);

    for (int i = mTopIndex; i < visibleEnd; ++i)
    {
        const MemoryRow& row = mMemories[static_cast<std::size_t>(i)];
        const bool selected = i == mCursor;
        const float rowX = panelX + mLayout.listX;
        const float rowY = panelY + mLayout.listY +
            static_cast<float>(i - mTopIndex) * mLayout.rowHeight;
        const DirectX::XMVECTOR labelColor = row.status == RowStatus::Locked
            ? MakeColor(mLayout.lockedR, mLayout.lockedG, mLayout.lockedB, 1.0f)
            : MakeColor(mLayout.textR, mLayout.textG, mLayout.textB, 1.0f);

        if (selected)
        {
            mTextRenderer.DrawStringRaw(">", rowX - 34.0f, rowY, StatusColor(row.status));
        }

        mTextRenderer.DrawStringRaw(MemoryName(row).c_str(), rowX, rowY, labelColor);
        mTextRenderer.DrawStringRaw(StatusLabel(row.status).c_str(),
                                    rowX + mLayout.listWidth - 118.0f,
                                    rowY,
                                    StatusColor(row.status));
    }
}

void ExpeditionJournalState::RenderDetailPanel(float panelX, float panelY)
{
    const float x = panelX + mLayout.detailX;
    const float y = panelY + mLayout.detailY;
    const float bodyWidth = mLayout.detailWidth - 48.0f;

    if (mTab == Tab::Route)
    {
        if (mObjectives.empty()) return;

        const ObjectiveRow& row = mObjectives[static_cast<std::size_t>(mCursor)];
        mTextRenderer.DrawStringRawScaled(
            ObjectiveTitle(row).c_str(),
            x,
            y,
            StatusColor(row.status),
            1.08f,
            true);
        mTextRenderer.DrawStringRaw(
            StatusLabel(row.status).c_str(),
            x,
            y + 38.0f,
            StatusColor(row.status));
        DrawWrappedText(
            WrapText(ObjectiveBody(row), bodyWidth, 0.92f),
            x,
            y + mLayout.detailBodyY,
            MakeColor(mLayout.textR, mLayout.textG, mLayout.textB, 0.94f),
            0.92f);
        return;
    }

    if (mMemories.empty()) return;

    const MemoryRow& row = mMemories[static_cast<std::size_t>(mCursor)];
    const std::string title = row.status == RowStatus::Complete
        ? MemoryName(row)
        : LocalizationManager::Get().Text("journal.memory.locked_title");
    const std::string body = row.status == RowStatus::Complete
        ? LocalizationManager::Get().Text("journal.memory.recovered_body")
        : LocalizationManager::Get().Text("journal.memory.locked_body");

    mTextRenderer.DrawStringRawScaled(title.c_str(), x, y, StatusColor(row.status), 1.08f, true);
    mTextRenderer.DrawStringRaw(MemoryStatusLabel(row.status).c_str(), x, y + 38.0f, StatusColor(row.status));
    DrawWrappedText(
        WrapText(body, bodyWidth, 0.92f),
        x,
        y + mLayout.detailBodyY,
        MakeColor(mLayout.textR, mLayout.textG, mLayout.textB, 0.94f),
        0.92f);
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the expedition journal overlay.
// Why:
//   StateManager renders the preserved screen beneath this overlay, so this
//   state only needs its dim layer, journal panel, rows, and detail view.
// ------------------------------------------------------------
void ExpeditionJournalState::Render()
{
    auto& d3d = D3DContext::Get();
    ID3D11DeviceContext* ctx = d3d.GetContext();
    const float screenW = static_cast<float>(d3d.GetWidth());
    const float screenH = static_cast<float>(d3d.GetHeight());
    const float panelX = (screenW - mLayout.panelWidth) * 0.5f;
    const float panelY = (screenH - mLayout.panelHeight) * 0.5f;
    const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();

    mDialogBox.Draw(ctx,
                    0.0f,
                    0.0f,
                    screenW,
                    screenH,
                    1.0f,
                    identity,
                    MakeColor(0.0f, 0.0f, 0.0f, mLayout.dimAlpha));
    mDialogBox.Draw(ctx,
                    panelX,
                    panelY,
                    mLayout.panelWidth,
                    mLayout.panelHeight,
                    1.0f,
                    identity,
                    MakeColor(1.0f, 1.0f, 1.0f, mLayout.panelAlpha));
    mDialogBox.Draw(ctx,
                    panelX + mLayout.detailX - 22.0f,
                    panelY + mLayout.detailY - 18.0f,
                    mLayout.detailWidth,
                    mLayout.detailHeight,
                    1.0f,
                    identity,
                    MakeColor(0.05f, 0.05f, 0.06f, mLayout.detailPanelAlpha));

    if (mIconSRV)
    {
        mIconRenderer.Draw(ctx,
                           mIconSRV.Get(),
                           panelX + mLayout.iconX,
                           panelY + mLayout.iconY,
                           mLayout.iconSize,
                           mLayout.iconSize,
                           identity);
    }

    mTextRenderer.BeginBatch(ctx);
    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("journal.title").c_str(),
        panelX + mLayout.panelWidth * 0.5f,
        panelY + mLayout.titleY,
        MakeColor(mLayout.textR, mLayout.textG, mLayout.textB, 1.0f),
        1.28f,
        true);

    const std::string subtitle = LocalizationManager::Get().Format(
        "journal.subtitle",
        {
            { "objectives", std::to_string(CompletedObjectiveCount()) },
            { "objectiveTotal", std::to_string(mObjectives.size()) },
            { "memories", std::to_string(RecoveredMemoryCount()) },
            { "memoryTotal", std::to_string(mMemories.size()) }
        });
    mTextRenderer.DrawStringCenteredRaw(subtitle.c_str(),
                                        panelX + mLayout.panelWidth * 0.5f,
                                        panelY + mLayout.subtitleY,
                                        MakeColor(mLayout.mutedR, mLayout.mutedG, mLayout.mutedB, 1.0f));

    RenderTabs(panelX, panelY);
    if (mTab == Tab::Route)
    {
        RenderRouteList(panelX, panelY);
    }
    else
    {
        RenderMemoryList(panelX, panelY);
    }
    RenderDetailPanel(panelX, panelY);

    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("journal.hint").c_str(),
        panelX + mLayout.panelWidth * 0.5f,
        panelY + mLayout.hintY,
        MakeColor(mLayout.mutedR, mLayout.mutedG, mLayout.mutedB, 1.0f));
    mTextRenderer.EndBatch();
}
