# Multi-QTE Combat Architecture (Osu! System)

The combat engine features a highly dynamic, concurrent rhythm-based Quick Time Event (QTE) system for attacks. Rather than forcing the player to respond to a series of strictly separated, single sequential inputs, the system spawns **overlapping, continuous QTE nodes** akin to rhythm games like *Osu!*

## 1. Data-Driven Configuration (JSON)

Every aspect of the rhythmic difficulty, timing, and visual feedback is parameterized explicitly in the skill's `.json` configuration file, which maps directly into the engine's `SkillData` via `JsonLoader`.

### Core Parameters
- **`qteMinCount` / `qteMaxCount`**: Bounds for the number of QTE diamonds spawned per attack. An encounter randomly rolls within this bound.
- **`qteSpacing`**: The internal lifetime (duration) of each QTE node mapped against the attack animation progress.
- **`qteFadeInRatio`**: The percentage of the QTE's lifetime assigned to the visual fade-in threshold.
- **`qteFadeOutDuration`**: The physical post-resolution explosion flash decay time in seconds.

### Mechanical Translators
- **`qteSlowMoScale`**: Global time dilation factor triggered instantly when the attack sequence commits. This uniformly scales `TimeSystem::Get().SetSlowMotion` ensuring node velocity translates comfortably exactly the same regardless of whether 1 or 8 nodes spawn.
- **Thresholds**: `qtePerfectThreshold` and `qteGoodThreshold` mathematically bound exactly what percentage of `qteSpacing` must be cleared to score.

---

## 2. The Node "Bucket" Spawning Algorithm

To resolve visual chaos and physically unplayable QTE "clusters", the `QteAnimDamageAction` relies on a constrained **bucket distribution** algorithm rather than purely naive random distribution.

### The Problem with Flat Randomization
If nodes spawn blindly and randomly across an attack window (e.g., `0.3s -> 0.8s`), the RNG engine can easily place Node 0 at `0.400s` and Node 1 at `0.415s`. Functionally, those two physical inputs overlap into an unreactable fraction of a second, causing the engine to aggressively swallow consecutive inputs and instantly force a Miss.

### The Constrained Bucket Solution
When requesting exactly `N` nodes, the engine chronologically sub-divides the available attack timeline into exactly `N` equal segments. 

1. **Isolation:** Node 0 is permanently bound to Segment 0, Node 1 to Segment 1, etc.
2. **Jitter:** Node timestamps are pseudo-randomized by injecting a dynamic jitter constrained dynamically to `70%` of its bucket's width.
3. **Rhythmic Integrity:** This guarantees a fixed chronological minimum "gap" between every overlapping node, naturally distributing human-readable rhythm tempos.

---

## 3. UI Renderer & Visual Feedback Mathematics

The graphical translation is processed natively via `BattleQTERenderer.cpp`, which evaluates the `QTEStatePayload` populated over the event bus. 

### Interpolating "Approach Circles"
Rather than a jarring size-snap, each unhandled QTE frame shrinks deterministically and completely linearly against its underlying progress timeline down from `1.5x` scale to `1.0x` scale. Because the visual outer border collapses smoothly precisely mapping to the progression percentage over `100%` of the timeline, players intuitively read the physics without referencing numbers. 

### Garbage Collection (`mFlashTimers`)
The attack animation cannot logically yield global control (`mState.isActive = false`) until the player's 3D/sprite animation completely returns backwards to its idle state frames. Thus, QTE graphics used to remain visibly idle and frozen for fractions of a second after a hit because they inherited the global state.

This is decoupled dynamically via `mFlashTimers[8]`:
- Each indexed node receives a decoupled timing structure explicitly managed by the designer's `qteFadeOutDuration` scalar.
- When an input executes, precisely that node generates its independent hit flash (Gold/Yellow/Gray).
- Once its local explosion interpolates entirely down to `0.0f` scalar, it forcefully triggers a local rendering `continue;` eviction, completely removing that target from the visual hierarchy while correctly leaving pending future nodes on-screen unmodified.

---

## 4. Multiplier Averaging Math

Because combat spans simultaneous overlapping nodes, dealing strictly additive damage scalars (`x1.5` per node) would massively shatter bounds whenever the `qteMaxCount` roles a 5 or an 8. 

During damage resolution (`DamageSteps::StatusBonusStep`), the engine strictly **aggregates the averages**:
```cpp
totalDamageMultiplier = (Node[0].mult + Node[1].mult + Node[2].mult) / 3.0f;
```
This guarantees scaling stability: an attack's absolute maximum output ceiling relies exclusively on the skill's absolute max parameters regardless of physical quantity variables.
