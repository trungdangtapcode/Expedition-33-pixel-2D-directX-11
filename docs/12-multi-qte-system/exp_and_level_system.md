# Experience and Leveling System Integration

This document covers the implementation of the organic Experience (EXP) accumulating pipeline and its respective Graphical User Interface (GUI) hooks executed natively inside the DirectX engine during the Victory screen.

## 1. The Core Architecture (Backend Variables)

The game incorporates organic JRPG leveling mechanics by strictly augmenting `BattlerStats`.
*   **Characters**: `level` and `exp` base structs were natively merged inside the `BattlerStats.h` definition alongside dynamic Stat scalers mapping through `BattlerGrowth` configurations natively sourced from definitions (`verso.json`, `maelle.json`).
*   **Mathematical Curve**: Native mathematical scaling is resolved inside `PartyManager::AddExp` structurally mimicking professional implementations: `Threshold = 100 * (Level ^ 1.5f)`.
*   **Victory Flow Execution**: `BattleManager` calculates encountered total EXP into a localized `mTotalExpPool`. When all enemies reach their death state and `CheckDeathAnimations(...)` cascades, the loop completes via `BattleOutcome::VICTORY`, pooling and cleanly awarding the exp centrally.

## 2. Asset Generation
Instead of bloating the codebase or manually opening Photoshop, a Python script `scratch/make_exp_bars.py` was deployed. By utilizing the `PIL` (Pillow) library, the script automatically scaffolds perfect 140x10 structural textures directly into `assets/UI`:
1.  `exp-bar-bg.png` — Dark green structured bounding box overlay.
2.  `exp-bar-fill.png` — Gold, bright-yellow filling.

## 3. The `ExpBarRenderer`
The graphical pipeline securely uses absolute Rect clipping limits to prevent visual texture warping via `DirectX::SpriteBatch` logic:
*   The C++ Renderer class dynamically calculates `ratio = currentExp / nextLevelExp`.
*   Instead of transforming or scaling geometrically, it explicitly evaluates:
    ```cpp
    long fillPixelWidth = static_cast<long>(kBarWidth * ratio);
    RECT srcRect = { 0, 0, fillPixelWidth, static_cast<long>(kBarHeight) };
    mSpriteBatch->Draw(mFillSRV.Get(), XMFLOAT2(mRenderX, mRenderY), &srcRect);
    ```
    This securely maps golden pixels matching structural progress dynamically without horizontal stretching artifacts!

## 4. UI Visibility State Hooks (The BattleState)
The system binds tightly internally below `HealthBarRenderer`.
*   **Initialization**: Loops inside `BattleState::InitUIRenderers()` sequentially calculating offsets exactly matching the party indexing variables!
*   **Text Layers**: Uses `BattleTextRenderer` implicitly mapping bold SpriteFonts writing out `Lv. [X]` and `[EXP]/[MAX]` centered natively directly over the HUD.
*   **Hidden Mid-Battle Config**: To avoid HUD clutter during mid-battle routines (as EXP only updates at the culmination of an encounter), the conditional `if (mBattle.GetOutcome() == BattleOutcome::VICTORY)` wraps the UI. 
*   **Outcome**: The renderer intelligently hides the graphical bar and text outputs securely until the decisive win condition screen triggers the ultimate victory phase!
