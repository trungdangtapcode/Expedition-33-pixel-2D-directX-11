# Context-Driven Turn-Based Battle System

Goal: make every part of battle (skills, damage, AI, UI, events) react to **live state**
(HP, MP, rage, status effects, turn count, environment, ally/enemy composition)
instead of using fixed values. Build on the existing architecture in [src/Battle/](../src/Battle/)
without rewriting the FSM or ownership model.

---

## What already exists (do NOT rewrite)

- [IBattler](../src/Battle/IBattler.h) — battle participant interface
- [BattlerStats](../src/Battle/BattlerStats.h) — HP/MP/ATK/DEF/MATK/MDEF/SPD/Rage
- [IStatusEffect](../src/Battle/IStatusEffect.h) — Apply / OnTurnEnd / Revert lifecycle
- [ISkill](../src/Battle/ISkill.h) — `CanUse(caster)` + `Execute(caster, targets)` returning IAction list
- [IDamageCalculator](../src/Battle/IDamageCalculator.h) — DamageRequest / DamageResult
- [IAction](../src/Battle/IAction.h) + [IActionDecorator](../src/Battle/IActionDecorator.h) — frame-stepped queue
- [BattleManager](../src/Battle/BattleManager.h) — FSM (INIT / PLAYER_TURN / RESOLVING / ENEMY_TURN / WIN / LOSE)
- AV-based timeline in `BattleManager` (tick agility system)
- [EventManager](../src/Events/EventManager.h) — pub/sub bus

The architecture is solid. The pieces below are **additive**.

---

## 1. `BattleContext` — single source of truth

Today, `ISkill`, `IDamageCalculator`, and AI each reach into `IBattler`/`BattlerStats`
directly. That doesn't scale once we want predicates like
"HP below 30%", "has Burn", "ally just died", "turn count > 5", "in low-light environment".

Define a read-only struct passed to every system that makes a combat decision:

```cpp
// src/Battle/BattleContext.h  (new — header-only)
struct BattleContext {
    const std::vector<IBattler*>& alivePlayers;
    const std::vector<IBattler*>& aliveEnemies;
    const std::vector<BattleManager::TurnNode>& timeline;
    int   turnCount;        // increments inside BattleManager::AdvanceTurn
    float battleElapsed;    // seconds since BattleState::OnEnter
    EnvironmentTag environment; // Cave / Open / RainSlick / ... — data-driven
};
```

`BattleManager` builds it once per frame and passes `const BattleContext&`
into `ISkill::CanUse`, `ISkill::Execute`, `IDamageCalculator::Calculate`,
`EnemyBrain::Choose`, and `ITrigger::ShouldFire`.

**Nothing else changes** — parameter lists widen, ownership stays put.

---

## 2. `StatModifier` stack — replace one-shot stat mutations

Today [WeakenEffect](../src/Battle/WeakenEffect.h) does
`stats.atk -= N` in `Apply` and `stats.atk += N` in `Revert`. Problems:

- Two overlapping debuffs cancel via arithmetic, not semantically.
- Healing/dispel never sees that the buff existed.
- Conditional buffs ("+20% ATK while HP < 50%") are impossible.

Split into two layers:

```cpp
// BattlerStats stores BASE values only — what JSON loaded.
struct BattlerStats { int baseAtk, baseDef, ...; int hp, mp, rage; };

// A modifier is a function of (base, context) → final.
struct StatModifier {
    enum class Op { AddFlat, AddPercent, Multiply } op;
    StatId        target;          // ATK, DEF, SPD, ...
    float         value;
    std::function<bool(const IBattler&, const BattleContext&)> condition; // optional
    int           sourceEffectId;  // for revert
};

// Effective stat resolution:
// StatResolver::Get(battler, ctx, StatId::ATK) walks all modifiers attached to
// the battler, applies the ones whose condition() is true under the current
// context, and folds them in.
```

Result:
- `WeakenEffect` adds a modifier instead of mutating `stats.atk`. Stacking,
  removal, and revert become bookkeeping (push/pop on a list).
- "Berserk: +50% ATK when HP < 30%" is one modifier with a predicate.
- The damage calculator never reads `stats.atk` directly — it asks
  `StatResolver::Get(attacker, ctx, ATK)`.

`IStatusEffect` interface stays; only its implementations change.

---

## 3. Damage calculator as a pipeline

`DefaultDamageCalculator` is the right pattern but it's a single formula.
Compose it as a chain so future hits (crits, elemental weakness, status
multipliers, environment, "extra damage vs Burning targets") slot in
without rewriting.

```cpp
struct IDamageStep {
    virtual void Apply(DamageRequest&, DamageResult&, const BattleContext&) = 0;
};

class DamageCalculator : public IDamageCalculator {
    std::vector<std::unique_ptr<IDamageStep>> mPipeline;
    // Calculate(): runs each step in order; result accumulates.
};

// Concrete steps:
//   BaseFormulaStep        atk * skillMult - def
//   ElementWeaknessStep    if defender has element tag X, * 1.5
//   StatusBonusStep        if defender has Burn, +20%
//   CritRollStep           rolls crit; flips result.isCritical
//   EnvironmentStep        Cave halves Light damage
//   FinalClampStep         max(1, ...)
```

