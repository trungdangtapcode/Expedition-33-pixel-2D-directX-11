#pragma once
#include "IAction.h"
#include "IBattler.h"
#include "BattleContext.h"
#include "BattleEvents.h"
#include <vector>

class BulletHellAction : public IAction
{
public:
    BulletHellAction(IBattler* attacker, IBattler* defender, float durationSec = 5.0f, std::string bulletTexture = "", float bRadius = 6.0f, float bSpeed = 150.0f, float bSpawnRate = 4.0f, float bInvincTimer = 1.0f, float bDmgScale = 0.15f);

    bool Execute(float dt) override;

private:
    IBattler* mAttacker;
    IBattler* mDefender;
    std::string mBulletTexturePath;
    float mBulletRadius;
    float mBulletSpeed;
    float mBulletSpawnRate;
    float mBulletInvincibilityDuration;
    float mBulletDamageScaling;

    // Time scaling 
    float mDuration;
    float mElapsed;

    // Simulation mapping
    float mBoxCx, mBoxCy, mBoxW, mBoxH;
    float mHeartX, mHeartY, mHeartRadius;

    // Bullet Spawning
    struct PhysicsBullet {
        float x, y, vx, vy, radius, angle;
    };
    std::vector<PhysicsBullet> mBullets;
    float mSpawnTimer;

    // Damage metrics
    float mInvincibilityTimer;
    int mHitsTaken;

    void SpawnBullet();
    void ApplyDamage(const BattleContext& ctx);
};
