# Multi-QTE Combat Architecture

The combat engine supports multi-node Quick Time Event (QTE) chains for
attack-mechanism skills. Each skill chooses its own timing flow from JSON:
`staggered` preserves the older overlapping rhythm, while `chain` gives every
prompt a full local input window.

## 1. Data-Driven Configuration (JSON)

Every aspect of the rhythmic difficulty, timing, and visual feedback is parameterized explicitly in the skill's `.json` configuration file, which maps directly into the engine's `SkillData` via `JsonLoader`.

### Core Parameters
- **`qteMinCount` / `qteMaxCount`**: Bounds for the number of QTE diamonds spawned per attack. An encounter randomly rolls within this bound.
- **`qteTimingFlow`**: `staggered` starts nodes by rhythm spacing; `chain` gives each node a full local window before the next one starts. `sequential` is accepted as an alias for `chain`.
- **`qteLeadInSeconds`**: Readable delay after the attack animation reaches `qteStartMoment` before the first node appears.
- **`qteSpacing`**: In `staggered`, time between node starts. In `sequential`, the gap after one node window before the next begins.
- **`qteNodeDuration`**: Full active window duration for one node, measured in UI-clock seconds.
- **`qteFadeInRatio`**: The percentage of the QTE's lifetime assigned to the visual fade-in threshold.
- **`qteFadeOutDuration`**: The physical post-resolution explosion flash decay time in seconds.

### Mechanical Translators
- **`qteSlowMoScale`**: Global time dilation factor triggered instantly when the attack sequence commits. This uniformly scales `TimeSystem::Get().SetSlowMotion` ensuring node velocity translates comfortably exactly the same regardless of whether 1 or 8 nodes spawn.
- **Thresholds**: `qtePerfectThreshold` and `qteGoodThreshold` define what percentage of the active node window must be cleared to score.

---

## 2. Node Scheduling

`QteAnimDamageAction` schedules nodes in real UI-clock seconds after the attack
animation reaches `qteStartMoment`. Animation progress only gates when the QTE
chain starts and when damage can resolve; it does not compress the prompt
windows.

### Staggered Flow

`staggered` preserves the fast rhythm style:

```text
start = qteLeadInSeconds + index * qteSpacing
end   = start + qteNodeDuration
```

This is appropriate for basic attacks and short strings where overlap is part
of the intended rhythm.

### Chain Flow

`chain` is used whenever prompts must stay readable. Each node's timer starts
only after that node becomes active, so later nodes cannot expire while the
player is resolving an earlier prompt:

```text
first active timer = -qteLeadInSeconds
next active timer  = -qteSpacing
window             = qteNodeDuration
```

This is currently used by basic attacks, advanced debuff attacks, fire attacks,
AoE attacks, and rage finishers. Basic attacks can roll up to five nodes, but
only one full-size prompt is playable at a time.

---

## 3. UI Renderer & Visual Feedback Mathematics

The graphical translation is processed natively via `BattleQTERenderer.cpp`, which evaluates the `QTEStatePayload` populated over the event bus. 

### Interpolating The Prompt

Each unresolved QTE frame shrinks against its own `progressRatio`. Because the
outer border maps to the active window, the player can learn timing from motion
instead of reading numbers.

### Chain Presentation

Chain mode uses two separate visual layers:

- One full-size active prompt is drawn at the configured chain anchor.
- Remaining prompts are shown as small preview markers, not playable prompts.
- Resolved prompts do not draw full-size flashes in chain mode because the next
  active prompt must remain readable.

### Staggered Flash Eviction

Staggered mode preserves per-node result flashes for older rhythm patterns:

- Perfect, Good, and Miss results tint only the resolved node.
- `qteFadeOutDuration` controls how long the result flash remains visible.
- Finished flashes are skipped without clearing future unresolved nodes.

---

## 4. Multiplier Averaging Math

Because combat can span several nodes, dealing strictly additive damage scalars
would break skill balance whenever a skill rolls a high node count.

During damage resolution (`DamageSteps::StatusBonusStep`), the engine strictly **aggregates the averages**:
```cpp
totalDamageMultiplier = (Node[0].mult + Node[1].mult + Node[2].mult) / 3.0f;
```
The optional `bonusQteCount` adds a small flat bonus for Perfect and Good nodes
after the average is computed. This keeps complex skills rewarding without
turning high node count into exponential damage.

---

## 5. Parameter Distribution (Global vs. Local)

To enforce DRY (Don't Repeat Yourself) principles and guarantee structural scaling, combat tuning parameters are strictly decoupled based on whether they dictate **Game Cinematic Feel** vs **Combat Mechanical Pacing**.

### Global UI / Engine Parameters (`BattleSystemConfig`)
Stored centrally in `data/battle_system_config.json` and piped synchronously into `BattleContext` each frame, these parameters govern universal engine presentation layers:
- `qteCameraZoom`: The universal scalar for real-time `DYNAMIC_FOLLOW` graphical tracking during attacks.
- `qteFadeInRatio` / `qteFadeOutDuration`: The UI cross-fade explosion and collapse durations.
- `qteSlowMoScale`: The intensity of the global battle simulation slowdown scalar when inputs trigger.
- `qtePromptRadius` / `qteFrameTextureSize`: Size contracts for the QTE frame texture.
- `qteChainAnchorXRatio` / `qteChainAnchorYRatio`: Screen-space anchor for chain-mode active prompts.
- `qteChainPreviewScale` / `qteChainPreviewActiveScale`: Small marker sizes for remaining chain nodes.
- `qteChainPreviewSpacing` / `qteChainPreviewOffsetY`: Preview row layout around the active prompt.

Modifying these instantly changes the presentation feel of the entire engine generically.

### Local Mechanical Parameters (`SkillData`)
Stored individually inside each attack (e.g. `data/skills/verso_attack.json`), these scale exactly how the move functionally resolves math:
- `qteStartMoment`, `qteTimingFlow`, `qteLeadInSeconds`, `qteMinCount`, `qteMaxCount`, `qteSpacing`, `qteNodeDuration`: define the physical timing traits and limits of the specific weapon swing.
- `qtePerfectMultiplier`, `qteGoodMultiplier`, `qteMissMultiplier`: Unique damage scaling scalars.
- `qtePerfectThreshold`, `qteGoodThreshold`: Strict mechanical difficulty requirements.
- `bonusQteCount`: Flat bonus addition modifiers depending on successful chain executions.

This separation lets one skill become a slow chain finisher while another stays
a quick staggered rhythm attack without changing engine code.
