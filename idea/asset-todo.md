# Asset TODO List

This is a list of art assets the engine **already references** but does not yet
have real art for. Every entry is currently rendered with a placeholder
(usually a tinted `mDialogBox` quad or a text glyph) so the game ships
playable, but each one is a slot waiting for a real PNG.

When art lands, replace the placeholder code path indicated under "where to
plug it in" — the data path (file location, JSON field) is already wired.

---

## Priority 1 — Battle item menu

These are referenced by the inventory UI you just saw on screen.

### 1.1 Item icons (15 PNGs, ~32x32 or 48x48)

Each item declares an `iconPath` in its JSON. The registry already warns
"icon missing" for every one of these but loads the item anyway.

| File | Item | Suggested visual |
|---|---|---|
| `assets/items/potion_small.png`  | Small Potion  | small green flask |
| `assets/items/potion_medium.png` | Medium Potion | medium green flask, brighter green |
| `assets/items/potion_large.png`  | Large Potion  | large green flask with cork |
| `assets/items/ether_small.png`   | Small Ether   | small blue vial |
| `assets/items/ether_medium.png`  | Medium Ether  | medium blue vial |
| `assets/items/elixir.png`        | Elixir        | crystal vial with golden liquid |
| `assets/items/phoenix_down.png`  | Phoenix Down  | red/orange feather |
| `assets/items/rage_gem.png`      | Rage Gem      | jagged orange-red crystal |
| `assets/items/antidote.png`      | Antidote      | clear vial with cyan swirl |
| `assets/items/power_tonic.png`   | Power Tonic   | flask with red+yellow stripes |
| `assets/items/iron_draft.png`    | Iron Draft    | flask with grey/silver swirl |
| `assets/items/swift_feather.png` | Swift Feather | white wing-feather |
| `assets/items/bomb.png`          | Bomb          | round black bomb with fuse |
| `assets/items/fire_bomb.png`     | Fire Bomb     | bomb wreathed in flame |
| `assets/items/ice_shard.png`     | Ice Shard     | jagged pale-blue crystal |

**Format suggestions:** 32x32 to 64x64 PNG with transparency. The current
placeholder fills the row's icon slot which is roughly `baseDialogHeight - 20px`
square (in world space).

**Where to plug it in:**
- [src/Battle/ItemRegistry.cpp](../src/Battle/ItemRegistry.cpp) already loads
  `iconPath` from JSON. Right now it only logs a warning when the file is
  missing. Add a `ComPtr<ID3D11ShaderResourceView>` to `ItemData` and a
  `LoadIconTextures(device, ctx)` pass that walks the registry once at
  battle start.
- [src/States/BattleState.cpp](../src/States/BattleState.cpp), inside the
  item menu render block, replace the `IconTint` placeholder lambda with
  a `mSpriteBatch->Draw(item->iconSRV, ...)` call.

Until then, the placeholder color-codes effect kind:

| Color | Effect kind |
|---|---|
| green   | HealHp / FullHeal |
| blue    | HealMp |
| pink    | Revive |
| orange  | RestoreRage |
| red     | DealDamage |
| yellow  | StatBuff |
| cyan    | Cleanse |

### 1.2 Scroll chevrons (1 PNG, any square size)

**Current state:** the chevrons are now drawn as real sprites with a
looping bob animation via [`ScrollArrowRenderer`](../src/UI/ScrollArrowRenderer.h),
**reusing `assets/UI/enemy-pointer-ui.png` as a temporary placeholder.**
The "up" instance renders the same texture rotated 180 degrees with the
bob direction inverted, so one PNG covers both directions.

**Size policy — read this before authoring art:**

The renderer auto-detects the texture's actual dimensions in `Initialize()`
via `ID3D11Texture2D::GetDesc()`. It is **size-agnostic** — drop in a
32x32, 64x64, 96x96, or 256x256 PNG and it just works. The on-screen
size is governed by [BattleState.cpp](../src/States/BattleState.cpp)
which computes:

```cpp
chevronSize  = baseDialogHeight * 0.55f;          // target world-space size
chevronScale = chevronSize / mChevronDown.GetWidth();
```

What this means in practice:
- A 128x128 PNG renders at the **same on-screen size** as a 64x64 PNG
  — just at twice the source resolution (sharper).
- To make the chevron render LARGER on screen, change the
  `chevronSize` formula in BattleState.cpp (e.g. `* 0.80f`).
- The pivot is always the texture center, so the 180-degree flip on
  the "up" instance always rotates around the visual center.
- A non-square texture works mechanically but will look distorted —
  the bob direction assumes a vertically-oriented chevron.

**What's still needed:**

