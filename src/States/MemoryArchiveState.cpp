// ============================================================
// File: MemoryArchiveState.cpp
// Responsibility: Implement the campfire memory archive screen.
//
// Architecture:
//   MemoryArchiveState is a read-only archive overlay. It loads authored shard
//   metadata, checks durable progress flags, and pushes DialogueState for replay.
//   It never grants collection rewards or mutates overworld entities.
//
// Common mistakes:
//   1. Replaying a memory by re-running OverworldState collection logic would
//      duplicate coin and item rewards.
//   2. Hiding locked rows entirely removes long-term goals from the player.
//   3. Keeping layout constants in code makes future UI tuning require builds.
// ============================================================
#define NOMINMAX
#include "MemoryArchiveState.h"
#include "DialogueState.h"
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
#include <memory>
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
//   Initialize archive UI resources and load current memory metadata.
// Why:
//   The state owns GPU resources only while the archive is visible, and
//   reloading entries on enter reflects the latest GameProgress flags.
// ------------------------------------------------------------
void MemoryArchiveState::OnEnter()
{
    auto& d3d = D3DContext::Get();

    LoadLayout("data/memory_archive_layout.json");
    LoadEntries("data/overworld_memory_shards.json");
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
    LoadIcon(d3d.GetDevice(), d3d.GetContext(), "assets/UI/memory_archive_icon.png");

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
    mFlashTimer = 0.0f;
    mFlashMessage.clear();

    LOG("[MemoryArchiveState] Opened with %d recovered memory entries.",
        CollectedCount());
}

// ------------------------------------------------------------
// Function: OnExit
// Purpose:
//   Release archive UI resources and event subscriptions.
// Why:
//   State-owned callbacks and GPU resources must not outlive the overlay.
// ------------------------------------------------------------
void MemoryArchiveState::OnExit()
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
    LOG("[MemoryArchiveState] Closed.");
}

// ------------------------------------------------------------
// Function: Pressed
// Purpose:
//   Convert raw keyboard state into one-press menu edges.
// Why:
//   Holding a key must not repeatedly move the archive cursor.
// ------------------------------------------------------------
bool MemoryArchiveState::Pressed(int vk, bool& wasDown)
{
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool fresh = down && !wasDown;
    wasDown = down;
    return fresh;
}

// ------------------------------------------------------------
// Function: LoadLayout
// Purpose:
//   Load archive screen dimensions from JSON.
// Why:
//   UI composition needs tuning without recompiling C++.
// ------------------------------------------------------------
bool MemoryArchiveState::LoadLayout(const std::string& path)
{
    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[MemoryArchiveState] WARNING: Missing layout '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    mLayout.panelWidth = ReadFloat(src, "panelWidth", mLayout.panelWidth);
    mLayout.panelHeight = ReadFloat(src, "panelHeight", mLayout.panelHeight);
    mLayout.titleY = ReadFloat(src, "titleY", mLayout.titleY);
    mLayout.subtitleY = ReadFloat(src, "subtitleY", mLayout.subtitleY);
    mLayout.iconX = ReadFloat(src, "iconX", mLayout.iconX);
    mLayout.iconY = ReadFloat(src, "iconY", mLayout.iconY);
    mLayout.iconSize = ReadFloat(src, "iconSize", mLayout.iconSize);
    mLayout.listX = ReadFloat(src, "listX", mLayout.listX);
    mLayout.listY = ReadFloat(src, "listY", mLayout.listY);
    mLayout.listWidth = ReadFloat(src, "listWidth", mLayout.listWidth);
    mLayout.rowHeight = ReadFloat(src, "rowHeight", mLayout.rowHeight);
    mLayout.maxVisibleRows = std::max(1, ReadInt(src, "maxVisibleRows", mLayout.maxVisibleRows));
    mLayout.detailX = ReadFloat(src, "detailX", mLayout.detailX);
    mLayout.detailY = ReadFloat(src, "detailY", mLayout.detailY);
    mLayout.detailWidth = ReadFloat(src, "detailWidth", mLayout.detailWidth);
    mLayout.detailHeight = ReadFloat(src, "detailHeight", mLayout.detailHeight);
    mLayout.hintY = ReadFloat(src, "hintY", mLayout.hintY);
    mLayout.flashY = ReadFloat(src, "flashY", mLayout.flashY);
    mLayout.flashDuration = ReadFloat(src, "flashDuration", mLayout.flashDuration);
    return true;
}

