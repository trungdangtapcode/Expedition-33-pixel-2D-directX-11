#pragma once
#include "IGameState.h"
#include "../Systems/IBattleTransitionController.h"
#include "../Renderer/Camera.h"
#include "../Scene/SceneGraph.h"
#include "../Entities/ControllableCharacter.h"
#include "../Entities/OverworldEnemy.h"
#include "../Entities/CheckpointCampfire.h"
#include "../Entities/OverworldMemoryShard.h"
#include "../Entities/OverworldStaticProp.h"
#include "../Entities/OverworldNpc.h"
#include "../Battle/EnemyEncounterData.h"
#include "../UI/BattleTextRenderer.h"
#include "../UI/CurrencyHudRenderer.h"
#include "../UI/ObjectiveBeaconRenderer.h"
#include "../UI/ObjectiveTrackerRenderer.h"
#include "../Renderer/ColorGradeFilter.h"
#include "../Systems/ObjectiveDirector.h"
#include "../Systems/StoryDirector.h"
#include <memory>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "../Renderer/TileMapRenderer.h"
#include "../Systems/OverworldThemeManager.h"

// ============================================================
// File: OverworldState.h
// Responsibility: Camera-follow gameplay state - overworld exploration.
//
// Scene:
//   - ControllableCharacter        - WASD-controlled Verso sprite.
//   - CheckpointCampfire           - save/load and campfire hub anchor.
//   - OverworldStaticProp          - data-driven large props with Y-sorting.
//   - OverworldEnemy (1..N)        - stationary enemies with encounter data.
//   - Camera2D                     - follows the player with smooth lerp.
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
//   World entities render through Camera2D::GetViewMatrix(). Screen-space UI
//   such as story and coin overlays renders after world filters without camera.
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
    std::vector<std::string> requiresFlags;
    std::vector<std::string> blockedByFlags;
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
    std::string themeId;
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
    // Overworld color grade filter - applied to the world before UI so
    // region mood changes never reduce HUD or story text readability.
    // ---------------------------------------------------------------
    std::unique_ptr<ColorGradeFilter> mColorGradeFilter;

    // Region theme manager - loads biome mood data and blends settings
    // before passing the grade to ColorGradeFilter.
    OverworldThemeManager mThemeManager;

    // ---------------------------------------------------------------
    // Battle transition phase state machine (IDLE -> PINCUSHION -> IDLE).
    // ---------------------------------------------------------------
    BattleTransitionPhase mBattleTransitionPhase = BattleTransitionPhase::IDLE;

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

    // Memory shards are SceneGraph-owned; this vector observes them for
    // optional lore pickup prompts and one-time reward collection.
    std::vector<OverworldMemoryShard*> mMemoryShards;

    // Story NPCs are SceneGraph-owned; this vector only observes them for
    // proximity prompts, dialogue triggers, and authored route gates.
    std::vector<OverworldNpc*> mNpcs;

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
    std::string mPendingStoryBattleId;

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

    // One-press E key tracking - talks to a nearby NPC.
    bool mEWasDown = false;

    // One-press campfire interaction tracking. U opens the campfire hub;
    // F/C remain quick save/load shortcuts while standing near the fire.
    bool mFWasDown = false;
    bool mCWasDown = false;
    bool mUWasDown = false;

    // Story and objective text are data-driven so the map can communicate
    // player motivation without hardcoding route logic in rendering.
    BattleTextRenderer mStoryTextRenderer;
    CurrencyHudRenderer mCurrencyHud;
    ObjectiveBeaconRenderer mObjectiveBeacon;
    ObjectiveTrackerRenderer mObjectiveTracker;
    std::vector<OverworldStoryRegion> mStoryRegions;
    std::string mDefaultArea = "Ashen Meadow";
    std::string mDefaultObjective = "Follow the dirt road to the eastern gate.";
    std::string mDefaultThemeId = "ashen_meadow";
    ObjectiveView mCurrentObjectiveView;
    std::string mCurrentArea;
    std::string mCurrentObjectiveBody;
    std::string mCurrentObjectiveHint;
    std::string mInteractionPrompt;
    std::string mTimedPrompt;
    float mTimedPromptTimer = 0.0f;
    float mTimedPromptDuration = 2.5f;

    // ListenerID for "window_resized" - stored so we can Unsubscribe in OnExit.
    int mResizeListenerID = -1;

    // ListenerID for "battle_end_victory" - marks the source overworld enemy
    // as defeated so SceneGraph::PurgeDead() removes it on the next frame.
    int mVictoryListenerID = -1;
    int mDefeatListenerID = -1;
    int mFleeListenerID = -1;
    int mDialogueCompletedListenerID = -1;

    // ListenerID for "checkpoint_loaded" - campfire slot loads mutate managers
    // first, then this state rebuilds itself from the loaded snapshot.
    int mCheckpointLoadedListenerID = -1;
    bool mReloadFromCheckpoint = false;

    bool LoadCampfireData(std::vector<CheckpointCampfireData>& outCampfires) const;
    bool LoadEnemySpawnData(std::vector<OverworldEnemySpawnData>& outSpawns) const;
    bool LoadStaticPropData(std::vector<OverworldStaticPropData>& outProps) const;
    bool LoadNpcData(std::vector<OverworldNpcData>& outNpcs) const;
    bool LoadMemoryShardData(std::vector<OverworldMemoryShardData>& outShards) const;
    bool LoadStoryData();
    bool LoadFeedbackData();
    bool IsEnemySpawnAvailable(const OverworldEnemySpawnData& spawn) const;
    bool IsMemoryShardAvailable(const OverworldMemoryShardData& shard) const;
    CheckpointCampfire* FindNearbyCampfire(float px, float py) const;
    OverworldNpc* FindNearbyNpc(float px, float py) const;
    OverworldMemoryShard* FindNearbyMemoryShard(float px, float py) const;
    void RemoveMemoryShardObserver(OverworldMemoryShard* shard);
    OverworldEnemy* FindNearbyEnemy(float px, float py) const;
    OverworldEnemy* FindEnemyBySpawnId(const std::string& spawnId) const;
    OverworldEnemy* FindObjectiveEnemyTarget(float px, float py) const;
    const OverworldStoryRegion* FindStoryRegion(float px, float py) const;
    void UpdateStoryRegion(float px, float py);
    void UpdateSavedOverworldSnapshot(const std::string& checkpointId, float px, float py);
    void ApplyNpcRouteBlocks(float px, float py);
    void ApplyNpcVisibilityFlags();
    bool BeginBattleTransition(const EnemyEncounterData& encounter,
                               OverworldEnemy* enemySource,
                               const std::string& enemySpawnId,
                               const std::string& storyBattleId);
    bool ExecuteStoryCommand(const StoryCommand& command);
    bool ProcessStoryCommands(float dt);
    bool ProcessQueuedStoryCommands(float dt);
    bool BeginRuntimeStoryCommand(const StoryCommand& command);
    bool UpdateRuntimeStoryCommand(float dt);
    void FinishRuntimeStoryCommand();
    void SetTimedPrompt(const std::string& text);
    bool HandleNpcInput(float px, float py);
    bool HandleMemoryShardInput(float px, float py);
    void RenderStoryOverlay();
    void RenderInteractionPrompt();
    void RenderCurrencyOverlay();
    bool HandleCampfireInput(float px, float py);

    StoryDirector mStoryDirector;
    // ObjectiveDirector reads the same durable progress flags as save/load and
    // StoryDirector, but never mutates them. It only returns HUD guidance.
    ObjectiveDirector mObjectiveDirector;

    // Story commands may include timed cutscene steps. A deque lets commands
    // resume after DialogueState or BattleState pops without losing order.
    std::deque<StoryCommand> mStoryCommandQueue;
    StoryCommand mActiveStoryCommand;
    bool mStoryCommandRunning = false;
    bool mStoryPlayerControlLocked = false;
    bool mStoryCameraManual = false;
    float mStoryCommandTimer = 0.0f;
    float mStoryMoveStartX = 0.0f;
    float mStoryMoveStartY = 0.0f;
    float mStoryCameraStartX = 0.0f;
    float mStoryCameraStartY = 0.0f;
};
