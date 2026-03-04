# Bonus — How to Play the Battle Demo

This is a quick control reference for the current battle demo build.

---

## Starting a Battle

From the main menu or `PlayState`, press **B** to trigger
`EventManager::Broadcast("start_battle")` → `StateManager::ChangeState(BattleState)`.

---

## In-Battle Controls

| Key | Action |
|---|---|
| `1` / `2` / `3` | Select skill slot 1 / 2 / 3 |
| `Tab` | Cycle target (next enemy) |
| `Enter` | Confirm action (enqueue `AttackAction`) |
| `Esc` | Back / cancel selection |

---

## Turn Flow (current implementation)

```
1. Player chooses a skill + target → Enter
2. AttackAction is enqueued in ActionQueue
3. ActionQueue::Update(dt) calls AttackAction::Execute(dt)
4. Damage resolves → BattleManager broadcasts "verso_hp_changed" (or enemy hp)
5. HealthBarRenderer receives the event → mTargetHP updated
6. HP bar drains smoothly via lerp over ~0.6 seconds
7. Next combatant's turn begins (enemy AI auto-attacks)
```

---

## What Is Visible

- **Verso** — left center, facing right (player slot 0)
- **Skeleton A / B** — right side, facing left (`flipX=true`, enemy slots 0 and 1)
- **HP Bar** — top-left corner: background / red fill / decorative frame

The battle ends when all enemies reach 0 HP or Verso's HP reaches 0.
Currently `BattleState` transitions back to `PlayState` on battle-end via
`EventManager::Broadcast("battle_ended")`.
