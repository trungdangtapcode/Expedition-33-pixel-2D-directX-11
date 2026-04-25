#include "BulletHellAction.h"
#include "EnemyCombatant.h"
#include "DamageSteps.h"
#include "DefaultDamageCalculator.h"
#include "../Core/InputManager.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include <cmath>
#include <random>

static constexpr int VK_W_KEY = 0x57;
static constexpr int VK_A_KEY = 0x41;
static constexpr int VK_S_KEY = 0x53;
static constexpr int VK_D_KEY = 0x44;

BulletHellAction::BulletHellAction(IBattler* attacker, IBattler* defender, float durationSec, std::string bulletTexture, float bRadius, float bSpeed, float bSpawnRate, float bInvincTimer, float bDmgScale)
    : mAttacker(attacker), mDefender(defender), mBulletTexturePath(bulletTexture), mBulletRadius(bRadius), mBulletSpeed(bSpeed), mBulletSpawnRate(bSpawnRate), mBulletInvincibilityDuration(bInvincTimer), mBulletDamageScaling(bDmgScale), mDuration(durationSec), mElapsed(0.0f), mHitsTaken(0)
{
    // Configure standard rendering boundaries
    mBoxCx = 640.0f; // Screen width 1280 (Center layout)
    mBoxCy = 480.0f; // Middle-lower section
    mBoxW = 550.0f;
    mBoxH = 250.0f;

    mHeartX = mBoxCx;
    mHeartY = mBoxCy;
    mHeartRadius = 6.0f;
    
    mSpawnTimer = 0.0f;
    mInvincibilityTimer = 0.0f;
    LOG("[BulletHellAction] Dodge phase begun for %.2f seconds", durationSec);
}

void BulletHellAction::SpawnBullet()
{
    PhysicsBullet b;
    b.radius = mBulletRadius;
    
    int side = rand() % 4;
    float speed = mBulletSpeed * (0.8f + (rand() % 40) / 100.0f); // 80% to 120% speed
    
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
    
    // Instead of shooting strictly perpendicularly, aim roughly towards the heart
    float targetX = mHeartX + ((rand() % 100) - 50.0f); // Add jitter to aim
    float targetY = mHeartY + ((rand() % 100) - 50.0f);
    
    float dx = targetX - b.x;
    float dy = targetY - b.y;
    float dist = std::sqrt(dx*dx + dy*dy);
    if (dist == 0) dist = 1.0f;
    
    b.vx = (dx / dist) * speed;
    b.vy = (dy / dist) * speed;
    b.angle = std::atan2(b.vy, b.vx);
    
    mBullets.push_back(b);
}

void BulletHellAction::ApplyDamage(const BattleContext& ctx)
{
    // Damage scaling fractional!
    DefaultDamageCalculator calc;
    DamageRequest req;
    req.attacker = mAttacker;
    req.defender = mDefender;
    req.type = DamageType::Physical;
    req.skillMultiplier = 1.0f;

    DamageResult res = calc.Calculate(req, ctx);

    // Apply reduced proportional UI damage since this is a barrage
    res.effectiveDamage = (int)(res.effectiveDamage * mBulletDamageScaling);
    if (res.effectiveDamage < 1) res.effectiveDamage = 1;

    mDefender->TakeDamage(res, mAttacker);
    
    EventData ed;
    DamageTakenPayload payload{ mDefender, res.effectiveDamage, false, false };
    ed.payload = &payload;
    EventManager::Get().Broadcast("damage_taken", ed);

    // Sync Attacker animation natively pushing visual feedback exactly alongside the UI
    PlayAnimPayload animPayload{ mAttacker, CombatantAnim::Attack };
    EventData edAnim;
    edAnim.payload = &animPayload;
    EventManager::Get().Broadcast("battler_play_anim", edAnim);
}

