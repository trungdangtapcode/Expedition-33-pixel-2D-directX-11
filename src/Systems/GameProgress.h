// ============================================================
// File: GameProgress.h
// Responsibility: Store durable story/world progress for save/load.
//
// Design:
//   Systems that need one-time world actions, such as a campfire upgrade,
//   write stable string flags here. SaveManager serializes those flags so
//   reloading cannot duplicate one-time rewards.
//   The overworld snapshot is kept here as the hand-off between SaveManager
//   and OverworldState; states read it after a load instead of parsing files.
//
// Lifetime:
//   Created on first Get() call as a Meyers singleton.
//   Reset by New Game and replaced by checkpoint loads.
// ============================================================
#pragma once

#include <string>
#include <unordered_set>
#include <vector>

struct OverworldProgressSnapshot
{
    std::string sceneId;
    std::string checkpointId;
    float playerX = 0.0f;
    float playerY = 0.0f;
    bool hasPlayerPosition = false;
};

class GameProgress
{
public:
    static GameProgress& Get()
    {
        static GameProgress instance;
        return instance;
    }

    void Reset();
    bool HasFlag(const std::string& id) const;
    void SetFlag(const std::string& id);

    std::vector<std::string> CaptureFlags() const;
    void ReplaceFlags(const std::vector<std::string>& flags);

    void SetOverworldSnapshot(const OverworldProgressSnapshot& snapshot);
    OverworldProgressSnapshot CaptureOverworldSnapshot() const;
    void ReplaceOverworldSnapshot(const OverworldProgressSnapshot& snapshot);

private:
    GameProgress() = default;

    GameProgress(const GameProgress&) = delete;
    GameProgress& operator=(const GameProgress&) = delete;

    std::unordered_set<std::string> mFlags;
    OverworldProgressSnapshot mOverworldSnapshot;
};