| File | Use |
|---|---|
| `assets/UI/scroll_chevron.png` | dedicated chevron sprite — single direction (down); the up instance flips it at draw time |

**Suggested visual:** simple pixel-art chevron (V-shape) or arrow,
white or pale yellow, with a 1px dark outline so it reads against any
battle background. Square aspect ratio (32x32 / 64x64 / 128x128 are
all fine). Pivot is implicit (texture center). The current placeholder
is the enemy pointer, which is shaped like a target marker — fine for
testing but visually heavy compared to a tight chevron.

**No JSON sidecar needed.** Unlike `enemy-pointer-ui.json`,
`ScrollArrowRenderer` derives every metric from the PNG itself.

**Where to plug it in:**
- [src/States/BattleState.cpp](../src/States/BattleState.cpp), in
  `OnEnter`: change the two `mChevronUp.Initialize` /
  `mChevronDown.Initialize` calls to load `scroll_chevron.png` instead
  of `enemy-pointer-ui.png`. No other edits needed.

### 1.3 Scrollbar art (optional polish, 2 PNGs)

| File | Use |
|---|---|
| `assets/UI/scrollbar_track.png` | thin vertical track, can be a 1xN slice that tiles vertically |
| `assets/UI/scrollbar_thumb.png` | thumb grip with a slight bevel |

**Where to plug it in:**
- [src/States/BattleState.cpp](../src/States/BattleState.cpp), the
  `if (itemCount > visibleCount)` block draws the track and thumb as
  tinted `mDialogBox` quads. Swap the two `mDialogBox.Draw(...)` calls
  for `SpriteBatch` draws on the new textures.

---

## Priority 1.4 — Equipment items (5 PNGs)

The data layer for equipment now exists: items declare a `kind` and
optional `bonusAtk/bonusDef/...` fields, and `PartyManager` tracks
slot assignments that auto-flow into battles via
`GetEffectiveVersoStats()`. The inventory screen has a full Equipment
tab with stat preview. Five starter items are already in
`data/items/` and seeded in the inventory:

| File | Item | Slot | Suggested visual |
|---|---|---|---|
| `assets/items/short_sword.png`     | Short Sword     | Weapon    | small steel blade, leather grip |
| `assets/items/iron_sword.png`      | Iron Sword      | Weapon    | heavier two-handed look, dull grey |
| `assets/items/village_clothes.png` | Village Clothes | Body      | tan/brown linen tunic |
| `assets/items/leather_cap.png`     | Leather Cap     | Head      | brown leather skullcap |
| `assets/items/copper_ring.png`     | Copper Ring     | Accessory | thin copper band, faint glow |

These are colored placeholder squares today (kind-tinted in the UI:
bronze for weapons, violet for body armor, steel-grey for helmets,
gold for accessories). When real PNGs land, the same `iconPath`
mechanism the consumables use will pick them up — no code changes.

### Priority 1.4.1 — Equipment slot icons (4 small PNGs)

Optional polish for the Equipment tab. Each slot row currently shows
`"Slot:"` as text; a tiny icon next to it would read at a glance.

| File | Use |
|---|---|
| `assets/UI/slot_weapon.png`    | crossed swords or single sword silhouette |
| `assets/UI/slot_body.png`      | tunic / chestpiece silhouette |
| `assets/UI/slot_head.png`      | helmet silhouette |
| `assets/UI/slot_accessory.png` | ring or amulet silhouette |

**Where to plug them in:** [src/States/InventoryStateRender.cpp](../src/States/InventoryStateRender.cpp),
inside `RenderEquipmentTab` — the slot list loop currently draws
`SlotLabel(order[i])` as text. Add a small SpriteBatch quad before
each label that draws the matching slot PNG.

---

## Priority 1.5 — Overworld inventory screen

The overworld inventory screen ([InventoryState](../src/States/InventoryState.cpp))
opens when the player presses **`I`** in the overworld. It shares the
same `Inventory` singleton as battle, so any item used here disappears
from the next battle's menu and vice versa.

**Current state:** the screen ships **fully playable** by reusing existing
assets as placeholders. Nothing is missing for the system to work — every
entry below is a polish replacement, not a blocker.

### 1.5.1 Asset reuse map (what's borrowed)

| Slot | Currently uses | Should be replaced with |
|---|---|---|
| Backdrop / dim overlay | `assets/UI/ui-dialog-box-hd.png` tinted black @60% alpha | dedicated solid black PNG OR procedural quad |
| Menu container | `assets/UI/ui-dialog-box-hd.png` (battle dialog 9-slice) | `assets/UI/inventory_panel.png` — wider, less ornate framing |
| Item row background | `assets/UI/ui-dialog-box-hd.png` again, scaled smaller | `assets/UI/inventory_row.png` — slim row strip |
| Scroll chevrons | `assets/UI/enemy-pointer-ui.png` (target marker reused) | `assets/UI/scroll_chevron.png` (shared with battle item menu — see §1.2) |
| Text | `assets/fonts/arial_16.spritefont` | (font is fine — no replacement needed) |
| Item icons | None — names only render today | `assets/items/<id>.png` (the §1.1 list — shared between battle and overworld) |

