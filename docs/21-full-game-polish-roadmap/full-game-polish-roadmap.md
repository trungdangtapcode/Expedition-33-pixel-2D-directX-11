# Full Game Polish Roadmap

## Purpose

This document defines the next production direction for turning the current
prototype into a complete, replayable 2D reactive turn-based JRPG. The goal is
not to copy Clair Obscur: Expedition 33, but to learn from its strongest design
patterns and translate them into this project's existing C++/DirectX systems.

The current game already has a strong base:

- Overworld exploration with story regions, NPCs, enemies, campfires, and save slots.
- Battle state with action-value turns, QTE attacks, bullet-hell defense phases,
  result screens, status effects, mana, rage, skill pages, and localized UI.
- StoryDirector, DialogueState, Maelle recruitment, audio buses, and data-driven
  layout/audio/skill/status files.

The missing work is mostly cohesion: a deeper progression loop, more readable
combat mastery, stronger encounter authoring, better route pacing, and a visual
target that keeps new art and UI consistent.

## Research Notes

The official Expedition 33 overview describes its combat as turn-based with
real-time dodge, parry, counter, combo rhythm, and free-aim weak-point actions:
https://www.expedition33.com/overview

The official gameplay preview explains the key design loop in more detail:
skills cost AP, skills can include timed inputs, dodging has a wider defensive
window, parrying has a tighter window and can counter after a full enemy combo,
free aim can solve boss mechanics, and exploration rewards players with Pictos
that teach passive Lumina abilities:
https://www.expedition33.com/post/your-first-look-at-gameplay-from-clair-obscur-expedition-33

Xbox Wire's beginner guide explains the long-term buildcraft loop: Pictos act
like accessories, mastering a Picto after four battles unlocks its passive for
the whole party if they have enough Lumina points. It also notes that level-ups
grant attribute points and skill tree points:
https://news.xbox.com/en-us/2025/04/23/tips-to-get-started-clair-obscur-expedition-33/

Xbox Wire's combat breakdown highlights encounter structure: exploration areas
hold enemies, secrets, and points of interest; striking enemies before they
strike you can grant turn advantage; combat mixes classic turn order with
real-time inputs:
https://news.xbox.com/en-us/2024/08/28/clair-obscur-expedition-33-combat-breakdown-preview/

The 2026 PlayStation development article reinforces the production lesson that
matters most for this project: small teams need data structures and tooling that
let designers combine gameplay elements freely instead of requiring bespoke code
for every skill, encounter, or visual:
https://blog.playstation.com/2026/03/11/how-the-clair-obscur-expedition-33-dev-process-powered-creative-design-freedom/

Patch 1.3.0 is also useful as a post-launch polish signal: timing windows,
damage multipliers, challenge modifiers, retry/rematch flow, collision, terrain,
and scripting polish all matter after the core systems already exist:
https://www.expedition33.com/post/patch-1-3-0-is-now-live

## Design Pillars

### 1. Reactive Turn-Based Combat

Every battle should ask the player to make strategic decisions on their turn and
perform readable timing actions on enemy turns. QTE attacks, bullet-hell defense,
and future dodge/parry windows should be authored per enemy and per skill from
data.

Implementation direction:

- Keep battle mutations inside `IAction::Execute`.
- Add a `ReactionDefenseAction` family for enemy attacks that need dodge, parry,
  block, counter, or bullet-hell variants.
- Add data-driven timing windows, telegraph SFX, and defensive result rewards.
- Track defensive results in `BattleResultTracker` so mastery is visible after
  battle.

### 2. Character-Specific Combat Engines

Verso and Maelle need different resource identities instead of just different
skill lists.

Initial direction:

- Verso: rage, execution, guard break, mark/vulnerable payoff, high-risk burst.
- Maelle: mana flow, burn setup, protection verse, revive/cleanse support, flame
  AoE payoff.
- Future recruit: design only after Verso and Maelle have strong loops.

Required systems:

- `CharacterCombatProfile` data file for per-character resource rules.
- Skill tags such as `breaker`, `setup`, `payoff`, `support`, `defensive`,
  `finisher`, and `risky`.
- UI details that explain why a skill matters, not only its cost.

### 3. Buildcraft Loop