// ------------------------------------------------------------
// Function: LoadEntries
// Purpose:
//   Load memory shard metadata into archive rows.
// Why:
//   The archive should automatically reflect future shard additions without
//   hardcoded C++ lists.
// ------------------------------------------------------------
bool MemoryArchiveState::LoadEntries(const std::string& path)
{
    mEntries.clear();

    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[MemoryArchiveState] WARNING: Missing archive data '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "shards");

    for (const std::string& objectSrc : objects)
    {
        Entry entry{};
        entry.id = ReadString(objectSrc, "id");
        entry.displayNameKey = ReadString(objectSrc, "displayNameKey");
        entry.displayNameFallback = ReadString(objectSrc, "displayName", "Memory");
        entry.collectedFlag = ReadString(objectSrc, "collectedFlag");
        entry.dialoguePath = ReadString(objectSrc, "dialoguePath");

        if (entry.id.empty() || entry.collectedFlag.empty())
        {
            LOG("[MemoryArchiveState] WARNING: Skipping invalid memory archive entry.");
            continue;
        }

        mEntries.push_back(std::move(entry));
    }

    return !mEntries.empty();
}

// ------------------------------------------------------------
// Function: LoadIcon
// Purpose:
//   Load the generated archive emblem into a shader resource view.
// Why:
//   The icon is a project asset, but this state owns only the GPU view
//   while the archive overlay is open.
// ------------------------------------------------------------
bool MemoryArchiveState::LoadIcon(ID3D11Device* device,
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
        LOG("[MemoryArchiveState] WARNING: Failed to load icon '%s'.", path.c_str());
        return false;
    }

    return true;
}

// ------------------------------------------------------------
// Function: ClampCursor
// Purpose:
//   Keep cursor and scroll window inside the loaded entry list.
// Why:
//   The archive list can grow or shrink as data changes.
// ------------------------------------------------------------
void MemoryArchiveState::ClampCursor()
{
    if (mEntries.empty())
    {
        mCursor = 0;
        mTopIndex = 0;
        return;
    }

    mCursor = std::max(0, std::min(mCursor, static_cast<int>(mEntries.size()) - 1));
    if (mCursor < mTopIndex) mTopIndex = mCursor;
    if (mCursor >= mTopIndex + mLayout.maxVisibleRows)
    {
        mTopIndex = mCursor - mLayout.maxVisibleRows + 1;
    }
    mTopIndex = std::max(0, std::min(mTopIndex, static_cast<int>(mEntries.size()) - 1));
}

// ------------------------------------------------------------
// Function: MoveCursor
// Purpose:
//   Move the selected archive row with wraparound.
// Why:
//   Menu navigation should stay fast even as the archive grows.
// ------------------------------------------------------------
void MemoryArchiveState::MoveCursor(int direction)
{
    if (mEntries.empty()) return;

    const int count = static_cast<int>(mEntries.size());
    mCursor = (mCursor + direction + count) % count;
    ClampCursor();
    AudioManager::Get().PlaySfx("ui_navigate");
}

// ------------------------------------------------------------
// Function: ActivateSelection
// Purpose:
//   Replay the selected recovered memory or explain why it is locked.
// Why:
//   The archive is read-only, so selection should never grant rewards.
// ------------------------------------------------------------
void MemoryArchiveState::ActivateSelection()
{
    if (mEntries.empty()) return;

    const Entry& entry = mEntries[static_cast<std::size_t>(mCursor)];
    if (!IsCollected(entry))
    {
        Flash(LocalizationManager::Get().Text("memory_archive.flash.locked"));
        AudioManager::Get().PlaySfx("battle_no_ap");
        return;
    }

    if (entry.dialoguePath.empty())
    {
        Flash(LocalizationManager::Get().Text("memory_archive.flash.no_dialogue"));
        AudioManager::Get().PlaySfx("battle_no_ap");
        return;
    }

    AudioManager::Get().PlaySfx("ui_confirm");
    StateManager::Get().PushState(std::make_unique<DialogueState>(entry.dialoguePath));
}

