// ============================================================
// File: DialogueState.cpp
// Responsibility: Implement the linear story dialogue overlay.
//
// Architecture:
//   DialogueState owns presentation flow only. Dialogue data is loaded by
//   DialogueManager, story completion is saved by GameProgress, and the
//   overworld remains visible through StateManager overlay rendering.
//
// Common mistakes:
//   1. Mutating overworld state directly from dialogue lines.
//   2. Splitting localized UTF-8 strings by raw byte count.
//   3. Applying gameplay dt here; UI text reveal should run while gameplay
//      underneath is frozen by the state stack.
// ============================================================
#define NOMINMAX
#include "DialogueState.h"
#include "StateManager.h"
#include "../Audio/AudioManager.h"
#include "../Core/InputManager.h"
#include "../Core/TimeSystem.h"
#include "../Events/EventManager.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/GameProgress.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/Log.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr const char* kLayoutPath = "data/dialogue_layout.json";

    bool ConfirmPressed(const InputManager& input)
    {
        return input.IsKeyPressed(VK_RETURN) ||
               input.IsKeyPressed(VK_SPACE);
    }

    bool BackPressed(const InputManager& input)
    {
        return input.IsKeyPressed(VK_ESCAPE) ||
               input.IsKeyPressed(VK_BACK);
    }

    std::size_t Utf8GlyphCount(const std::string& text)
    {
        std::size_t count = 0;
        for (unsigned char c : text)
        {
            if ((c & 0xC0u) != 0x80u) ++count;
        }
        return count;
    }

    std::string Utf8PrefixByGlyphs(const std::string& text, std::size_t glyphCount)
    {
        if (glyphCount == 0) return std::string();

        std::size_t glyphsSeen = 0;
        std::size_t byteEnd = text.size();
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if ((c & 0xC0u) != 0x80u)
            {
                if (glyphsSeen == glyphCount)
                {
                    byteEnd = i;
                    break;
                }
                ++glyphsSeen;
            }
        }

        return text.substr(0, byteEnd);
    }
}

DialogueState::DialogueState(std::string scriptPath)
    : mScriptPath(std::move(scriptPath))
{
}

void DialogueState::OnEnter()
{
    auto& d3d = D3DContext::Get();
    mRenderer.Initialize(d3d.GetDevice(),
                         d3d.GetContext(),
                         kLayoutPath,
                         d3d.GetWidth(),
                         d3d.GetHeight());

    DialogueManager loader;
    if (!loader.LoadScript(mScriptPath, mScript))
    {
        LOG("[DialogueState] Failed to load script '%s'.",
            mScriptPath.c_str());
        mPendingClose = true;
        return;
    }

    mResizeListenerId = EventManager::Get().Subscribe("window_resized",
        [this](const EventData&)
        {
            mRenderer.SetScreenSize(D3DContext::Get().GetWidth(),
                                    D3DContext::Get().GetHeight());
        });

    mLineIndex = 0;
    mVisibleGlyphs = 0.0f;
    mElapsed = 0.0f;
    mReady = true;
    mPendingClose = false;

    LOG("[DialogueState] Opened script '%s'.", mScript.id.c_str());
}

void DialogueState::OnExit()
{
    if (mResizeListenerId != -1)
    {
        EventManager::Get().Unsubscribe("window_resized", mResizeListenerId);
        mResizeListenerId = -1;
    }

    mRenderer.Shutdown();
    LOG("[DialogueState] Closed.");
}

void DialogueState::Update(float)
{
    const float uiDt = TimeSystem::Get().GetUIClock().GetDeltaTime();
    mElapsed += uiDt;

    if (mPendingClose)
    {
        StateManager::Get().PopState();
        return;
    }

    if (!mReady || mScript.lines.empty())
    {
        mPendingClose = true;
        return;
    }

    if (!CurrentLineComplete())
    {
        mVisibleGlyphs += mRenderer.GetCharsPerSecond() * uiDt;
    }

    const InputManager& input = InputManager::Get();
    if (ConfirmPressed(input))
    {
        AdvanceOrReveal();
        return;
    }

    if (BackPressed(input) && mScript.skippable)
    {
        AudioManager::Get().PlaySfx(mRenderer.GetBackSfxId());
        CompleteDialogue();
    }
}

void DialogueState::Render()
{
    if (!mReady || mScript.lines.empty()) return;
    mRenderer.Render(D3DContext::Get().GetContext(), BuildRenderState());
}

void DialogueState::AdvanceOrReveal()
{
    if (!CurrentLineComplete())
    {
        mVisibleGlyphs = static_cast<float>(CurrentLineGlyphCount());
        AudioManager::Get().PlaySfx(mRenderer.GetConfirmSfxId());
        return;
    }

    AudioManager::Get().PlaySfx(mRenderer.GetConfirmSfxId());

    if (mLineIndex + 1 >= static_cast<int>(mScript.lines.size()))
    {
        CompleteDialogue();
        return;
    }

    ++mLineIndex;
    mVisibleGlyphs = 0.0f;
}

void DialogueState::CompleteDialogue()
{
    if (!mScript.completionFlag.empty())
    {
        GameProgress::Get().SetFlag(mScript.completionFlag);
    }

    EventData data{};
    data.name = mScript.id;
    EventManager::Get().Broadcast("dialogue_completed", data);

    mPendingClose = true;
}

DialogueRenderState DialogueState::BuildRenderState() const
{
    DialogueRenderState state{};
    const DialogueLine& line = mScript.lines[static_cast<std::size_t>(mLineIndex)];
    state.speakerName = line.speakerName;
    state.lineComplete = CurrentLineComplete();
    state.elapsed = mElapsed;
    state.prompt = LocalizationManager::Get().Text("dialogue.prompt.next");

    const std::size_t glyphsVisible =
        static_cast<std::size_t>(std::floor(std::min(
            mVisibleGlyphs,
            static_cast<float>(Utf8GlyphCount(line.text)))));
    state.text = Utf8PrefixByGlyphs(line.text, glyphsVisible);
    return state;
}

bool DialogueState::CurrentLineComplete() const
{
    return mVisibleGlyphs >= static_cast<float>(CurrentLineGlyphCount());
}

std::size_t DialogueState::CurrentLineGlyphCount() const
{
    if (mLineIndex < 0 || mLineIndex >= static_cast<int>(mScript.lines.size()))
    {
        return 0;
    }

    return Utf8GlyphCount(mScript.lines[static_cast<std::size_t>(mLineIndex)].text);
}
