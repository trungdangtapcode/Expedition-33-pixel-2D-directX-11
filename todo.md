

Todo:
- QTE
- Enhance walking anim
- Develop statical formula and system for battle

- Refactor structure of code, make it more modular and easier to maintain: FOLDER STRUCTURE
- PlayerInputPhase::CHARACTER_ACTION_SELECT -> PlayerInputPhase::SKILL_SELECT


- Queue randomize system
- Add background assets
-  with clear separation between damage types (physical, magical, true damage) and modifiers (buffs, debuffs, critical hits).
- Items and inventory system
- Winning screen -> reward screen with loot and experience points

- Sprite asset/draw refactor — Sprite + SpriteLoader + SpriteDraw pattern
  Goal: kill the "fat renderer" trap (every visible thing = its own class
  with its own SpriteBatch + hardcoded mWidth fallback). Three roles, three
  owners: Sprite (asset facts: SRV + auto-detected source size + named
  regions), SpriteDraw (stateless draw helpers), call sites (target size +
  layout). Source size always queried from PNG via D3D11_TEXTURE2D_DESC,
  never hardcoded. Target size always lives at the call site.
  Rollout (NOT one big PR):
    1. Add src/Renderer/Sprite.h (struct)
    2. Add src/Renderer/SpriteLoader.h/.cpp (PNG load + optional JSON sidecar)
    3. Add src/Renderer/SpriteDraw.h/.cpp (Draw / DrawSized / DrawRegion)
    4. Migrate ScrollArrowRenderer first (smallest demo) — delete the class,
       inline the chevron draw at the BattleState call site
    5. Migrate PointerRenderer (fixes its mWidth=64 hardcoded fallback)
    6. Migrate NineSliceRenderer regions into Sprite::regions
    7. HealthBarRenderer / EnemyHpBarRenderer keep their layout/animation
       state but their mTextureSRV becomes a Sprite member; draw calls go
       through SpriteDraw::DrawRegion("hp_fill", ...)
    8. WorldSpriteRenderer same — animation state machine stays, raw SRV
       becomes a Sprite, frame draws go through DrawRegion
  Rule going forward: writing `int mWidth = 64;` anywhere in a renderer is
  a defect — query the texture descriptor instead. See chat thread for
  rationale and the audit of which renderers currently follow it.


Done:
- Enhance hp bar
- Render order in battle state
- UI when in turn based battle
- Camera can rotate
- [fix] Hp disappear when dead but it's not waiting for the animation to end

- ready to fight animation
- Move in battle
- Attack animations
- dealing dame sync with animation

- Add agility stat to determine turn order, 

- enemy also need to move
- Add turn order UI
- A readable damage calculation formula and system,