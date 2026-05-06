# Consequences of Hardcoded Design

## The Revive Bug — A Case Study

Adding Phoenix Down to the game required zero new C++ classes. The item
JSON existed, the `Revive` effect kind was implemented, the inventory
seeded 2 copies. On paper the feature was "done." In practice, using the
item produced a character that was:

1. **Alive but turn-locked** — HP restored, but never acted again.
2. **Alive but visually dead** — HP restored, but still showing the
   death animation frame.

Two bugs. Neither was in the revive code itself. Both were consequences
of hardcoded one-way paths elsewhere in the codebase.

---

## Bug 1: Turn Queue — One-Way Door

### The Hardcoded Path

`BattleManager::AdvanceTurn()` had this:

```cpp
// Remove dead combatants from the timeline
mTimeline.erase(std::remove_if(mTimeline.begin(), mTimeline.end(),
    [](const TurnNode& n) { return !n.battler->IsAlive(); }), mTimeline.end());
```

This is a **one-way door**. Combatants go out, nothing brings them back.
The code was written with the implicit assumption that death is permanent.
That assumption was never documented, never enforced by a type, and never
visible to the developer who later wrote the revive effect.

### Why It Wasn't Caught Earlier

The revive effect was implemented in `ItemEffectAction.cpp`. The turn
queue lives in `BattleManager.cpp`. These are separate files, separate
responsibilities. The developer writing the revive effect had no reason
to open `BattleManager.cpp` — the item system is designed to be
self-contained. The architecture actively encouraged them NOT to look.

### The Fix

Add a re-insertion scan in the same function that removes:

```cpp
// Re-insert combatants that are alive but missing from the timeline.
for (auto& p : mPlayers) reinsertIfMissing(p.get());
for (auto& e : mEnemies) reinsertIfMissing(e.get());
```

The fix is 15 lines. The debugging session to find it could be hours.

### The Lesson

**Every removal path must have a corresponding insertion path, or the
removal must be provably final.** If `AdvanceTurn` had been written as
"rebuild from alive combatants" instead of "subtract the dead," the bug
would have been impossible.

---

## Bug 2: Animation — Asymmetric Event Usage

### The Hardcoded Path

`Combatant::TakeDamage()` broadcasts the death animation:

```cpp
if (mStats.hp <= 0)
{
    PlayAnimPayload animPayload{ this, CombatantAnim::Die };
    EventManager::Get().Broadcast("battler_play_anim", edAnim);
}
```

The event interface exists. It works. But `ItemEffectAction::Revive`
did not use it:

```cpp
s.hp = mEffect.amount;
s.ClampHp();
// ...and nothing else. No animation broadcast.
```

The death path used the event bus. The revive path used raw stat mutation.
Same interface was available for both directions, but only one direction
was wired. The developer who wrote the revive effect saw `BattlerStats`
and thought "set HP, done." They had no way to know that a separate
system (the renderer) was listening for animation events that they were
expected to broadcast.

### The Fix

One broadcast after the HP restore:

```cpp
PlayAnimPayload animPayload{ mTarget, CombatantAnim::Idle };
EventManager::Get().Broadcast("battler_play_anim", animEvent);
```

Same event, same payload struct, opposite direction. The renderer
already knows how to play any clip — it just was never told to.

### The Lesson

**If system A broadcasts an event to trigger a state change in system B,
then every code path that reverses that state change must also broadcast
through the same event.** Otherwise the two systems desynchronize silently.

---

## Why Hardcoding Creates Exponential Testing Burden

### The Multiplication Problem

Consider a game with N features. Each feature can interact with every
other feature. The number of pairwise interactions is N*(N-1)/2.

| Features | Pairwise Interactions |
|---|---|
| 5 | 10 |
| 10 | 45 |
| 20 | 190 |
| 50 | 1,225 |

When a feature is hardcoded with implicit assumptions (e.g. "death is
permanent"), every NEW feature that violates those assumptions creates
bugs at interaction points that nobody anticipated. The bugs are not
in the new feature. They are in the old feature's hidden assumptions.

**Phoenix Down is one item.** It exposed two bugs. If the game has 10
features that can reverse death (revive items, revive spells, auto-revive
passives, story resurrections, checkpoint respawns), each one independently
hits the same two bugs — unless the developer happens to know about the
hidden assumptions in `BattleManager` and `Combatant`.

### The Testing Spiral

```
Session 1: Ship 10 features. Test 45 interactions.  Find 3 bugs.
Session 2: Add 1 feature.   Test 55 interactions.  Find 2 new bugs.
Session 3: Add 1 feature.   Test 66 interactions.  Find 4 new bugs.
           (the new feature hit 2 old hardcoded assumptions
            that nobody remembered existed)
```

The tester does not know which 4 of the 66 interactions are broken. They
must test all 66. Every session. The cost grows quadratically while the
team's understanding of hidden assumptions stays constant (or declines
as people forget why code was written a certain way).

