#pragma once
#include "IGameState.h"
#include "../Renderer/CircleRenderer.h"
#include "../Systems/IBattleTransitionController.h"
#include "../Renderer/Camera.h"
#include "../Scene/SceneGraph.h"
#include "../Entities/ControllableCharacter.h"
#include "../Entities/OverworldEnemy.h"
#include "../Entities/CheckpointCampfire.h"
#include "../Battle/EnemyEncounterData.h"
#include "../Debug/DebugTextureViewer.h"
#include "../UI/BattleTextRenderer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../Renderer/TileMapRenderer.h"

// ============================================================
// File: OverworldState.h
// Responsibility: Camera-follow gameplay state - overworld exploration.
//
// Scene:
//   - Blue circle (static NPC)     - fixed world position, never moves.
//   - ControllableCharacter        - WASD-controlled Verso sprite (via SceneGraph).
//   - OverworldEnemy (1..N)        - stationary enemies with encounter data.
//   - Camera2D                     - follows the player character with smooth lerp.
//
// Architecture:
//   OverworldState does NOT know how the player moves, how many draw calls it needs,
//   or what texture it uses.  It only knows IGameObject* (via SceneGraph).
//   ControllableCharacter* mPlayer is kept solely to call GetX()/GetY()
//   for camera follow - that is the entire extent of this class's player knowledge.
//
//   OverworldEnemy* entries in mOverworldEnemies are non-owning; SceneGraph
//   holds sole ownership.  OverworldState uses them ONLY to check:
//     - IsPlayerNearby(px, py)    - proximity for "press B" prompt
//     - GetEncounterData()        - data package to hand to BattleState
//
// Battle trigger flow (2-phase, no iris in overworld):
//   1. PINCUSHION  - B pressed + enemy nearby:
//                    pincushion distortion ramps up over kPincushionDuration seconds
//                    using UI-clock dt (unaffected by slow-motion).
//                    TimeSystem::SetSlowMotion(0.25) slows gameplay simultaneously.
//   2. Push (IDLE) - intensity reached 1.0:
//                    slow-motion reset, BattleState pushed immediately onto the stack.
//
//   BattleState::OnEnter() starts its own iris at radius=0 then opens outward,
//   producing the classic circle-wipe reveal on the battle side.
//   BattleState pops -> OverworldState resumes normally (no iris to manage).
//
// World vs Screen:
//   Circles live in WORLD coordinates; Camera2D::WorldToScreen() converts them
//   each frame for CircleRenderer (SDF shader, no GPU VP matrix path).
// ============================================================

// Battle transition phase - controls the two-step trigger sequence.
// Declared outside the class so BattleState or other states never need to
// include OverworldState.h to read this enum; OverworldState is the sole owner.
enum class BattleTransitionPhase {
    IDLE,         // no transition active; overworld runs normally
    PINCUSHION    // pincushion distortion ramping up + slow-motion active
};

struct OverworldEnemySpawnData
{
    std::string id;
    std::string encounterPath;
    float worldX = 0.0f;
    float worldY = 0.0f;
};

struct OverworldStoryRegion
{
    std::string id;
    std::string name;
    std::string nameKey;
    std::string objective;
    std::string objectiveKey;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
};

class OverworldState : public IGameState {
public:
    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "OverworldState"; }

