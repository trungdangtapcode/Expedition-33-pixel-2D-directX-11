// ============================================================
// File: StoryDirector.h
// Responsibility: Evaluate data-driven story triggers and queue
//                 reusable overworld commands.
//
// Architecture:
//   StoryDirector does not push states, spawn entities, or mutate renderers.
//   It observes durable GameProgress flags plus explicit dialogue/battle
//   notifications, then emits StoryCommand values for OverworldState.
//
// Why:
//   DialogueState and BattleState should remain generic screens. Keeping the
//   story sequence in data avoids coupling those states to the Maelle duel.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter()
//   Destroyed in -> OverworldState::OnExit()
// ============================================================
#pragma once

#include <string>
#include <vector>

enum class StoryCommandType
{
    StartDialogue,
    StartBattle,
    RecruitMember,
    SetFlag,
    SaveCheckpoint,
    PushPlayer,
    GrantCoins,
    GrantItem,
    SetPlayerControl,
    MovePlayer,
    FocusCamera,
    Wait
};

struct StoryCommand
{
    StoryCommandType type = StoryCommandType::SetFlag;
    std::string dialoguePath;
    std::string encounterPath;
    std::string storyBattleId;
    std::string memberId;
    std::string flagId;
    std::string saveReason;
    std::string itemId;
    int amount = 0;
    float x = 0.0f;
    float y = 0.0f;
    float duration = 0.0f;
    bool enabled = true;
};

class StoryDirector
{
public:
    enum class TriggerKind
    {
        OnLoad,
        EnterArea,
        DialogueCompleted,
        BattleVictory,
        BattleDefeat
    };

    bool Initialize(const std::string& path);

    void NotifyDialogueCompleted(const std::string& dialogueId);
    void NotifyBattleVictory(const std::string& storyBattleId);
    void NotifyBattleDefeat(const std::string& storyBattleId);

    void Update(float playerX, float playerY);
    std::vector<StoryCommand> ConsumeCommands();

private:
    struct StoryEvent
    {
        std::string id;
        TriggerKind trigger = TriggerKind::EnterArea;
        std::string triggerId;
        std::vector<std::string> requiresFlags;
        std::vector<std::string> blockedByFlags;
        std::string onceFlag;
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        std::vector<StoryCommand> commands;
    };

    bool LoadEventsFromSource(const std::string& src, const std::string& path);
    void EvaluateTrigger(TriggerKind trigger, const std::string& triggerId);
    void TryFireEvent(StoryEvent& event);
    bool RequirementsMet(const StoryEvent& event) const;
    bool IsInsideArea(const StoryEvent& event, float playerX, float playerY) const;

    std::vector<StoryEvent> mEvents;
    std::vector<StoryCommand> mQueuedCommands;
};