Expedition 33's Pictos/Lumina loop works because it creates continuous equipment
rotation, mastery goals, and late-game passive buildcraft. This project should
use an original equivalent called `Echoes` and `Memories`.

Proposed terms:

- `Echo`: an equippable relic that grants stats and one passive while equipped.
- `Memory`: a passive unlocked after enough battle mastery with an Echo.
- `Memory Points`: per-character budget used to equip unlocked Memories.

V1 behavior:

- Echoes drop from story fights, optional elites, and route secrets.
- An Echo is mastered after N victorious battles while equipped.
- Mastered Memories become available to all recruited party members.
- Memories can react to QTE results, status application, mana recovery, rage
  gain, kill bonuses, low HP, campfire rest state, or enemy type.

Data files:

- `data/echoes/*.json`
- `data/memories/*.json`
- `data/memory_rules.json`
- `data/equipment_slots.json`

### 4. Encounter Variety

The current enemy list is too small for a full game loop. Each route segment
needs enemies that teach one mechanic and then combine mechanics later.

Encounter tiers:

- Tutorial scout: teaches attack QTE and basic defense.
- Shielded guard: teaches break, vulnerable, and guard-up icons.
- Burning caster: teaches cleanse and damage-over-time urgency.
- Armoured zombie: teaches bullet-hell pattern recognition and defensive buffs.
- Maelle duel: teaches one-on-one duel pacing and story battle retry.
- Clone gate boss: tests status setup, burst timing, and defensive mastery.

Authoring rule:

Every encounter JSON should declare its design purpose:

```json
{
  "designRole": "teaches_guard_break",
  "expectedParty": [ "verso", "maelle" ],
  "mechanicTags": [ "break", "weak", "parry_combo" ],
  "rewardTags": [ "echo", "story_key" ]
}
```

### 5. Exploration With Motivation

The overworld should guide the player through a readable route while rewarding
side curiosity. The current regions already form a usable skeleton, but the route
needs gates, optional rewards, and more story beats.

Route structure:

1. Rue Cendre: Verso alone, first scout, first campfire.
2. Sister Crossing: Maelle confrontation and duel.
3. Silent Market: optional shop stock, first Echo reward, burn enemy.
4. Western Watch: side campfire, optional elite, world lore.
5. Pilgrim Crossing: patrol encounter and route gate.
6. Glass Shrine: zombie armour cluster, cleanse/revive test.
7. Mirror Gate: clone boss and chapter endpoint.

Overworld systems to add:

- `ObjectiveDirector`: current objective, completed objective, next hint.
- `WorldInteractable`: one data-driven object type for lore, chests, doors,
  memory cages, and route blockers.
- `EncounterGate`: validates story flags before allowing route progress.
- `RouteRewardTable`: declares coin, Echo, item, and story rewards.

### 6. Campfire As Progress Hub

Campfire should remain the only place for save, load, equipment changes, party
loadout, Echo/Memory setup, shop, and character upgrades. This prevents menu
cheats and makes campfires meaningful.

Campfire V2 menu:

- Rest
- Save / Load
- Shop
- Upgrade Attributes
- Equipment
- Echoes / Memories
- Party Talk
- Leave

The menu should use the existing campfire gating model. New rows are data-driven
and should expose lock reasons when unavailable.

### 7. Visual And Audio Cohesion

Visual polish should be systemic, not just new PNGs. Existing filters, particle
layers, battle result effects, 9-slice UI, and BGM rules should be consolidated
into authored style packages.

Add:

- `data/visual_style_presets.json`
- `data/battle_cinematic_profiles.json`
- `data/ui_style_tokens.json`
- `data/audio_event_rules.json`

Rules:

- UI chrome uses one family of 9-slice panels and gold/red accent tokens.
- Battle timing windows always have SFX support, because audio is a timing cue.
- Status icons are always visible near HP bars and repeated in detail panels.
- Generated art is accepted only after slicing/validation scripts produce final
  predictable assets.

## Generated Asset Direction

The first generated visual target sheet is saved here:

```text
docs/21-full-game-polish-roadmap/assets/visual-target-sheet-v1.png
```

It is not a final runtime atlas. It is a reference sheet for:

- Battle skill card motifs.
- Status icon silhouettes.
- Campfire hub composition.
- Victory and defeat result symbols.

