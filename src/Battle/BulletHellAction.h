#pragma once
#include "IAction.h"
#include "IBattler.h"
#include "BattleContext.h"
#include "BattleEvents.h"
#include "IBulletSpawner.h"
#include <vector>
#include <string>
#include <memory>

class BulletHellAction : public IAction
{
public:
    BulletHellAction(IBattler* attacker, IBattler* defender, const std::string& patternPath, const BattleContext* context);

    bool Execute(float dt) override;

private:
    IBattler* mAttacker;
    IBattler* mDefender;
    const BattleContext* mContext;

    // Phase management
    float mDuration;
    float mElapsed;

    // Simulation mapping
    float mBoxCx, mBoxCy, mBoxW, mBoxH;
    float mHeartX, mHeartY, mHeartRadius;
    float mHeartSpeed;

    std::vector<PhysicsBullet> mBullets;
    std::vector<std::unique_ptr<IBulletSpawner>> mSpawners;
    std::vector<std::string> mTexturePaths;

    // Damage metrics
    float mInvincibilityTimer;
    float mInvincibilityDuration;
    int mHitsTaken;

    void ApplyDamage(float overrideScaling);
};
