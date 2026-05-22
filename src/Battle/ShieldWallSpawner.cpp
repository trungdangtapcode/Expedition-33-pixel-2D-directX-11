// ============================================================
// File: ShieldWallSpawner.cpp
// Responsibility: Implement a data-driven safe-gap wall pattern.
//
// Common mistakes:
//   1. Treating spawnRate as bullets per second here -> impossible walls.
//      For this spawner it means wall waves per second.
//   2. Hardcoding the gap lane for one enemy -> every enemy using this
//      spawner would inherit that behavior.  Use gapMode in JSON instead.
//   3. Spawning bullets outside the arena without matching cleanup bounds
//      -> bullets can vanish before the player reads the wave.
// ============================================================
#define NOMINMAX

#include "ShieldWallSpawner.h"

#include <cmath>

namespace
{
    constexpr float kPi = 3.1415926535f;

    int ClampInt(int value, int low, int high)
    {
        if (value < low) return low;
        if (value > high) return high;
        return value;
    }
}

ShieldWallSpawner::ShieldWallSpawner(const JsonLoader::BulletSpawnerData& data, int textureIdx)
    : mData(data)
    , mTextureIndex(textureIdx)
    , mSpawnTimer(0.0f)
    , mWaveIndex(0)
{
}

// ------------------------------------------------------------
// Function: SelectGapStart
// Purpose:
//   Choose the first safe lane for the next wall wave.
// Why:
//   The player needs a readable lane opening, while designers still
//   choose whether it tracks the heart or cycles predictably.
// Parameters:
//   boxCy, boxH     - Arena vertical bounds.
//   heartY          - Current player hitbox Y position.
//   laneCount       - Number of lanes in the wall.
//   gapLaneCount    - Number of adjacent safe lanes.
// Caveats:
//   - Returns a clamped start index so the safe gap never exits the arena.
// ------------------------------------------------------------
int ShieldWallSpawner::SelectGapStart(float boxCy, float boxH, float heartY, int laneCount, int gapLaneCount) const
{
    const int maxGapStart = laneCount - gapLaneCount;
    if (maxGapStart <= 0) return 0;

    if (mData.gapMode == "cycle") {
        const int step = mData.gapStep <= 0 ? 1 : mData.gapStep;
        return (mWaveIndex * step) % (maxGapStart + 1);
    }

    const float top = boxCy - boxH * 0.5f;
    const float normalizedY = (heartY - top) / boxH;
    int targetLane = static_cast<int>(std::floor(normalizedY * static_cast<float>(laneCount)));
    targetLane = ClampInt(targetLane, 0, laneCount - 1);

    return ClampInt(targetLane - gapLaneCount / 2, 0, maxGapStart);
}

// ------------------------------------------------------------
// Function: IsLaneInsideGap
// Purpose:
//   Test whether a wall lane should be skipped for the safe gap.
// Why:
//   Keeping this in one helper avoids off-by-one errors when gap size
//   changes per enemy pattern.
// Parameters:
//   lane          - Lane index being considered.
//   gapStart      - First safe lane index.
//   gapLaneCount  - Number of safe lanes.
// ------------------------------------------------------------
bool ShieldWallSpawner::IsLaneInsideGap(int lane, int gapStart, int gapLaneCount) const
{
    return lane >= gapStart && lane < gapStart + gapLaneCount;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance the wall timer and spawn full waves when their interval elapses.
// Why:
//   BulletHellAction owns the generic simulation loop, while this component
//   owns the shape of a shield-wall pattern.
// Parameters:
//   dt             - Frame delta time in seconds.
//   boxCx, boxCy   - Arena center.
//   boxW, boxH     - Arena size.
//   heartX, heartY - Player hitbox position; Y selects tracked gaps.
//   outBullets     - Shared bullet list owned by BulletHellAction.
// Caveats:
//   - For this spawner, spawnRate means wall waves per second.
// ------------------------------------------------------------
void ShieldWallSpawner::Update(float dt,
                               float boxCx,
                               float boxCy,
                               float boxW,
                               float boxH,
                               float heartX,
                               float heartY,
                               std::vector<PhysicsBullet>& outBullets)
{
    (void)heartX;

    if (mData.spawnRate <= 0.0f) return;

    mSpawnTimer += dt;
    const float waveInterval = 1.0f / mData.spawnRate;

    while (mSpawnTimer >= waveInterval) {
        mSpawnTimer -= waveInterval;

        int laneCount = mData.laneCount <= 1 ? 2 : mData.laneCount;
        int gapLaneCount = mData.gapLaneCount <= 0 ? 1 : mData.gapLaneCount;
        if (gapLaneCount >= laneCount) gapLaneCount = laneCount - 1;
        if (gapLaneCount <= 0) gapLaneCount = 1;

        const int gapStart = SelectGapStart(boxCy, boxH, heartY, laneCount, gapLaneCount);
        const bool fromLeft = mData.wallDirection == "right_to_left"
            ? false
            : (mData.wallDirection == "left_to_right" || (mWaveIndex % 2) == 0);

        const float left = boxCx - boxW * 0.5f;
        const float right = boxCx + boxW * 0.5f;
        const float top = boxCy - boxH * 0.5f;
        const float padding = mData.lanePadding < 0.0f ? 0.0f : mData.lanePadding;
        const float usableHeight = boxH - padding * 2.0f;
        const float laneHeight = usableHeight > 0.0f
            ? usableHeight / static_cast<float>(laneCount)
            : boxH / static_cast<float>(laneCount);

        for (int lane = 0; lane < laneCount; ++lane) {
            if (IsLaneInsideGap(lane, gapStart, gapLaneCount)) continue;

            PhysicsBullet bullet;
            bullet.x = fromLeft ? left : right;
            bullet.y = top + padding + (static_cast<float>(lane) + 0.5f) * laneHeight;
            bullet.vx = fromLeft ? mData.bulletSpeed : -mData.bulletSpeed;
            bullet.vy = 0.0f;
            bullet.radius = mData.bulletRadius;
            bullet.angle = fromLeft ? 0.0f : kPi;
            bullet.textureIndex = mTextureIndex;
            bullet.damageScaling = static_cast<int>(mData.bulletDamageScaling * 100.0f);
            outBullets.push_back(bullet);
        }

        ++mWaveIndex;
    }
}