The prompt used the built-in `image_gen` workflow and explicitly requested
original Belle Epoque dark-fantasy pixel-art concepts without existing game
logos, characters, or UI.

Next asset tasks:

1. Select usable motifs from the target sheet.
2. Generate focused individual assets with flat backgrounds when transparency is
   needed.
3. Run deterministic Pillow scripts under `patches/` to slice, trim, validate,
   and pack final atlases.
4. Commit source sheets, scripts, final atlases, and JSON metadata together.

Suggested asset deliverables:

- `assets/UI/polish/status_icons_v2.png`
- `assets/UI/polish/battle_panel_v2.png`
- `assets/UI/polish/result_sigils_v2.png`
- `assets/animations/campfire_hub_v2.png`
- `assets/environments/paris_route_props_v2.png`

## Implementation Roadmap

### Sprint 1: Combat Timing And Readability

Goal: make every enemy turn readable and fair.

- Add `data/reaction_windows/*.json`.
- Add `ReactionDefenseAction`.
- Add dodge/parry/block result enum and event payload.
- Route result stats into `BattleResultTracker`.
- Add timing SFX ids to attack pattern data.
- Add debug overlay for reaction windows.

Verification:

- A player can learn timing from animation and SFX.
- Defeat never feels like invisible damage.
- Result screen reports defensive mastery.

### Sprint 2: Echo And Memory Buildcraft

Goal: create long-term progression beyond level and skill list.

- Add Echo and Memory registries.
- Add equipment slots for Echoes.
- Add mastery counters persisted in saves.
- Add campfire Echo/Memory menu.
- Add passives that hook into existing battle events.

Verification:

- Equipping an Echo changes combat immediately.
- Mastery unlocks a reusable Memory.
- Save/load preserves equipped Echoes and mastered Memories.

### Sprint 3: Encounter Director

Goal: make route combat intentional instead of repeated enemies.

- Add encounter design metadata.
- Add pre-battle advantage from overworld contact direction.
- Add optional elite and route gate encounters.
- Add boss mechanic data for clone gate.

Verification:

- Each route segment teaches or combines a mechanic.
- Optional fights have meaningful rewards.
- Story fights do not disappear on defeat leave.

### Sprint 4: Exploration And Story Completion

Goal: make the first chapter playable from start to endpoint.

- Add objective director.
- Add world interactables and memory cages.
- Add Paris route lore pickups.
- Add post-Maelle banter and campfire party talks.
- Add Mirror Gate chapter endpoint sequence.

Verification:

- The player always knows the next main goal.
- Side paths reward exploration.
- Maelle joining changes both story dialogue and gameplay.

### Sprint 5: Visual/Audio Style Pass

Goal: reduce visual mismatch and improve perceived quality.

- Convert generated concepts into validated atlases.
- Add UI token file for gold, red, panel alpha, typography scale, and frame paths.
- Add battle cinematic profiles for camera, particles, vignette, and result effects.
- Add audio event rules for UI, QTE, parry, dodge, hit, defeat, and victory.

Verification:

- Skill UI, pause, campfire, result, and save slot screens look related.
- Combat timing has consistent audio cues.
- Generated assets are processed through scripts and not manually cropped.

## Full Playable Definition

The first full playable chapter is complete only when all of the following are
true:

- New Game starts with Verso alone.
- The player can recruit Maelle through the sister duel.
- The route has at least six purposeful combat encounters and one endpoint boss.
- Campfire supports save/load, rest, shop, equipment, upgrades, and Echo/Memory
  loadout.
- Combat includes attack QTE, defensive reaction, bullet-hell, status effects,
  mana, rage, healing, revive, cleanse, AoE, rewards, victory, defeat, retry,
  and flee.
- Exploration has at least three optional rewards or secrets.
- Save/load restores story flags, party roster, wallet, inventory, equipment,
  Echo/Memory progression, objective, and player position.
- English, Vietnamese, and French player-facing text fits in all relevant UI.
- Build succeeds with `.\build_src_static.bat 2>&1`.
- A manual smoke test can play from title screen to chapter endpoint without
  dead-end state, invisible progression, or missing core UI.

## Next Concrete Change

Implement Sprint 1 first. It gives the greatest return because the game already
has QTE attacks and bullet-hell defense, but enemy turns need a unified timing
language before encounter count, buildcraft, and boss mechanics scale further.
