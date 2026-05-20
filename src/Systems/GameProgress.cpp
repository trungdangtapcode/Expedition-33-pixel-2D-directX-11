// ============================================================
// File: GameProgress.cpp
// Responsibility: Implement durable world/story progress flags.
// ============================================================
#include "GameProgress.h"
#include <algorithm>

void GameProgress::Reset()
{
    mFlags.clear();
}

bool GameProgress::HasFlag(const std::string& id) const
{
    return mFlags.find(id) != mFlags.end();
}

void GameProgress::SetFlag(const std::string& id)
{
    if (id.empty()) return;
    mFlags.insert(id);
}

std::vector<std::string> GameProgress::CaptureFlags() const
{
    std::vector<std::string> flags;
    flags.reserve(mFlags.size());

    for (const std::string& id : mFlags)
    {
        flags.push_back(id);
    }

    std::sort(flags.begin(), flags.end());
    return flags;
}

void GameProgress::ReplaceFlags(const std::vector<std::string>& flags)
{
    mFlags.clear();

    for (const std::string& id : flags)
    {
        SetFlag(id);
    }
}