The reuse is intentional: shipping the menu functionally before art exists
proves the data layer (Inventory singleton, item-use plumbing through
PartyManager) actually works end-to-end.

### 1.5.2 Dedicated overworld inventory assets (planned)

| File | Use | Suggested visual |
|---|---|---|
| `assets/UI/inventory_panel.png` + `.json` | The main menu container — replaces `ui-dialog-box-hd` for the inventory's centered dialog | Wider rectangle (~480x300), parchment / leather-bound book texture, 9-slice metadata for the borders |
| `assets/UI/inventory_row.png` + `.json` | Background strip for one item row | Thin horizontal panel, neutral tone, lighter version for the hovered/selected state OR use a single PNG and tint at draw time |
| `assets/UI/inventory_row_hover.png` | (optional) Highlighted row state | Same shape as `inventory_row.png` but yellow / cream tint baked in |
| `assets/UI/inventory_title_banner.png` | (optional) "INVENTORY" header bar at the top of the panel | Small banner with a stylized scroll/ribbon |
| `assets/UI/inventory_close_hint.png` | (optional) Bottom hint area showing "I/Esc to close" | Small key-cap icons + label |

**Format:** any PNG size with transparency works. The dialog box currently
uses 9-slice metadata (`assets/UI/ui-dialog-box-hd.json`) — a replacement
panel needs the same JSON shape so `NineSliceRenderer` knows where the
borders are. See [src/Renderer/NineSliceRenderer.h](../src/Renderer/NineSliceRenderer.h)
for the schema.

### 1.5.3 Out-of-battle item-use feedback (planned)

When the player uses a healing item from the inventory, today we show a
text flash like *"Used Small Potion (+30 HP)"* at the bottom of the
panel. A real game would show a small VFX over Verso's portrait too.

| File | Use |
|---|---|
| `assets/UI/portrait_verso_small.png` | Small portrait shown beside the menu so the player sees who they're healing (especially when multi-character party lands) |
| `assets/animations/inventory_heal_pulse.png` + `.json` | A short pulse VFX overlaid on the portrait when an HP item lands |
| `assets/animations/inventory_mana_pulse.png` + `.json` | Same for MP items |
| `assets/sound/inventory_open.wav` | Open / close menu sound |
| `assets/sound/inventory_use_heal.wav` | Healing item used (warm chime) |
| `assets/sound/inventory_reject.wav` | Item refused (low buzz, e.g. trying to use a Bomb in the overworld) |

**Where to plug them in:**
- [src/States/InventoryState.cpp](../src/States/InventoryState.cpp), inside `OnEnter`:
  load the new panel/row textures via `mDialogBox.Initialize(...)` (currently
  hardcoded to `ui-dialog-box-hd`). Add a second `NineSliceRenderer mRowBox`
  member if you want a distinct row background.
- [src/States/InventoryState.cpp](../src/States/InventoryState.cpp), inside `Render`:
  the row draw loop currently calls `mDialogBox.Draw` for each row's background.
  Swap to `mRowBox.Draw` (or a `SpriteBatch::Draw` on the dedicated row PNG).
- [src/States/InventoryState.cpp](../src/States/InventoryState.cpp), inside `TryUseItem`:
  on success, broadcast a new event (e.g. `"inventory_item_used"`) carrying
  the effect kind. Add a small VFX overlay member that listens and plays the
  matching pulse animation. SFX hooks the same way.

### 1.5.4 What the inventory screen does NOT need

These were considered and rejected:

- **Per-state dialog box class** — `NineSliceRenderer` is generic enough.
  The menu would only need a new class if its layout math diverged from
  the existing 9-slice draw pattern, which it doesn't.
- **A second font** — `arial_16.spritefont` covers titles, labels, and
  hint text. Adding a heading font is pure polish, not a blocker.
- **JSON sidecar for chevrons** — the chevron renderer auto-detects size
  via `D3D11_TEXTURE2D_DESC` (see §1.2). Drop in a PNG of any square
  size and it just works.

---

## Priority 2 — Item action animations (when items are used)

Currently item use just logs a line and applies the effect. There is no VFX,
no use animation. When art exists you'll want:

