
// ============================================================
// File: CombatantStanceState.h
// Responsibility: Implement the State pattern for combatant stance
//   transitions (Idle -> Ready -> FightState -> Unready -> Idle).
//   This keeps animation tracking decoupled from the battle logic.
// ============================================================
#pragma once

class BattleRenderer;

class ICombatantStanceState
{
public:
    virtual ~ICombatantStanceState() = default;

    // Called once when the state is entered.
    virtual void Enter(BattleRenderer* renderer, int slot, bool isPlayer) = 0;

    // Called every frame. Returns a new state if a transition occurs,
    // otherwise returns nullptr.
    // requestedFightStance enables external logic to tell the state machine
    // what the current expected stance is.
    virtual ICombatantStanceState* Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance) = 0;
};

// ------------------------------------------------------------
// Concrete states
// We use Singletons (Meyers) to avoid heap allocation per transition.
// ------------------------------------------------------------

class StanceStateIdle : public ICombatantStanceState
{
public:
    static StanceStateIdle* Get();
    void Enter(BattleRenderer* renderer, int slot, bool isPlayer) override;
    ICombatantStanceState* Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance) override;
};

class StanceStateReady : public ICombatantStanceState
{
public:
    static StanceStateReady* Get();
    void Enter(BattleRenderer* renderer, int slot, bool isPlayer) override;
    ICombatantStanceState* Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance) override;
};

class StanceStateFight : public ICombatantStanceState
{
public:
    static StanceStateFight* Get();
    void Enter(BattleRenderer* renderer, int slot, bool isPlayer) override;
    ICombatantStanceState* Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance) override;
};

class StanceStateUnready : public ICombatantStanceState
{
public:
    static StanceStateUnready* Get();
    void Enter(BattleRenderer* renderer, int slot, bool isPlayer) override;
    ICombatantStanceState* Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance) override;
};

