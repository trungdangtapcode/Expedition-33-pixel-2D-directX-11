// ============================================================
// File: ShieldWallSpawner.h
// Responsibility: Spawn readable bullet-wall waves with a safe lane.
//
// Owns:
//   JsonLoader::BulletSpawnerData copied from the active pattern.
//
// Lifetime:
//   Created in  -> BulletHellAction when a pattern uses type "shield_wall".
//   Destroyed in -> BulletHellAction destructor through unique_ptr<IBulletSpawner>.
//
// Important:
//   - Wave timing is driven by spawnRate as waves per second.
//   - Safe-lane selection is configured by gapMode, not enemy-specific code.
// ============================================================
#pragma once

#include "IBulletSpawner.h"
#include "../Utils/JsonLoader.h"

class ShieldWallSpawner : public IBulletSpawner
{
public:
    ShieldWallSpawner(const JsonLoader::BulletSpawnerData& data, int textureIdx);

    void Update(float dt,
                float boxCx,
                float boxCy,
                float boxW,
                float boxH,
                float heartX,
                float heartY,
                std::vector<PhysicsBullet>& outBullets) override;

private:
    JsonLoader::BulletSpawnerData mData;
    int mTextureIndex;
    float mSpawnTimer;
    int mWaveIndex;

    int SelectGapStart(float boxCy, float boxH, float heartY, int laneCount, int gapLaneCount) const;
    bool IsLaneInsideGap(int lane, int gapStart, int gapLaneCount) const;
};
