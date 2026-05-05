#pragma once
#include "IBulletSpawner.h"
#include "../Utils/JsonLoader.h"
#include <cstdlib>

class RandomEdgeSpawner : public IBulletSpawner {
public:
    RandomEdgeSpawner(const JsonLoader::BulletSpawnerData& data, int textureIdx)
        : mData(data), mTextureIndex(textureIdx), mSpawnTimer(0.0f) {}

    void Update(float dt, float mBoxCx, float mBoxCy, float mBoxW, float mBoxH, float mHeartX, float mHeartY, std::vector<PhysicsBullet>& outBullets) override;

private:
    JsonLoader::BulletSpawnerData mData;
    int mTextureIndex;
    float mSpawnTimer;
};
