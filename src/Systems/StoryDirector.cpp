// ============================================================
// File: StoryDirector.cpp
// Responsibility: Load story events and translate satisfied triggers
//                 into executable overworld commands.
// ============================================================
#define NOMINMAX
#include "StoryDirector.h"
#include "GameProgress.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    std::filesystem::path ResolveReadablePath(const std::string& path)
    {
        namespace fs = std::filesystem;

        fs::path direct(path);
        if (fs::exists(direct)) return direct;

        fs::path parent = fs::path("..") / path;
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

    std::string ReadString(const std::string& src, const std::string& key)
    {
        return JsonLoader::detail::CleanString(JsonLoader::detail::ValueOf(src, key));
    }

    StoryDirector::TriggerKind ParseTriggerKind(const std::string& value)
    {
        if (value == "on_load") return StoryDirector::TriggerKind::OnLoad;
        if (value == "dialogue_completed") return StoryDirector::TriggerKind::DialogueCompleted;
        if (value == "battle_victory") return StoryDirector::TriggerKind::BattleVictory;
        if (value == "battle_defeat") return StoryDirector::TriggerKind::BattleDefeat;
        return StoryDirector::TriggerKind::EnterArea;
    }

    StoryCommandType ParseCommandType(const std::string& value)
    {
        if (value == "start_dialogue") return StoryCommandType::StartDialogue;
        if (value == "start_battle") return StoryCommandType::StartBattle;
        if (value == "recruit_member") return StoryCommandType::RecruitMember;
        if (value == "save_checkpoint") return StoryCommandType::SaveCheckpoint;
        if (value == "push_player") return StoryCommandType::PushPlayer;
        return StoryCommandType::SetFlag;
    }

    bool HasFlag(const std::string& flag)
    {
        return !flag.empty() && GameProgress::Get().HasFlag(flag);
    }
}

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Load story event definitions from disk and reset queued commands.
// Why:
//   OverworldState can be recreated after save/load, so story trigger data
//   must be rebuilt from authoritative JSON each time.
// ------------------------------------------------------------
bool StoryDirector::Initialize(const std::string& path)
{
    mEvents.clear();
    mQueuedCommands.clear();

    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[StoryDirector] ERROR: Could not read story event file '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    const bool loaded = LoadEventsFromSource(src, path);
    if (loaded)
    {
        EvaluateTrigger(TriggerKind::OnLoad, "");
    }
    return loaded;
}

// ------------------------------------------------------------
// Function: NotifyDialogueCompleted
// Purpose:
//   Evaluate events waiting for a specific dialogue id.
// Why:
//   DialogueState broadcasts only generic completion; StoryDirector maps that
//   reusable signal to story-specific next commands.
// ------------------------------------------------------------
void StoryDirector::NotifyDialogueCompleted(const std::string& dialogueId)
{
    EvaluateTrigger(TriggerKind::DialogueCompleted, dialogueId);
}

// ------------------------------------------------------------
// Function: NotifyBattleVictory
// Purpose:
//   Evaluate events waiting for a named story battle victory.
// Why:
//   Normal overworld enemy wins and bespoke story wins share the battle
//   result event, but only storyBattleId should advance cutscene chains.
// ------------------------------------------------------------
void StoryDirector::NotifyBattleVictory(const std::string& storyBattleId)
{
    EvaluateTrigger(TriggerKind::BattleVictory, storyBattleId);
}

