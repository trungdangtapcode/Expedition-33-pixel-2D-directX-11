// ============================================================
// File: GameProgress.cpp
// Responsibility: Implement durable world/story progress storage.
// ============================================================
#include "GameProgress.h"
#include <algorithm>

// ------------------------------------------------------------
// Function: Reset
// Purpose:
//   Clear every durable progress value for a fresh new game.
// Why:
//   Starting from title must not inherit defeated enemies, campfire rewards,
//   or a previous slot's saved overworld position.
// ------------------------------------------------------------
void GameProgress::Reset()
{
    mFlags.clear();
    mOverworldSnapshot = OverworldProgressSnapshot{};
}

// ------------------------------------------------------------
// Function: HasFlag
// Purpose:
//   Check whether a stable world-progress flag is already active.
// Why:
//   Overworld systems need a small, saveable way to gate one-time rewards
//   and defeated enemy spawns without coupling to save-file parsing.
// ------------------------------------------------------------
bool GameProgress::HasFlag(const std::string& id) const
{
    return mFlags.find(id) != mFlags.end();
}

// ------------------------------------------------------------
// Function: SetFlag
// Purpose:
//   Record a stable world-progress flag.
// Why:
//   Empty ids are ignored so accidental missing data cannot create an
//   ambiguous save-file entry.
// ------------------------------------------------------------
void GameProgress::SetFlag(const std::string& id)
{
    if (id.empty()) return;
    mFlags.insert(id);
}

// ------------------------------------------------------------
// Function: CaptureFlags
// Purpose:
//   Return a deterministic list of progress flags for SaveManager.
// Why:
//   Sorting keeps save-file diffs stable and makes debugging slots easier.
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// Function: ReplaceFlags
// Purpose:
//   Replace all world-progress flags from a loaded save slot.
// Why:
//   Loading must be authoritative; stale flags from the previous runtime
//   should not survive after a different slot is restored.
// ------------------------------------------------------------
void GameProgress::ReplaceFlags(const std::vector<std::string>& flags)
{
    mFlags.clear();

    for (const std::string& id : flags)
    {
        SetFlag(id);
    }
}

// ------------------------------------------------------------
// Function: SetOverworldSnapshot
// Purpose:
//   Store the latest saveable overworld scene, checkpoint, and position.
// Why:
//   SaveManager serializes this snapshot while OverworldState owns the live
//   player entity and knows when the position should change.
// ------------------------------------------------------------
void GameProgress::SetOverworldSnapshot(const OverworldProgressSnapshot& snapshot)
{
    mOverworldSnapshot = snapshot;
}

// ------------------------------------------------------------
// Function: CaptureOverworldSnapshot
// Purpose:
//   Return the current overworld save snapshot.
// Why:
//   SaveManager needs a state-neutral way to write world restore metadata.
// ------------------------------------------------------------
OverworldProgressSnapshot GameProgress::CaptureOverworldSnapshot() const
{
    return mOverworldSnapshot;
}

// ------------------------------------------------------------
// Function: ReplaceOverworldSnapshot
// Purpose:
//   Replace the overworld snapshot with data loaded from a save slot.
// Why:
//   OverworldState reads this value on entry so a loaded slot rebuilds at
//   the correct checkpoint instead of the map default.
// ------------------------------------------------------------
void GameProgress::ReplaceOverworldSnapshot(const OverworldProgressSnapshot& snapshot)
{
    mOverworldSnapshot = snapshot;
}
