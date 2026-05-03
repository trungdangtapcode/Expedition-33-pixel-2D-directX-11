#include "SpiralSpawner.h"
#include <cmath>

void SpiralSpawner::Update(float dt, float mBoxCx, float mBoxCy, float mBoxW, float mBoxH, float mHeartX, float mHeartY, std::vector<PhysicsBullet>& outBullets)
{
    mSpawnTimer += dt;
    float interval = 1.0f / mData.spawnRate;
    
    // Rotate the spiral emitter
    mCurrentAngle += dt * 2.0f; // 2 radians per sec or configure it in JSON
    if (mCurrentAngle > 3.14159f * 2.0f) mCurrentAngle -= 3.14159f * 2.0f;

    while (mSpawnTimer > interval) {
        mSpawnTimer -= interval;

        PhysicsBullet b;
        b.radius = mData.bulletRadius;
        b.textureIndex = mTextureIndex;
        b.damageScaling = static_cast<int>(mData.bulletDamageScaling * 100);
        
        // Spawn from center of box
        b.x = mBoxCx;
        b.y = mBoxCy;
        
        b.vx = std::cos(mCurrentAngle) * mData.bulletSpeed;
        b.vy = std::sin(mCurrentAngle) * mData.bulletSpeed;
        b.angle = mCurrentAngle;
        
        outBullets.push_back(b);
        
        // if fast spawning, increment slightly inside loop to make arms
        mCurrentAngle += 0.2f; 
    }
}
