#include "BulletHellAction.h"
#include "EnemyCombatant.h"
#include "DamageSteps.h"
#include "DefaultDamageCalculator.h"
#include "../Core/InputManager.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"
#include "RandomEdgeSpawner.h"
#include "SpiralSpawner.h"
#include "SineSpawner.h"
#include "ShieldWallSpawner.h"
#include <cmath>
#include <random>

static constexpr int VK_W_KEY = 0x57;
static constexpr int VK_A_KEY = 0x41;
static constexpr int VK_S_KEY = 0x53;
static constexpr int VK_D_KEY = 0x44;

BulletHellAction::BulletHellAction(IBattler* attacker, IBattler* defender, const std::string& patternPath, const BattleContext* context)
    : mAttacker(attacker), mDefender(defender), mContext(context), mElapsed(0.0f), mHitsTaken(0)
{
    JsonLoader::BulletHellPatternData patternData;
    if (!JsonLoader::LoadBulletHellPatternData(patternPath, patternData)) {
        LOG("[BulletHellAction] Warning: Failed to load pattern '%s'. Using defaults.", patternPath.c_str());
    }

    mDuration = patternData.durationSec;
    mBoxCx = patternData.boxCenterX;
    mBoxCy = patternData.boxCenterY;
    mBoxW = patternData.boxWidth;
    mBoxH = patternData.boxHeight;
    mHeartRadius = patternData.heartRadius;
    mHeartSpeed = patternData.heartSpeed;
    mInvincibilityDuration = patternData.invincibilityDuration;

    // Start the player hitbox in the configured arena center so each
    // pattern can choose its own readable dodge space without C++ edits.
    mHeartX = mBoxCx;
    mHeartY = mBoxCy;
    mInvincibilityTimer = 0.0f;

    LOG("[BulletHellAction] Dodge phase begun for %.2f seconds", mDuration);

    // Initialize Texture mapping and Spawners
    for (const auto& spawnerConfig : patternData.spawners) {
        int texIndex = -1;
        // Check if we already registered this texture
        for (int i = 0; i < (int)mTexturePaths.size(); ++i) {
            if (mTexturePaths[i] == spawnerConfig.texturePath) {
                texIndex = i;
                break;
            }
        }
        // Not found, register it
        if (texIndex == -1) {
            mTexturePaths.push_back(spawnerConfig.texturePath);
            texIndex = (int)mTexturePaths.size() - 1;
        }

        // Instantiate specific spawner
        if (spawnerConfig.type == "shield_wall") {
            mSpawners.push_back(std::make_unique<ShieldWallSpawner>(spawnerConfig, texIndex));
        } else if (spawnerConfig.type == "spiral") {
            mSpawners.push_back(std::make_unique<SpiralSpawner>(spawnerConfig, texIndex));
        } else if (spawnerConfig.type == "sine") {
            mSpawners.push_back(std::make_unique<SineSpawner>(spawnerConfig, texIndex));
        } else {
            // Default to random edge
            mSpawners.push_back(std::make_unique<RandomEdgeSpawner>(spawnerConfig, texIndex));
        }
    }
}

void BulletHellAction::ApplyDamage(float overrideScaling)
{
    DefaultDamageCalculator calc;
    DamageRequest req;
    req.attacker = mAttacker;
    req.defender = mDefender;
    req.type = DamageType::Physical;
    req.skillMultiplier = 1.0f;

    const BattleContext fallbackContext;
    const BattleContext& damageContext = mContext ? *mContext : fallbackContext;
    DamageResult res = calc.Calculate(req, damageContext);

    // Bullet pattern data stores each projectile as a fraction of the
    // attack's normal damage, so dense barrages can hurt without acting
    // like many full-strength melee strikes.
    res.effectiveDamage = (int)(res.effectiveDamage * overrideScaling);
    if (res.effectiveDamage < 1) res.effectiveDamage = 1;

    mDefender->TakeDamage(res, mAttacker);
    
    EventData ed;
    DamageTakenPayload payload{ mDefender, res.effectiveDamage, false, false };
    ed.payload = &payload;
    EventManager::Get().Broadcast("damage_taken", ed);

    // Keep the enemy sprite reacting when the bullet actually connects,
    // which ties the dodge minigame feedback back to the battle scene.
    PlayAnimPayload animPayload{ mAttacker, CombatantAnim::Attack };
    EventData edAnim;
    edAnim.payload = &animPayload;
    EventManager::Get().Broadcast("battler_play_anim", edAnim);
}

bool BulletHellAction::Execute(float dt)
{
    mElapsed += dt;
    // The dodge phase owns movement input while the queued action is
    // active.  The configured speed keeps pattern difficulty data-driven.
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

    mHeartX += dx * mHeartSpeed * dt;
    mHeartY += dy * mHeartSpeed * dt;

    // Extent bounding logic (clamping inside UI rect)
    float minX = mBoxCx - mBoxW / 2.0f + mHeartRadius;
    float maxX = mBoxCx + mBoxW / 2.0f - mHeartRadius;
    float minY = mBoxCy - mBoxH / 2.0f + mHeartRadius;
    float maxY = mBoxCy + mBoxH / 2.0f - mHeartRadius;
    if (mHeartX < minX) mHeartX = minX;
    if (mHeartX > maxX) mHeartX = maxX;
    if (mHeartY < minY) mHeartY = minY;
    if (mHeartY > maxY) mHeartY = maxY;

    // Bullet Spawning
    for (auto& spawner : mSpawners) {
        spawner->Update(dt, mBoxCx, mBoxCy, mBoxW, mBoxH, mHeartX, mHeartY, mBullets);
    }

    if (mInvincibilityTimer > 0.0f) {
        mInvincibilityTimer -= dt;
    }

    // Update bullets intersecting loops
    for (auto it = mBullets.begin(); it != mBullets.end(); ) {
        it->timeAlive += dt;
        if (it->behavior == BulletBehavior::Sine) {
            float perpX = -it->vy;
            float perpY = it->vx;
            float len = std::sqrt(perpX*perpX + perpY*perpY);
            if (len > 0) { perpX /= len; perpY /= len; }
            
            float baseDist = std::sqrt(it->vx*it->vx + it->vy*it->vy) * it->timeAlive;
            float baseX = it->startX + (it->vx / len) * baseDist;
            float baseY = it->startY + (it->vy / len) * baseDist;
            
            float offset = std::sin(it->timeAlive * it->frequency) * it->amplitude;
            it->x = baseX + perpX * offset;
            it->y = baseY + perpY * offset;
        } else {
            it->x += it->vx * dt;
            it->y += it->vy * dt;
        }

        float distSq = (it->x - mHeartX)*(it->x - mHeartX) + (it->y - mHeartY)*(it->y - mHeartY);
        float rSum = it->radius + mHeartRadius;

        if (distSq <= rSum * rSum) {
            // Hit detected
            if (mInvincibilityTimer <= 0.0f) {
                mInvincibilityTimer = mInvincibilityDuration; 
                mHitsTaken++;
                ApplyDamage(it->damageScaling / 100.0f);
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
        outPayload.bullets.push_back({b.x, b.y, b.radius, b.angle, b.textureIndex});
    }
    outPayload.texturePaths = mTexturePaths;
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
