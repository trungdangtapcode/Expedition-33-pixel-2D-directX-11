# Data-Driven Battle Menu Animations

## Objective
To implement clean, robust entry animations (slide-in from the side and a smooth fade-in) for our turn-based Battle Menus (`Command Select` & `Skill Select`) without adding monolithic `if-else` loops or hardcoded pixel numbers into `BattleState`. All adjustments must flow from `data/battle_menu_layout.json`.

## Core Mechanisms
We combined the following elements to create smooth, staggered menu rendering:

### 1. State Tracking Timers
Two primary floats track animation timelines: `mCmdMenuTimer` and `mSkillMenuTimer`. When `mInputController` switches `PlayerInputPhase`, the relevant timer resets seamlessly.

```cpp
if (mLastInputPhase != currentInputPhase) {
    if (currentInputPhase == PlayerInputPhase::COMMAND_SELECT) mCmdMenuTimer = 0.0f;
    if (currentInputPhase == PlayerInputPhase::SKILL_SELECT) mSkillMenuTimer = 0.0f;
    mLastInputPhase = currentInputPhase;
}
```

### 2. Delaying Animation (Entry Delay)
For the `Skill Select` phase specifically, the UI shouldn't spawn exactly on frame 1 because other events (like the Stance Engine pulling the character's sword out and the BattleCamera zooming) are simultaneously running. By extending a struct field `entryDelay`, we halt visual accumulation using `std::max`.

```cpp
float activeSkillTimer = (std::max)(0.0f, mSkillMenuTimer - mMenuLayout.skill.entryDelay);
float t = (std::min)(activeSkillTimer / mMenuLayout.skill.entryDuration, 1.0f);
```

### 3. Alpha Blending Override in UI Stack
Previously, `NineSliceRenderer::Draw` did not support varying opacity paths. We expanded its method signature:
```cpp
void Draw(ID3D11DeviceContext* context, float destX, float destY, float targetWidth, float targetHeight,
          float scale = 1.0f, DirectX::CXMMATRIX transform = DirectX::XMMatrixIdentity(),
          DirectX::XMVECTOR color = DirectX::Colors::White); // <--- Added Color Multiplier
```
Both `SpriteBatch` inside the `NineSliceRenderer` and `BattleTextRenderer` use `DirectX::XMVectorSetW(textColor, currentAlpha)` guaranteeing the outer bounding box and font text fade uniformly.

### 4. Interpolation Formulars
Cubic Ease-Out creates satisfying "snap" effects where the menu enters fast but gracefully slides to a halt.

```cpp
// Cubic ease out math
float easeT = 1.0f - std::pow(1.0f - t, 3.0f);

// Delta values evaluated from battle_menu_layout.json
float slideOffset = mMenuLayout.skill.slideOffsetX * (1.0f - easeT);
float currentAlpha = fadeStartAlpha + (1.0f - fadeStartAlpha) * easeT;
```

## JSON Configuration Layout
The JSON allows instant fine-tuning to perfectly match UI reveals with specific combatant combat stances.

```json
{
  "skill_entryDelay": 0.35,        // UI waits 0.35 sec (allows char sword unsheathe)
  "skill_entryDuration": 0.35,     // Then slides and fades in across 0.35s
  "skill_slideOffsetX": -40.0      // UI starts 40 pixels to the left and slides back to 0
}
```