// ------------------------------------------------------------
// Function: NotifyBattleDefeat
// Purpose:
//   Evaluate events waiting for a named story battle defeat or flee.
// Why:
//   The Maelle duel leave path must reset player position without marking
//   victory or recruitment flags.
// ------------------------------------------------------------
void StoryDirector::NotifyBattleDefeat(const std::string& storyBattleId)
{
    EvaluateTrigger(TriggerKind::BattleDefeat, storyBattleId);
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Check area-triggered story events against the current player position.
// Why:
//   Area triggers belong in the overworld tick, while the resulting commands
//   remain queued until OverworldState is ready to execute them.
// ------------------------------------------------------------
void StoryDirector::Update(float playerX, float playerY)
{
    for (StoryEvent& event : mEvents)
    {
        if (event.trigger != TriggerKind::EnterArea) continue;
        if (!IsInsideArea(event, playerX, playerY)) continue;
        TryFireEvent(event);
    }
}

// ------------------------------------------------------------
// Function: ConsumeCommands
// Purpose:
//   Move queued commands to the caller and empty the internal queue.
// Why:
//   OverworldState owns execution timing; StoryDirector only records what
//   should happen after a trigger fires.
// ------------------------------------------------------------
std::vector<StoryCommand> StoryDirector::ConsumeCommands()
{
    std::vector<StoryCommand> commands = std::move(mQueuedCommands);
    mQueuedCommands.clear();
    return commands;
}

// ------------------------------------------------------------
// Function: LoadEventsFromSource
// Purpose:
//   Parse story event objects and their command lists from JSON text.
// Why:
//   Keeping the schema flat and shallow matches the project's lightweight
//   JsonLoader constraints without adding a new dependency.
// ------------------------------------------------------------
bool StoryDirector::LoadEventsFromSource(const std::string& src, const std::string& path)
{
    const std::vector<std::string> eventObjects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "events");

    for (const std::string& eventSrc : eventObjects)
    {
        StoryEvent event{};
        event.id = ReadString(eventSrc, "id");
        event.trigger = ParseTriggerKind(ReadString(eventSrc, "trigger"));
        event.triggerId = ReadString(eventSrc, "triggerId");
        event.onceFlag = ReadString(eventSrc, "onceFlag");

        event.requiresFlags =
            JsonLoader::detail::ExtractStringArray(eventSrc, "requiresFlags");
        event.blockedByFlags =
            JsonLoader::detail::ExtractStringArray(eventSrc, "blockedByFlags");

        const std::string singleRequired = ReadString(eventSrc, "requiresFlag");
        if (!singleRequired.empty()) event.requiresFlags.push_back(singleRequired);

        const std::string singleBlocked = ReadString(eventSrc, "blockedByFlag");
        if (!singleBlocked.empty()) event.blockedByFlags.push_back(singleBlocked);

        event.minX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(eventSrc, "minX"), 0.0f);
        event.minY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(eventSrc, "minY"), 0.0f);
        event.maxX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(eventSrc, "maxX"), 0.0f);
        event.maxY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(eventSrc, "maxY"), 0.0f);

        const std::vector<std::string> commandObjects =
            JsonLoader::detail::ExtractObjectsFromArray(eventSrc, "commands");
        for (const std::string& commandSrc : commandObjects)
        {
            StoryCommand command{};
            command.type = ParseCommandType(ReadString(commandSrc, "type"));
            command.dialoguePath = ReadString(commandSrc, "dialoguePath");
            command.encounterPath = ReadString(commandSrc, "encounterPath");
            command.storyBattleId = ReadString(commandSrc, "storyBattleId");
            command.memberId = ReadString(commandSrc, "memberId");
            command.flagId = ReadString(commandSrc, "flagId");
            command.saveReason = ReadString(commandSrc, "saveReason");
            command.x = JsonLoader::detail::ParseFloat(
                JsonLoader::detail::ValueOf(commandSrc, "x"), 0.0f);
            command.y = JsonLoader::detail::ParseFloat(
                JsonLoader::detail::ValueOf(commandSrc, "y"), 0.0f);
            event.commands.push_back(std::move(command));
        }

        if (event.id.empty() || event.commands.empty())
        {
            LOG("[StoryDirector] WARNING: Skipping invalid story event in '%s'.",
                path.c_str());
            continue;
        }

        mEvents.push_back(std::move(event));
    }

    LOG("[StoryDirector] Loaded %zu story event(s) from '%s'.",
        mEvents.size(),
        path.c_str());
    return !mEvents.empty();
}

// ------------------------------------------------------------
// Function: EvaluateTrigger
// Purpose:
//   Find events matching a non-area trigger and try to fire them.
// Why:
//   Dialogue and battle notifications are edge events, so they should not be
//   polled from Update().
// ------------------------------------------------------------
void StoryDirector::EvaluateTrigger(TriggerKind trigger, const std::string& triggerId)
{
    for (StoryEvent& event : mEvents)
    {
        if (event.trigger != trigger) continue;
        if (!event.triggerId.empty() && event.triggerId != triggerId) continue;
        TryFireEvent(event);
    }
}

// ------------------------------------------------------------
// Function: TryFireEvent
// Purpose:
//   Gate an event by flags, apply its once flag, and queue its commands.
// Why:
//   Flag checks prevent repeated cutscenes, while queuing commands preserves
//   state-stack safety for the caller.
// ------------------------------------------------------------
void StoryDirector::TryFireEvent(StoryEvent& event)
{
    if (!RequirementsMet(event)) return;

    if (!event.onceFlag.empty())
    {
        GameProgress::Get().SetFlag(event.onceFlag);
    }

    for (const StoryCommand& command : event.commands)
    {
        mQueuedCommands.push_back(command);
    }

    LOG("[StoryDirector] Fired story event '%s'.", event.id.c_str());
}

// ------------------------------------------------------------
// Function: RequirementsMet
// Purpose:
//   Validate required, blocked, and once-only flags for a story event.
// Why:
//   Story progression must survive save/load and avoid runtime-only booleans.
// ------------------------------------------------------------
bool StoryDirector::RequirementsMet(const StoryEvent& event) const
{
    if (HasFlag(event.onceFlag)) return false;

    for (const std::string& flag : event.requiresFlags)
    {
        if (!HasFlag(flag)) return false;
    }

    for (const std::string& flag : event.blockedByFlags)
    {
        if (HasFlag(flag)) return false;
    }

    return true;
}

// ------------------------------------------------------------
// Function: IsInsideArea
// Purpose:
//   Test whether the player is inside one authored trigger rectangle.
// Why:
//   Rectangles are easy to tune in JSON and match the overworld's existing
//   collision/object authoring style.
// ------------------------------------------------------------
bool StoryDirector::IsInsideArea(const StoryEvent& event, float playerX, float playerY) const
{
    return playerX >= event.minX && playerX <= event.maxX &&
           playerY >= event.minY && playerY <= event.maxY;
}
