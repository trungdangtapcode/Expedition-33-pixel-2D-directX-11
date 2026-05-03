#pragma once
#include <vector>

enum class BulletBehavior {
    Linear,
    Sine
};

struct PhysicsBullet {
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    float angle;
    int textureIndex;
    int damageScaling; // Raw damage scaling proportional, e.g. 15 for 15% 
    
    // Custom trajectory states
    BulletBehavior behavior = BulletBehavior::Linear;
    float timeAlive = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    float amplitude = 0.0f;
    float frequency = 0.0f;
};

class IBulletSpawner {
public:
    virtual ~IBulletSpawner() = default;

    // dt: delta time
    // mBoxW, mBoxH, mBoxCx, mBoxCy: Arena physics box constraints
    // mHeartX, mHeartY: Current Target coordinates
    virtual void Update(float dt, float mBoxCx, float mBoxCy, float mBoxW, float mBoxH, float mHeartX, float mHeartY, std::vector<PhysicsBullet>& outBullets) = 0;
};
