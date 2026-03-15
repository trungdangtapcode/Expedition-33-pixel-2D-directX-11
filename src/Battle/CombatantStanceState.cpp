
// ============================================================
// File: CombatantStanceState.cpp
// ============================================================
#include "CombatantStanceState.h"
#include "BattleRenderer.h"
#include "CombatantAnim.h"

// ============================================================
// StanceStateIdle
// ============================================================
StanceStateIdle* StanceStateIdle::Get()
{
    static StanceStateIdle instance;
    return &instance;
}

void StanceStateIdle::Enter(BattleRenderer* renderer, int slot, bool isPlayer)
{
    if (isPlayer) renderer->PlayPlayerClip(slot, CombatantAnim::Idle);
    else          renderer->PlayEnemyClip(slot, CombatantAnim::Idle);
}

ICombatantStanceState* StanceStateIdle::Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance)
{
    if (requestedFightStance)
    {
        return StanceStateReady::Get();
    }
    return nullptr;
}

// ============================================================
// StanceStateReady
// ============================================================
StanceStateReady* StanceStateReady::Get()
{
    static StanceStateReady instance;
    return &instance;
}

void StanceStateReady::Enter(BattleRenderer* renderer, int slot, bool isPlayer)
{
    if (isPlayer) renderer->PlayPlayerClip(slot, CombatantAnim::Ready);
    else          renderer->PlayEnemyClip(slot, CombatantAnim::Ready);
}

ICombatantStanceState* StanceStateReady::Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance)
{
    bool isDone = isPlayer ? renderer->IsPlayerClipDone(slot) : renderer->IsEnemyClipDone(slot);
    if (isDone)
    {
        return StanceStateFight::Get();
    }
    return nullptr;
}

// ============================================================
// StanceStateFight
// ============================================================
StanceStateFight* StanceStateFight::Get()
{
    static StanceStateFight instance;
    return &instance;
}

void StanceStateFight::Enter(BattleRenderer* renderer, int slot, bool isPlayer)
{
    if (isPlayer) renderer->PlayPlayerClip(slot, CombatantAnim::FightState);
    else          renderer->PlayEnemyClip(slot, CombatantAnim::FightState);
}

ICombatantStanceState* StanceStateFight::Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance)
{
    if (!requestedFightStance)
    {
        return StanceStateUnready::Get();
    }
    return nullptr;
}

// ============================================================
// StanceStateUnready
// ============================================================
StanceStateUnready* StanceStateUnready::Get()
{
    static StanceStateUnready instance;
    return &instance;
}

void StanceStateUnready::Enter(BattleRenderer* renderer, int slot, bool isPlayer)
{
    if (isPlayer) renderer->PlayPlayerClip(slot, CombatantAnim::Unready);
    else          renderer->PlayEnemyClip(slot, CombatantAnim::Unready);
}

ICombatantStanceState* StanceStateUnready::Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance)
{
    bool isDone = isPlayer ? renderer->IsPlayerClipDone(slot) : renderer->IsEnemyClipDone(slot);
    if (isDone)
    {
        return StanceStateIdle::Get();
    }
    return nullptr;
}