### What "Not Hardcoded" Looks Like

A properly symmetric design eliminates entire categories of interaction
bugs:

```
HARDCODED (one-way):
  TakeDamage  →  remove from timeline     (implicit: permanent)
  TakeDamage  →  broadcast Die anim       (implicit: irreversible)
  Revive      →  set HP                   (no timeline, no anim)
  Result: 2 bugs per revive-like feature

SYMMETRIC (interface-driven):
  AdvanceTurn →  remove dead, re-insert alive-but-missing
  TakeDamage  →  broadcast("battler_play_anim", Die)
  Revive      →  broadcast("battler_play_anim", Idle)
  Result: 0 bugs. Any future HP-restoration path is automatically correct.
```

The symmetric version does not need testing for "does revive restore the
turn queue" because the turn queue is always rebuilt from ground truth
(`IsAlive()`). There is no hidden assumption to violate.

---

## Catalog of Hardcoded Patterns and Their Costs

These are patterns that create the same class of bug as the revive issue.
Some have been fixed; others are latent risks.

### 1. One-Way State Transitions

**Pattern:** Code removes/disables something but has no path to reverse it.

**Example (fixed):** `AdvanceTurn` removed dead combatants with no
re-insertion path.

**Cost:** Every feature that reverses the state (revive, respawn, undo)
must independently discover and fix the gap. N reversing features =
N independent bugs.

**Prevention:** State should be derived from ground truth, not
accumulated from deltas. "Who is in the timeline?" should be answered by
"who is alive?" not by "who was added minus who was removed."

### 2. Asymmetric Event Wiring

**Pattern:** System A notifies system B on state change X, but not on
the reverse of X.

**Example (fixed):** `TakeDamage` broadcasts `Die` animation.
`Revive` did not broadcast `Idle` animation.

**Cost:** Systems B, C, D all hold stale state after the reverse. Each
must be found and fixed individually. The bug is silent — no crash, no
error log, just wrong visual/behavioral state.

**Prevention:** Wrap both directions in one function. If `NotifyDeath()`
exists, `NotifyRevive()` must exist in the same file, at the same
abstraction level. Or better: make the event data-driven ("here is the
new state") rather than imperative ("this thing died").

### 3. Implicit Preconditions

**Pattern:** Function assumes a condition is always true but does not
assert or document it.

**Example:** `HandlePlayerTurn` assumes the current combatant is in the
timeline. If a bug removes it prematurely, the function silently skips
the turn with no error.

**Cost:** Debugging time. The symptom (skipped turn) is far from the
cause (premature removal). Hours of printf debugging to trace the gap.

**Prevention:** Assert preconditions explicitly:
```cpp
assert(CurrentCombatant() != nullptr && "AdvanceTurn must set a valid combatant before PLAYER_TURN");
```
A crash at the assertion site is 100x cheaper than a silent wrong
behavior discovered by a tester three features later.

### 4. Stat Mutation Without Notification

**Pattern:** Code modifies a stat directly without going through the
system that other code depends on for updates.

**Example:** Setting `s.hp = amount` in the revive case without
broadcasting `hp_changed`. UI elements that listen for HP changes
(health bars) will show stale values until the next unrelated damage
event.

**Cost:** UI desyncs. The health bar shows 0 HP for a character who
is alive and acting. The tester reports "health bar broken after revive."
The developer checks the health bar code — it is correct. The bug is in
the caller who bypassed the notification path.

**Prevention:** All stat mutations go through a single function that
handles notification as a side effect:
```cpp
void Combatant::SetHp(int newHp) {
    mStats.hp = std::clamp(newHp, 0, mStats.maxHp);
    BroadcastHpChanged();
    if (mStats.hp <= 0) BroadcastDeath();
    if (wasDeadBefore && mStats.hp > 0) BroadcastRevive();
}
```
Now there is ONE place where HP changes, ONE place where events fire,
and ZERO ways to forget a notification.

---

## The Rule

> **Every state change that any system observes must be reversible
> through the same interface that created it. If it is not, every
> future feature that reverses the state will independently rediscover
> and re-fix the gap.**

This is not a style preference. It is a direct multiplier on testing
cost, debugging time, and bug count. A game with 50 features and
symmetric interfaces needs ~1,200 interaction tests. The same game with
10 hardcoded one-way paths needs ~1,200 tests PLUS an unknown number of
hidden-assumption bugs that no test matrix will reliably cover, because
the interactions are implicit and undocumented.

The revive bug cost two fixes totaling 25 lines of code. Finding it
required reading three files across two subsystems. In a larger
codebase with more features, the same class of bug costs days, not
minutes. The fix is always small. The search is always large. That
asymmetry is the entire cost of hardcoded design.