// ------------------------------------------------------------
// Function: Flash
// Purpose:
//   Show short feedback for locked or invalid selections.
// Why:
//   The player should understand that locked memories are goals, not bugs.
// ------------------------------------------------------------
void MemoryArchiveState::Flash(const std::string& message)
{
    mFlashMessage = message;
    mFlashTimer = mLayout.flashDuration;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance archive timers and handle menu input.
// Why:
//   While this overlay is active, lower campfire gameplay is frozen by the
//   state stack and this state owns input.
// ------------------------------------------------------------
void MemoryArchiveState::Update(float dt)
{
    mElapsed += dt;
    if (mFlashTimer > 0.0f)
    {
        mFlashTimer = std::max(0.0f, mFlashTimer - dt);
    }

    if (Pressed(VK_ESCAPE, mEscWasDown) || Pressed(VK_BACK, mBackWasDown))
    {
        AudioManager::Get().PlaySfx("ui_back");
        StateManager::Get().PopState();
        return;
    }

    if (Pressed(VK_UP, mUpWasDown)) MoveCursor(-1);
    if (Pressed(VK_DOWN, mDownWasDown)) MoveCursor(1);
    if (Pressed(VK_RETURN, mEnterWasDown)) ActivateSelection();
}

// ------------------------------------------------------------
// Function: IsCollected
// Purpose:
//   Check whether a memory row has been recovered in durable progress.
// Why:
//   Save/load owns collection persistence through GameProgress flags.
// ------------------------------------------------------------
bool MemoryArchiveState::IsCollected(const Entry& entry) const
{
    return GameProgress::Get().HasFlag(entry.collectedFlag);
}

// ------------------------------------------------------------
// Function: EntryName
// Purpose:
//   Resolve the localized name for one recovered memory.
// Why:
//   Player-facing text belongs in localization data with English fallback.
// ------------------------------------------------------------
std::string MemoryArchiveState::EntryName(const Entry& entry) const
{
    return LocalizationManager::Get().TextOrFallback(
        entry.displayNameKey,
        entry.displayNameFallback);
}

// ------------------------------------------------------------
// Function: CollectedCount
// Purpose:
//   Count recovered memory rows.
// Why:
//   The archive subtitle shows progression without creating a new save field.
// ------------------------------------------------------------
int MemoryArchiveState::CollectedCount() const
{
    int count = 0;
    for (const Entry& entry : mEntries)
    {
        if (IsCollected(entry)) ++count;
    }
    return count;
}

// ------------------------------------------------------------
// Function: RenderEntryList
// Purpose:
//   Draw the scrollable memory row list.
// Why:
//   Locked rows preserve long-term discovery goals while recovered rows allow
//   dialogue replay.
// ------------------------------------------------------------
void MemoryArchiveState::RenderEntryList(float panelX, float panelY)
{
    const int visibleEnd = std::min(
        static_cast<int>(mEntries.size()),
        mTopIndex + mLayout.maxVisibleRows);

    for (int i = mTopIndex; i < visibleEnd; ++i)
    {
        const Entry& entry = mEntries[static_cast<std::size_t>(i)];
        const bool selected = (i == mCursor);
        const bool collected = IsCollected(entry);
        const float rowX = panelX + mLayout.listX;
        const float rowY = panelY + mLayout.listY +
            static_cast<float>(i - mTopIndex) * mLayout.rowHeight;

        const std::string label = collected
            ? EntryName(entry)
            : LocalizationManager::Get().Text("memory_archive.locked_entry");
        const std::string status = collected
            ? LocalizationManager::Get().Text("memory_archive.recovered")
            : LocalizationManager::Get().Text("memory_archive.locked");
        const DirectX::XMVECTOR labelColor = selected
            ? DirectX::Colors::PaleGoldenrod
            : (collected ? DirectX::Colors::White : DirectX::Colors::Gray);
        const DirectX::XMVECTOR statusColor = collected
            ? DirectX::Colors::PaleGreen
            : DirectX::Colors::DarkGray;

        if (selected)
        {
            mTextRenderer.DrawStringRaw(">", rowX - 38.0f, rowY, DirectX::Colors::PaleGoldenrod);
        }
        mTextRenderer.DrawStringRaw(label.c_str(), rowX, rowY, labelColor);
        mTextRenderer.DrawStringRaw(status.c_str(),
                                    rowX + mLayout.listWidth - 116.0f,
                                    rowY,
                                    statusColor);
    }
}

// ------------------------------------------------------------
// Function: RenderDetailPanel
// Purpose:
//   Draw selected memory details and replay affordance.
// Why:
//   The archive should explain exactly what pressing Enter will do.
// ------------------------------------------------------------
void MemoryArchiveState::RenderDetailPanel(float panelX, float panelY)
{
    if (mEntries.empty())
    {
        mTextRenderer.DrawStringRaw(
            LocalizationManager::Get().Text("memory_archive.empty").c_str(),
            panelX + mLayout.detailX,
            panelY + mLayout.detailY,
            DirectX::Colors::Silver);
        return;
    }

    const Entry& entry = mEntries[static_cast<std::size_t>(mCursor)];
    const bool collected = IsCollected(entry);
    const float x = panelX + mLayout.detailX;
    const float y = panelY + mLayout.detailY;

    const std::string name = collected
        ? EntryName(entry)
        : LocalizationManager::Get().Text("memory_archive.locked_entry");
    const std::string body = collected
        ? LocalizationManager::Get().Text("memory_archive.detail_replay")
        : LocalizationManager::Get().Text("memory_archive.detail_locked");
    const std::string hint = collected
        ? LocalizationManager::Get().Text("memory_archive.detail_hint_replay")
        : LocalizationManager::Get().Text("memory_archive.detail_hint_locked");

    mTextRenderer.DrawStringRawScaled(name.c_str(), x, y, DirectX::Colors::PaleGoldenrod, 1.08f, true);
    mTextRenderer.DrawStringRaw(body.c_str(), x, y + 54.0f, DirectX::Colors::LightGray);
    mTextRenderer.DrawStringRaw(hint.c_str(), x, y + 112.0f, DirectX::Colors::Silver);
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the archive overlay.
// Why:
//   StateManager renders the campfire beneath this state, so the archive
//   only needs its own dim layer and panel.
// ------------------------------------------------------------
void MemoryArchiveState::Render()
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
                    DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.55f));
    mDialogBox.Draw(ctx,
                    panelX,
                    panelY,
                    mLayout.panelWidth,
                    mLayout.panelHeight,
                    1.0f,
                    identity,
                    DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.96f));

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

    mDialogBox.Draw(ctx,
                    panelX + mLayout.detailX - 22.0f,
                    panelY + mLayout.detailY - 18.0f,
                    mLayout.detailWidth,
                    mLayout.detailHeight,
                    1.0f,
                    identity,
                    DirectX::XMVectorSet(0.05f, 0.05f, 0.06f, 0.72f));

    mTextRenderer.BeginBatch(ctx);
    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("memory_archive.title").c_str(),
        panelX + mLayout.panelWidth * 0.5f,
        panelY + mLayout.titleY,
        DirectX::Colors::White,
        1.28f,
        true);

    const std::string subtitle = LocalizationManager::Get().Format(
        "memory_archive.subtitle",
        {
            { "count", std::to_string(CollectedCount()) },
            { "total", std::to_string(mEntries.size()) }
        });
    mTextRenderer.DrawStringCenteredRaw(subtitle.c_str(),
                                        panelX + mLayout.panelWidth * 0.5f,
                                        panelY + mLayout.subtitleY,
                                        DirectX::Colors::LightGray);

    RenderEntryList(panelX, panelY);
    RenderDetailPanel(panelX, panelY);

    if (mFlashTimer > 0.0f && !mFlashMessage.empty())
    {
        mTextRenderer.DrawStringCenteredRaw(mFlashMessage.c_str(),
                                            panelX + mLayout.panelWidth * 0.5f,
                                            panelY + mLayout.flashY,
                                            DirectX::Colors::PaleGreen);
    }

    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("memory_archive.hint").c_str(),
        panelX + mLayout.panelWidth * 0.5f,
        panelY + mLayout.hintY,
        DirectX::Colors::Silver);
    mTextRenderer.EndBatch();
}
