// ============================================================
// File: GameProgress.h
// Responsibility: Store durable story/world progress flags for save/load.
//
// Design:
//   Systems that need one-time world actions, such as a campfire upgrade,
//   write stable string flags here. SaveManager serializes those flags so
//   reloading cannot duplicate one-time rewards.
//
// Lifetime:
//   Created on first Get() call as a Meyers singleton.
//   Reset by New Game and replaced by checkpoint loads.
// ============================================================
#pragma once

#include <string>
#include <unordered_set>
#include <vector>

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

private:
    GameProgress() = default;

    GameProgress(const GameProgress&) = delete;
    GameProgress& operator=(const GameProgress&) = delete;

    std::unordered_set<std::string> mFlags;
};