| File | Use |
|---|---|
| `assets/animations/vfx_heal.png`        | green sparkle for HealHp / FullHeal |
| `assets/animations/vfx_mana.png`        | blue swirl for HealMp / Ether |
| `assets/animations/vfx_revive.png`      | pink/gold burst for Phoenix Down |
| `assets/animations/vfx_buff.png`        | yellow upward arrow for StatBuff |
| `assets/animations/vfx_cleanse.png`     | cyan ripple for Antidote |
| `assets/animations/vfx_explosion.png`   | red/orange explosion for Bomb |
| `assets/animations/vfx_fire_aoe.png`    | wide flame sweep for Fire Bomb |
| `assets/animations/vfx_ice_pierce.png`  | white pierce streak for Ice Shard |

**Format:** sprite sheet, ~6-12 frames each. Use the same JSON descriptor
schema as `assets/animations/verso.json`.

**Where to plug it in:**
- New `IAction` subclass `ItemVfxAction` that spawns a `VFXSprite` into the
  battle scene at the target's slot position, then waits for the clip to
  finish. Insert it into `BuildItemActions::Build` between `LogAction` and
  `ItemEffectAction` so the visual plays before the stats change.

---

## Priority 3 — Damage type / crit feedback

The damage pipeline now produces `DamageResult::isCritical = true` 10% of
the time, but nothing on screen reflects that.

| File | Use |
|---|---|
| `assets/UI/crit_burst.png`              | radial flash drawn behind the damage number on a crit |
| `assets/animations/element_fire.png`    | fire ring overlay for fire-typed hits |
| `assets/animations/element_ice.png`     | ice shard overlay for ice-typed hits |

**Where to plug it in:**
- New `IDamageStep` is not the right place — the visual is a render-time
  effect, not a math step. Add a new event broadcast inside
  `Combatant::TakeDamage` (`"damage_landed"` with `DamageResult` payload),
  and a UI listener in `BattleState` that spawns a one-shot `VFXSprite`.

---

## Priority 4 — Status effect indicators

`Combatant` can hold any number of `IStatusEffect` instances but the
combatant sprite has no visual badge for them.

| File | Use |
|---|---|
| `assets/UI/status_weaken.png`   | red downward arrow on ATK/DEF |
| `assets/UI/status_buff_atk.png` | red upward arrow |
| `assets/UI/status_buff_def.png` | grey shield + up arrow |
| `assets/UI/status_buff_spd.png` | yellow lightning + up arrow |
| `assets/UI/status_poison.png`   | purple bubble (planned, no effect yet) |
| `assets/UI/status_burn.png`     | flame puff (planned, no effect yet) |
| `assets/UI/status_stun.png`     | yellow swirl (planned, no effect yet) |

**Where to plug it in:**
- New `StatusBadgeStrip` UI widget that reads `IBattler::GetStatModifiers()`
  and `IBattler::HasAnyStatusEffect()` to render a horizontal row of
  badges under each combatant's HP bar.
- Wire it from [src/States/BattleState.cpp](../src/States/BattleState.cpp)
  alongside `mEnemyHpBar` and `mHealthBar`.

---

## Priority 5 — Quality-of-life polish

| File | Use |
|---|---|
| `assets/UI/menu_cursor_hand.png`        | replacement for the current text-based hover indicator |
| `assets/UI/turn_indicator_glow.png`     | soft glow behind the active turn portrait |
| `assets/sound/item_use_generic.wav`     | "pop" SFX for any item use |
| `assets/sound/item_use_heal.wav`        | warm chime for healing items |
| `assets/sound/item_use_explosive.wav`   | boom for bombs |
| `assets/sound/scroll_tick.wav`          | quiet tick when the cursor moves in the item list |

---

## What's NOT on this list

These already exist and are working:
- Battle background / foreground (`assets/environments/battle-paris-view-*.png`)
- Verso + Skeleton sprite sheets (`assets/animations/verso.png`, `skeleton.png`)
- Turn-view portraits (`assets/UI/turn-view-*.png`)
- Player + enemy HP bars (`assets/UI/UI_verso_hp.png`, `enemy-hp-ui*.png`)
- Pointer cursor (`assets/UI/enemy-pointer-ui.png`)
- 9-slice dialog box (`assets/UI/ui-dialog-box-hd.png`)
- Iris transition (procedural, no PNG needed)

---

## Naming conventions

- Item icons: `assets/items/<item_id>.png` — must match `ItemData::iconPath`
  exactly. The registry compares paths character-for-character.
- VFX sprite sheets: `assets/animations/vfx_<name>.png` + matching `.json`
- UI elements: `assets/UI/<short_name>.png` (existing folder convention)
- SFX: `assets/sound/<category>_<name>.wav`