bool BulletHellAction::Execute(float dt)
{
    mElapsed += dt;
    // Input polling statically linked bypassing the Turn system input freeze!
    float speedSq = 250.0f;
    InputManager& input = InputManager::Get();
    float dx = 0; float dy = 0;
    
    if (input.IsKeyDown(VK_LEFT) || input.IsKeyDown(VK_A_KEY))  dx -= 1.0f;
    if (input.IsKeyDown(VK_RIGHT)|| input.IsKeyDown(VK_D_KEY)) dx += 1.0f;
    if (input.IsKeyDown(VK_UP)   || input.IsKeyDown(VK_W_KEY))  dy -= 1.0f;
    if (input.IsKeyDown(VK_DOWN) || input.IsKeyDown(VK_S_KEY)) dy += 1.0f;

    if (dx != 0 && dy != 0) {
        float norm = std::sqrt(dx*dx + dy*dy);
        dx /= norm; dy /= norm;
    }

    mHeartX += dx * speedSq * dt;
    mHeartY += dy * speedSq * dt;

    // Extent bounding logic (clamping inside UI rect)
    float minX = mBoxCx - mBoxW / 2.0f + mHeartRadius;
    float maxX = mBoxCx + mBoxW / 2.0f - mHeartRadius;
    float minY = mBoxCy - mBoxH / 2.0f + mHeartRadius;
    float maxY = mBoxCy + mBoxH / 2.0f - mHeartRadius;
    if (mHeartX < minX) mHeartX = minX;
    if (mHeartX > maxX) mHeartX = maxX;
    if (mHeartY < minY) mHeartY = minY;
    if (mHeartY > maxY) mHeartY = maxY;

    // Bullet physics & Spawn
    mSpawnTimer += dt;
    float interval = 1.0f / mBulletSpawnRate;
    if (mSpawnTimer > interval) {
        SpawnBullet();
        mSpawnTimer = 0.0f;
    }

    if (mInvincibilityTimer > 0.0f) {
        mInvincibilityTimer -= dt;
    }

    // Assume standard context from generic static singleton scope conceptually
    // Here we need mock BattleContext since we don't have it natively inside IAction, wait DefaultDamageCal doesn't strictly need it unless stat bonus relies on it. 
    BattleContext dummyCtx; 

    // Update bullets intersecting loops
    for (auto it = mBullets.begin(); it != mBullets.end(); ) {
        it->x += it->vx * dt;
        it->y += it->vy * dt;

        float distSq = (it->x - mHeartX)*(it->x - mHeartX) + (it->y - mHeartY)*(it->y - mHeartY);
        float rSum = it->radius + mHeartRadius;

        if (distSq <= rSum * rSum) {
            // Hit detected
            if (mInvincibilityTimer <= 0.0f) {
                mInvincibilityTimer = mBulletInvincibilityDuration; 
                mHitsTaken++;
                ApplyDamage(dummyCtx);
            }
            it = mBullets.erase(it);
        } else {
            // Bounds clearing natively off-screen logic
            if (it->x < mBoxCx - mBoxW || it->x > mBoxCx + mBoxW || it->y < mBoxCy - mBoxH || it->y > mBoxCy + mBoxH) {
                it = mBullets.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Publish state payload explicitly!
    BulletHellPayload outPayload;
    outPayload.isActive = true;
    outPayload.boxCenterX = mBoxCx;
    outPayload.boxCenterY = mBoxCy;
    outPayload.boxWidth = mBoxW;
    outPayload.boxHeight = mBoxH;
    outPayload.heartX = mHeartX;
    outPayload.heartY = mHeartY;
    outPayload.heartRadius = mHeartRadius;
    for (const auto& b : mBullets) {
        outPayload.bullets.push_back({b.x, b.y, b.radius, b.angle});
    }
    outPayload.bulletTexturePath = mBulletTexturePath;
    outPayload.invincibilityTimer = mInvincibilityTimer;

    EventData ed;
    ed.payload = &outPayload;
    EventManager::Get().Broadcast("verso_bullet_hell_state", ed);

    // End Condition structurally mapped
    if (mElapsed >= mDuration) {
        LOG("[BulletHellAction] Dodge phase ended cleanly.");
        outPayload.isActive = false; // Final clear payload
        EventManager::Get().Broadcast("verso_bullet_hell_state", ed);
        return true; 
    }
    return false;
}