private:
    // ---------------------------------------------------------------
    // SDF circle renderer - still used for the static blue landmark.
    // ---------------------------------------------------------------
    CircleRenderer mCircleRenderer;

    // ---------------------------------------------------------------
    // TileMap renderer - draws the 2D background world grid.
    // ---------------------------------------------------------------
    TileMapRenderer mTileMap;

    // ---------------------------------------------------------------
    // Battle transition controller - encapsulates all visual effects
    // (pincushion, zoom, rotation) and timings used to transition 
    // from Overworld to BattleState.
    // ---------------------------------------------------------------
    std::unique_ptr<IBattleTransitionController> mTransitionController;

    // ---------------------------------------------------------------
    // Battle transition phase state machine (IDLE -> PINCUSHION -> IDLE).
    // ---------------------------------------------------------------
    BattleTransitionPhase mBattleTransitionPhase = BattleTransitionPhase::IDLE;

    // ---------------------------------------------------------------
    // Static blue circle - fixed world position, never updated.
    // Kept as a visual landmark to demonstrate world-space rendering.
    // ---------------------------------------------------------------
    static constexpr float kBlueX      = 400.0f;
    static constexpr float kBlueY      = 200.0f;
    static constexpr float kBlueRadius = 35.0f;

    // ---------------------------------------------------------------
    // Camera - follows the player character with smooth lerp.
    // std::unique_ptr defers construction until screen dimensions are known.
    // ---------------------------------------------------------------
    std::unique_ptr<Camera2D> mCamera;

    // Camera follow smoothing: higher = snappier (5 = fast, 2 = cinematic).
    static constexpr float kCameraSmoothing = 5.0f;

    // ---------------------------------------------------------------
    // SceneGraph - owns all IGameObject instances.
    // OverworldState calls only Update(dt) and Render(ctx) on it; it has no
    // knowledge of what entities live inside.
    // ---------------------------------------------------------------
    SceneGraph mScene;

    // Non-owning observer pointer to the spawned player character.
    // Used ONLY for GetX()/GetY() to drive camera follow.
    // SceneGraph retains sole ownership via unique_ptr.
    ControllableCharacter* mPlayer = nullptr;

    // ---------------------------------------------------------------
    // Overworld enemies - non-owning observer pointers.
    // SceneGraph owns the actual entities via unique_ptr.
    // Used by Update() for proximity checks and GetEncounterData().
    // Cleared in OnExit() BEFORE SceneGraph::Clear() frees the entities.
    // ---------------------------------------------------------------
    std::vector<OverworldEnemy*> mOverworldEnemies;

    // Campfires are SceneGraph-owned; this vector only observes them for
    // proximity checks and campfire-specific interaction input.
    std::vector<CheckpointCampfire*> mCampfires;

    // Encounter data copied from the nearby enemy when B is pressed.
    // Passed to BattleState constructor after the iris closes.
    EnemyEncounterData mPendingEncounter;

    // Non-owning pointer to the overworld enemy that triggered the last battle.
    // Set on B-press, cleared after victory callback fires (or on OnExit).
    // Used to call MarkDefeated() so the enemy disappears after a win.
    OverworldEnemy* mPendingEnemySource = nullptr;

    // Stable spawn id for the pending enemy; saved as enemy_defeated:<id>
    // after battle victory so the enemy does not respawn on load.
    std::string mPendingEnemySpawnId;

    // Maps live SceneGraph-owned enemies back to data/overworld_spawns.json ids.
    // The map is cleared before SceneGraph destroys the entities.
    std::unordered_map<OverworldEnemy*, std::string> mEnemySpawnIds;

    // One-press B key tracking - member variable (no static local) for clean
    // lifecycle management (reset to false in OnExit via destruction).
    bool mBWasDown = false;

    // One-press I key tracking - opens the InventoryState overlay.
    // Same pattern as mBWasDown so the key only fires on a fresh edge,
    // not while held.
    bool mIWasDown = false;

    // One-press L key tracking - opens the LineupState overlay only at campfires.
    bool mLWasDown = false;

    // One-press campfire interaction tracking. U opens the campfire hub;
    // F/C remain quick save/load shortcuts while standing near the fire.
    bool mFWasDown = false;
    bool mCWasDown = false;
    bool mUWasDown = false;

    // DEBUG: raw texture viewer - bypasses all sprite sheet / pivot math.
    DebugTextureViewer mDebugView;

    // Story objective text is data-driven by overworld_story.json so the map
    // can communicate player motivation without hardcoding text in rendering.
    BattleTextRenderer mStoryTextRenderer;
    std::vector<OverworldStoryRegion> mStoryRegions;
    std::string mDefaultArea = "Ashen Meadow";
    std::string mDefaultObjective = "Follow the dirt road to the eastern gate.";
    std::string mCurrentArea;
    std::string mCurrentObjective;

    // ListenerID for "window_resized" - stored so we can Unsubscribe in OnExit.
    int mResizeListenerID = -1;

    // ListenerID for "battle_end_victory" - marks the source overworld enemy
    // as defeated so SceneGraph::PurgeDead() removes it on the next frame.
    int mVictoryListenerID = -1;

    // ListenerID for "checkpoint_loaded" - campfire slot loads mutate managers
    // first, then this state rebuilds itself from the loaded snapshot.
    int mCheckpointLoadedListenerID = -1;
    bool mReloadFromCheckpoint = false;

    bool LoadCampfireData(std::vector<CheckpointCampfireData>& outCampfires) const;
    bool LoadEnemySpawnData(std::vector<OverworldEnemySpawnData>& outSpawns) const;
    bool LoadStoryData();
    CheckpointCampfire* FindNearbyCampfire(float px, float py) const;
    const OverworldStoryRegion* FindStoryRegion(float px, float py) const;
    void UpdateStoryRegion(float px, float py);
    void UpdateSavedOverworldSnapshot(const std::string& checkpointId, float px, float py);
    void RenderStoryOverlay();
    bool HandleCampfireInput(float px, float py);
};