Steps are registered once at startup. **The order is the formula.**
Adding "all skills do +10% damage if caster's HP is full" is one new
step, no edits to existing files.

---

## 4. `ITrigger` — passive reactions to state changes

The hard part of context-dependent JRPG combat is **reactions**:
"when boss drops below 50% HP, push a cutscene", "when ally dies, this
character enters Vengeance stance", "after 5 turns of poison, immune".
These would otherwise become hand-wired `if` statements.

```cpp
struct ITrigger {
    virtual bool ShouldFire(const BattleContext&) const = 0;
    virtual std::vector<std::unique_ptr<IAction>> Build(BattleContext&) = 0;
    virtual bool ConsumeOnFire() const = 0;  // one-shot vs persistent
};
```

`BattleManager` keeps a `vector<unique_ptr<ITrigger>>` populated from
`data/encounters/*.json`. After each `IAction` resolves inside
`HandleResolving`, the manager walks the trigger list once, fires any
matching ones, and prepends their actions to the queue. `ConsumeOnFire`
removes one-shots like "boss intro on first hit".

This is the structured driver between gameplay state and `EventManager`
that the boss-half-health → cutscene flow in [CLAUDE.md](../CLAUDE.md) §6
needs but doesn't yet have.

---

## 5. Utility AI — data-driven enemy brain

`EnemyCombatant::ChooseTarget` is fine for MVP but for real bosses the AI
must also see context. Tiny utility-AI pattern:

```cpp
struct AIRule {
    std::function<float(const IBattler&, const BattleContext&)> score; // >0 = use
    int  skillIndex;
    std::function<IBattler*(const BattleContext&)> pickTarget;
};
// EnemyBrain runs each rule, picks the highest score, executes its skill.
```

Boss JSON declares its brain:

```json
"behavior": [
  { "rule": "heal_when_low",    "skill": 2, "target": "self",      "score": "hp_pct < 0.4 ? 100 : 0" },
  { "rule": "aoe_when_grouped", "skill": 3, "target": "all_enemy", "score": "alive_enemies >= 3 ? 50 : 0" },
  { "rule": "basic",            "skill": 0, "target": "lowest_hp", "score": "1" }
]
```

Parse expressions once at load into `std::function`s. Behaviour is
**edited in JSON**, not C++.

---

## 6. UI as projection — reactive subscriptions

`BattleManager::HandleResolving` already broadcasts `verso_hp_changed`.
Generalize: every meaningful state change (HP, MP, rage, status applied/
removed, turn started, turn order rebuilt) becomes a typed event. UI
widgets `Subscribe` in their constructor and unsubscribe in their
destructor. **No widget reads combatant state in `Update(dt)`.**

Rule: the simulation owns the state, the UI is a projection of it.

---

## File additions (drop into existing folders)

| File | Folder | Purpose |
|---|---|---|
| `BattleContext.h` | [src/Battle/](../src/Battle/) | Read-only struct passed to all systems |
| `StatModifier.h`, `StatResolver.h/.cpp` | [src/Battle/](../src/Battle/) | Modifier-stack stat resolution |
| `IDamageStep.h` + `Steps/*.h/.cpp` | [src/Battle/](../src/Battle/) | Pipeline calculator |
| `ITrigger.h`, `TriggerSet.h/.cpp` | [src/Battle/](../src/Battle/) | Reaction system |
| `AIRule.h`, `EnemyBrain.h/.cpp` | [src/Battle/](../src/Battle/) | Utility-AI replacement for `ChooseTarget` |
| `data/triggers/*.json`, `data/ai/*.json` | [data/](../data/) | Author boss reactions and behavior trees |

Each file is small (<300 lines per [CLAUDE.md](../CLAUDE.md) rule 4),
each maps one-to-one with an interface, and none of them require
touching `BattleManager`'s FSM beyond passing `BattleContext` through.

---

## Implementation order (smallest unblock first)

1. **`BattleContext` + `StatResolver`** — port `WeakenEffect` and
   `DefaultDamageCalculator` to use them. Immediately enables
   "ATK +30% while HP > 80%"-style effects.
2. **Damage pipeline** — convert `DefaultDamageCalculator` into a chain,
   add `CritRollStep` and `StatusBonusStep` as the first two real steps.
3. **`ITrigger` + `TriggerSet`** — wire into `HandleResolving`. First
   real trigger: "boss HP < 50% → push CutsceneState".
4. **Utility AI** — replace `EnemyCombatant::ChooseTarget` with
   `EnemyBrain`, drive from JSON.
5. **Event-driven UI** — audit each UI widget that reads combatant state
   per-frame and convert to subscriptions.

Each step is independently shippable; the system stays playable between
steps.
