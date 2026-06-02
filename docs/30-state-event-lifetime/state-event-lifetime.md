# State and Event Lifetime Hardening

## Purpose

This pass fixes a crash-prone ownership pattern in overlay states. The concrete
symptom was a crash after finishing memory dialogue, but the underlying issue was
broader: states were allowed to mutate the state stack while their own
`Update()` call was still executing.

## Root Cause

`DialogueState` ended by calling `StateManager::PopState()` from inside
`DialogueState::Update()`. `PopState()` immediately called `OnExit()` and erased
the owning `unique_ptr`, which destroyed the `DialogueState` object while one of
its member functions was still on the call stack.

That is undefined behavior. It can appear stable for many frames, then crash
when the next dialogue completion, broadcast, or render pass touches memory that
has already been freed.

## Correct Pattern

`StateManager` now treats stack changes requested during `Update()` as deferred
operations:

1. The active state calls `PushState`, `PopState`, or `ChangeState`.
2. `StateManager` records the operation if it is currently updating a state.
3. The active state's `Update()` returns normally.
4. `StateManager` applies the queued stack operations.

This preserves the old public API while making self-pop, pause resume, battle
exit, and title transitions safe.

## Event Broadcast Safety

`EventManager::Broadcast()` already copied listener entries so callbacks could
subscribe or unsubscribe without invalidating iterators. The missing guard was
listener liveness: a copied callback could still be invoked after another
callback unsubscribed it earlier in the same broadcast.

Broadcast now checks that each copied listener id is still subscribed before
calling it. This prevents stale state-owned callbacks from firing after a state
transition has already torn them down.

## Rules

- State classes may request stack changes during `Update()`.
- State classes must not assume the stack changed until their `Update()` returns.
- Every event subscription made by a state must still be unsubscribed in
  `OnExit()`.
- Broadcast payload pointers are observer-only; callbacks must not store them.
- SceneGraph-owned entities tracked by raw observer lists must be removed from
  those lists before the entity can be purged.

## Objective Beacon Tuning

The objective beacon is navigation help, not a target badge. Its visual scale and
hide distance remain data-driven:

- `data/objective_beacon.json` controls enabled state, asset path, and close
  range hiding.
- `assets/UI/objective-beacon-ui.json` controls size, bobbing, and vertical
  offset.

The current tuning hides the marker earlier and renders it smaller so nearby
enemies and NPCs stay readable.

## Verification

- Finish `memory_paris_first_echo` and confirm the dialogue closes without a
  crash.
- Open and close pause, inventory, lineup, and campfire overlays to confirm
  self-pop transitions remain stable.
- Finish a battle and confirm battle result exit still returns to overworld.
- Approach an objective enemy and confirm the beacon disappears before covering
  the enemy sprite.
