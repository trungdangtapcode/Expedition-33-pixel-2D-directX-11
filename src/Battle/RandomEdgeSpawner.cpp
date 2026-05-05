#include "RandomEdgeSpawner.h"
#include <cmath>

void RandomEdgeSpawner::Update(float dt, float mBoxCx, float mBoxCy, float mBoxW, float mBoxH, float mHeartX, float mHeartY, std::vector<PhysicsBullet>& outBullets)
{
    mSpawnTimer += dt;
    float interval = 1.0f / mData.spawnRate;
    
    while (mSpawnTimer > interval) {
        mSpawnTimer -= interval;

        PhysicsBullet b;
        b.radius = mData.bulletRadius;
        b.textureIndex = mTextureIndex;
        b.damageScaling = static_cast<int>(mData.bulletDamageScaling * 100);
        
        int side = rand() % 4;
        float speed = mData.bulletSpeed * (0.8f + (rand() % 40) / 100.0f); // 80% to 120% speed
        
        if (side == 0) { // Top edge
            b.x = mBoxCx - (mBoxW / 2.0f) + (rand() % (int)mBoxW);
            b.y = mBoxCy - (mBoxH / 2.0f);
        } else if (side == 1) { // Bottom edge
            b.x = mBoxCx - (mBoxW / 2.0f) + (rand() % (int)mBoxW);
            b.y = mBoxCy + (mBoxH / 2.0f);
        } else if (side == 2) { // Left
            b.x = mBoxCx - (mBoxW / 2.0f);
            b.y = mBoxCy - (mBoxH / 2.0f) + (rand() % (int)mBoxH);
        } else { // Right
            b.x = mBoxCx + (mBoxW / 2.0f);
            b.y = mBoxCy - (mBoxH / 2.0f) + (rand() % (int)mBoxH);
        }
        
        // Aim roughly towards the heart
        float targetX = mHeartX + ((rand() % 100) - 50.0f); // Jitter aim
        float targetY = mHeartY + ((rand() % 100) - 50.0f);
        
        float dx = targetX - b.x;
        float dy = targetY - b.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist == 0) dist = 1.0f;
        
        b.vx = (dx / dist) * speed;
        b.vy = (dy / dist) * speed;
        b.angle = std::atan2(b.vy, b.vx);
        
        outBullets.push_back(b);
    }
}
