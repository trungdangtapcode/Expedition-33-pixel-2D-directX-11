// ============================================================
// File: OverworldState.cpp
// Responsibility: Camera-follow gameplay state - overworld exploration.
//
// Architecture:
//   OverworldState knows NOTHING about how ControllableCharacter moves or renders.
//   It spawns the character into SceneGraph, then calls only:
//     mScene.Update(dt)   - drives all entity logic
//     mScene.Render(ctx)  - drives all entity drawing
//   Camera follow is the only reason OverworldState holds a ControllableCharacter*,
//   and only the narrow GetX()/GetY() interface is used.
//
//   OverworldEnemy and OverworldStaticProp entities are spawned the same way.
//   OverworldState only keeps non-owning enemy pointers for battle proximity;
//   static props need no tracking after SceneGraph takes ownership.
//
// Battle trigger (2-phase sequence - NO iris in overworld):
//   1. PINCUSHION  - B pressed near enemy:
//                    TimeSystem::SetSlowMotion(0.25) slows gameplay.
//                    PincushionDistortionFilter ramps intensity 0->1 over
//                    kPincushionDuration seconds using the UI clock (wall-accurate).
//   2. IDLE (push) - intensity reached 1.0:
//                    slow-motion reset to 1.0, BattleState pushed immediately.
//
//   BattleState::OnEnter() starts its own iris at radius=0 (black) then opens.
//   BattleState pops -> OverworldState resumes normally (no iris state to manage).
//
// Scene:
//   ControllableCharacter    - Verso sprite, WASD controlled, SceneGraph-owned.
//   CheckpointCampfire       - save/load and campfire hub anchors.
//   OverworldStaticProp      - large data-driven props sorted with entities.
//   OverworldEnemy (1..N)    - stationary enemies, SceneGraph-owned.
//   Camera2D                 - follows ControllableCharacter with smooth lerp.
//   Story overlay            - data-driven area and objective text.
//   ColorGradeFilter         - subtle world-only biome mood pass.
//   PincushionDistortionFilter - fullscreen warp effect during transition phase.
//
// Input:
//   W / A / S / D - move the Verso character
//   ESC           - open PauseState while the overworld is idle
//   B (near enemy) - trigger battle transition (pincushion -> push BattleState)
//   U (near campfire) - open the CampfireState hub
//   L (near campfire) - open the party lineup
// ============================================================
#include "OverworldState.h"
#include "StateManager.h"
#include "BattleState.h"
#include "InventoryState.h"
#include "CampfireState.h"
#include "LineupState.h"
#include "PauseState.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/ZoomPincushionTransitionController.h"
#include "../Systems/GameProgress.h"
#include "../Systems/LocalizationManager.h"
#include "../Systems/SaveManager.h"
#include "../Systems/Wallet.h"
#include "../Core/TimeSystem.h"
#include "../Core/InputManager.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"
#include <DirectXColors.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <windows.h>

namespace
{
    bool ReadablePathExists(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path) ||
               std::filesystem::exists(std::filesystem::path("..") / path);
    }

    bool EnemyAssetsExist(const EnemyEncounterData& data)
    {
        if (!ReadablePathExists(std::filesystem::path(data.texturePath))) return false;
        if (!ReadablePathExists(std::filesystem::path(data.jsonPath))) return false;

        for (const EnemySlotData& slot : data.battleParty)
        {
            if (!ReadablePathExists(std::filesystem::path(slot.texturePath))) return false;
            if (!ReadablePathExists(std::filesystem::path(slot.jsonPath))) return false;
            if (!slot.turnViewPath.empty() &&
                !ReadablePathExists(std::filesystem::path(slot.turnViewPath)))
            {
                return false;
            }
        }

        return true;
    }
}

// ------------------------------------------------------------
// Function: OnEnter
// Purpose:
//   1. Build Camera2D from current screen dimensions.
//   2. Load the tile map and data-driven story/theme data.
//   3. Load the Verso SpriteSheet from JSON.
//   4. Spawn ControllableCharacter into SceneGraph.
//   5. Spawn campfires, static props, and overworld enemies from JSON.
//   6. Initialize world color grade and battle transition filters.
//   7. Subscribe to window_resized to keep Camera2D in sync.
//   8. Subscribe to battle_end_victory to remove defeated enemies.
//   9. Subscribe to checkpoint_loaded so campfire slot loads rebuild the map.
// ------------------------------------------------------------
void OverworldState::OnEnter()
{
    LOG("[OverworldState] OnEnter");

    EventManager::Get().Broadcast("bgm_play_overworld", {});
    ID3D11Device*        device  = D3DContext::Get().GetDevice();
    ID3D11DeviceContext* context = D3DContext::Get().GetContext();
    const int W = D3DContext::Get().GetWidth();
    const int H = D3DContext::Get().GetHeight();

    // --- Tile Map ---
    if (!mTileMap.Initialize(device, context, "assets/environments/overworld_map.json")) {
        LOG("[OverworldState] WARNING - Tile map failed to load.");
    }

    // --- Camera ---
    mCamera = std::make_unique<Camera2D>(W, H);

    const std::string storyFontPath = LocalizationManager::Get().GetCurrentFontPath();
    mStoryTextRenderer.Initialize(
        device, context,
        std::wstring(storyFontPath.begin(), storyFontPath.end()),
        W, H);
    mCurrencyHud.Initialize(
        device, context,
        std::wstring(storyFontPath.begin(), storyFontPath.end()),
        W, H);
    LoadStoryData();
    mCurrentArea = mDefaultArea;
    mCurrentObjective = mDefaultObjective;

    if (!mThemeManager.Initialize("data/overworld_themes.json"))
    {
        LOG("[OverworldState] WARNING - Overworld themes failed to load; using neutral grade.");
    }
    mThemeManager.SetTheme(mDefaultThemeId, true);

    mColorGradeFilter = std::make_unique<ColorGradeFilter>();
    if (!mColorGradeFilter->Initialize(device, W, H))
    {
        LOG("[OverworldState] WARNING - ColorGradeFilter init failed; overworld themes will skip post-process.");
        mColorGradeFilter.reset();
    }
    else
    {
        mColorGradeFilter->SetSettings(mThemeManager.GetCurrentGrade());
    }

    // --- Load Verso sprite sheet ---
    SpriteSheet sheet;
    if (!JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet)) {
        LOG("[OverworldState] ERROR - Failed to load verso.json.");
        return;
    }

    const OverworldProgressSnapshot savedWorld =
        GameProgress::Get().CaptureOverworldSnapshot();
    const SaveCheckpointConfig& saveConfig = SaveManager::Get().GetConfig();
    const float startX = savedWorld.hasPlayerPosition
        ? savedWorld.playerX
        : saveConfig.defaultPlayerX;
    const float startY = savedWorld.hasPlayerPosition
        ? savedWorld.playerY
        : saveConfig.defaultPlayerY;

    // --- Spawn player character ---
    mPlayer = mScene.Spawn<ControllableCharacter>(
        device, context,
        L"assets/animations/verso.png",
        sheet,
        std::string("idle"),
        startX, startY,
        mCamera.get(),
        &mTileMap.GetData().colliders
    );

    UpdateStoryRegion(startX, startY);
    if (const OverworldStoryRegion* initialRegion = FindStoryRegion(startX, startY))
    {
        mThemeManager.SetTheme(initialRegion->themeId.empty()
                                   ? mDefaultThemeId
                                   : initialRegion->themeId,
                               true);
    }
    else
    {
        mThemeManager.SetTheme(mDefaultThemeId, true);
    }
    if (mColorGradeFilter)
    {
        mColorGradeFilter->SetSettings(mThemeManager.GetCurrentGrade());
    }

    std::vector<CheckpointCampfireData> campfireData;
    if (LoadCampfireData(campfireData))
    {
        for (const CheckpointCampfireData& data : campfireData)
        {
            CheckpointCampfire* campfire = mScene.Spawn<CheckpointCampfire>(
                device, context, data, mCamera.get());
            if (campfire) mCampfires.push_back(campfire);
        }
    }
    else
    {
        LOG("[OverworldState] WARNING: No checkpoint campfires were loaded.");
    }

    std::vector<OverworldStaticPropData> propData;
    if (LoadStaticPropData(propData))
    {
        for (const OverworldStaticPropData& data : propData)
        {
            mScene.Spawn<OverworldStaticProp>(device, context, data, mCamera.get());
        }
    }
    else
    {
        LOG("[OverworldState] WARNING: No overworld static props were loaded.");
    }

    // --- Spawn overworld enemies ---
    // Positions live in data/overworld_spawns.json so encounter pacing can
    // follow the map story without recompiling this state.
    std::vector<OverworldEnemySpawnData> enemySpawns;
    if (LoadEnemySpawnData(enemySpawns))
    {
        for (const OverworldEnemySpawnData& spawn : enemySpawns)
        {
            const std::string defeatedFlag = "enemy_defeated:" + spawn.id;
            if (GameProgress::Get().HasFlag(defeatedFlag))
            {
                LOG("[OverworldState] Spawn '%s' skipped because it is already defeated.",
                    spawn.id.c_str());
                continue;
            }

            EnemyEncounterData encounterData{};
            if (JsonLoader::LoadEnemyEncounterData(spawn.encounterPath, encounterData))
            {
                if (!EnemyAssetsExist(encounterData))
                {
                    LOG("[OverworldState] WARNING - Spawn '%s' skipped because one or more assets are missing.",
                        spawn.id.c_str());
                    continue;
                }

                OverworldEnemy* enemy = mScene.Spawn<OverworldEnemy>(
                    device, context, encounterData, spawn.worldX, spawn.worldY, mCamera.get());
                if (enemy)
                {
                    mOverworldEnemies.push_back(enemy);
                    mEnemySpawnIds[enemy] = spawn.id;
                }
            }
            else
            {
                LOG("[OverworldState] WARNING - Could not load encounter '%s' for spawn '%s'.",
                    spawn.encounterPath.c_str(), spawn.id.c_str());
            }
        }
    }
    else
    {
        LOG("[OverworldState] WARNING - No overworld enemy spawns were loaded.");
    }

    // --- Pincushion and Camera Effect for battle transition ---
    // Created via the concrete type but stored behind the interface.
    mTransitionController = std::make_unique<ZoomPincushionTransitionController>();
    if (!mTransitionController->Initialize(device, W, H))
    {
        LOG("[OverworldState] WARNING - ZoomPincushionTransitionController init failed; battle transition will skip distortion.");
        mTransitionController.reset();  // disable filter rather than crash on use
    }

    // Reset transition state in case this state is re-entered (e.g., returning from battle).
    mBattleTransitionPhase = BattleTransitionPhase::IDLE;

    // Subscribe to window resize so Camera2D stays in sync.
    mResizeListenerID = EventManager::Get().Subscribe("window_resized",
        [this](const EventData&)
        {
            const int nW = D3DContext::Get().GetWidth();
            const int nH = D3DContext::Get().GetHeight();
            if (mCamera) mCamera->SetScreenSize(nW, nH);
            if (mTransitionController) mTransitionController->OnResize(nW, nH);
            if (mColorGradeFilter)
            {
                mColorGradeFilter->Shutdown();
                if (mColorGradeFilter->Initialize(D3DContext::Get().GetDevice(), nW, nH))
                {
                    mColorGradeFilter->SetSettings(mThemeManager.GetCurrentGrade());
                }
            }
            mStoryTextRenderer.SetScreenSize(nW, nH);
            mCurrencyHud.SetScreenSize(nW, nH);
            LOG("[OverworldState] window_resized -> %dx%d", nW, nH);
        });

    // Subscribe to "battle_end_victory" to remove the defeated overworld enemy.
    // This fires inside BattleState::Update() (deferred-exit block) while OverworldState
    // is still alive underneath the stack.  It is safe to access mOverworldEnemies
    // because OverworldState has NOT been destroyed yet.
    //
    // Why erase from mOverworldEnemies immediately?
    //   MarkDefeated() sets mAlive=false, and SceneGraph::PurgeDead() will free the
    //   UniquePtr on the next Update() pass.  If we leave the raw pointer in
    //   mOverworldEnemies past that point, the proximity loop would dereference
    //   freed memory.  Erasing here, before PurgeDead() runs, prevents that.
    mVictoryListenerID = EventManager::Get().Subscribe("battle_end_victory",
        [this](const EventData&)
        {
            if (mPendingEnemySource)
            {
                if (!mPendingEnemySpawnId.empty())
                {
                    GameProgress::Get().SetFlag("enemy_defeated:" + mPendingEnemySpawnId);
                }
                if (mPlayer)
                {
                    const std::string battleCheckpoint = mPendingEnemySpawnId.empty()
                        ? std::string("after_battle")
                        : ("after_battle:" + mPendingEnemySpawnId);
                    UpdateSavedOverworldSnapshot(battleCheckpoint,
                                                  mPlayer->GetX(),
                                                  mPlayer->GetY());
                }

                // Mark the entity dead so SceneGraph::PurgeDead() frees it next frame.
                mPendingEnemySource->MarkDefeated();

                // Remove the raw pointer from the tracking vector before PurgeDead()
                // frees the entity; any later access would be a use-after-free.
                mOverworldEnemies.erase(
                    std::remove(mOverworldEnemies.begin(),
                                mOverworldEnemies.end(),
                                mPendingEnemySource),
                    mOverworldEnemies.end());
                mEnemySpawnIds.erase(mPendingEnemySource);

                LOG("[OverworldState] battle_end_victory -> enemy defeated and removed from overworld.");
                SaveManager::Get().SaveCheckpoint("battle_victory");
                mPendingEnemySource = nullptr;
                mPendingEnemySpawnId.clear();
            }
        });

    mCheckpointLoadedListenerID = EventManager::Get().Subscribe("checkpoint_loaded",
        [this](const EventData&)
        {
            mReloadFromCheckpoint = true;
        });
}

// ------------------------------------------------------------
// Function: OnExit
// Purpose:
//   Release all GPU resources.
//   SceneGraph::Clear() destroys all entities (unique_ptr destructor calls
//   ControllableCharacter/OverworldEnemy destructors -> Shutdown()).
//   mPlayer and mOverworldEnemies become dangling after Clear() - clear them first.
// ------------------------------------------------------------
void OverworldState::OnExit()
{
    LOG("[OverworldState] OnExit");

    // Stop BGM when OverworldState is fully dismissed (e.g., transitioning to MenuState).
    // Not broadcast when BattleState is pushed - that push triggers its own
    // "bgm_play_battle" event, so the audio transitions cleanly without a gap.
    EventManager::Get().Broadcast("bgm_stop", {});

    EventManager::Get().Unsubscribe("window_resized", mResizeListenerID);
    EventManager::Get().Unsubscribe("battle_end_victory", mVictoryListenerID);
    EventManager::Get().Unsubscribe("checkpoint_loaded", mCheckpointLoadedListenerID);

    // Clear the source pointer regardless of whether a battle was in progress.
    // Prevents any stale pointer dereference if OnExit() is called mid-battle
    // (e.g., a forced state change that bypasses the normal victory path).
    mPendingEnemySource = nullptr;
    mPendingEnemySpawnId.clear();

    mTileMap.Shutdown();
    mStoryTextRenderer.Shutdown();
    mCurrencyHud.Shutdown();

    if (mColorGradeFilter)
    {
        mColorGradeFilter->Shutdown();
        mColorGradeFilter.reset();
    }

    // Release transition controller GPU resources.
    if (mTransitionController)
    {
        mTransitionController->Shutdown();
        mTransitionController.reset();
    }

    // Ensure transition state is clean for potential re-entry.
    mBattleTransitionPhase = BattleTransitionPhase::IDLE;

    // Clear non-owning pointers BEFORE SceneGraph::Clear() frees the entities.
    // Accessing these pointers after Clear() is a use-after-free.
    mPlayer = nullptr;
    mOverworldEnemies.clear();
    mEnemySpawnIds.clear();
    mCampfires.clear();

    // Destroy all SceneGraph entities (ControllableCharacter, OverworldEnemy, etc.).
    mScene.Clear();

    mCamera.reset();
    mBWasDown = false;
    mIWasDown = false;
    mLWasDown = false;
    mFWasDown = false;
    mCWasDown = false;
    mUWasDown = false;
    mReloadFromCheckpoint = false;
}

bool OverworldState::LoadCampfireData(std::vector<CheckpointCampfireData>& outCampfires) const
{
    namespace fs = std::filesystem;

    fs::path path("data/campfires.json");
    std::ifstream file(path);
    if (!file.is_open())
    {
        path = fs::path("..") / "data/campfires.json";
        file.clear();
        file.open(path);
    }

    if (!file.is_open())
    {
        LOG("[OverworldState] Cannot open campfire config.");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();
    JsonLoader::detail::WarnIfUTF16(src, path.string());

    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "campfires");

    for (const std::string& objectSrc : objects)
    {
        CheckpointCampfireData data{};
        data.id = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "id"));

        const std::string texturePath = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "texturePath"));
        data.texturePath = std::wstring(texturePath.begin(), texturePath.end());

        data.jsonPath = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "jsonPath"));
        data.idleClip = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "idleClip"));
        if (data.idleClip.empty()) data.idleClip = "idle";

        data.worldX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "worldX"), 0.0f);
        data.worldY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "worldY"), 0.0f);
        data.contactRadius = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "contactRadius"), 80.0f);
        data.renderScale = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "renderScale"), 1.0f);
        data.upgradeExpReward = JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(objectSrc, "upgradeExpReward"), 0);

        if (data.id.empty() || texturePath.empty() || data.jsonPath.empty())
        {
            LOG("[OverworldState] WARNING: Skipping invalid campfire entry.");
            continue;
        }

        outCampfires.push_back(data);
    }

    LOG("[OverworldState] Loaded %zu checkpoint campfire(s).", outCampfires.size());
    return !outCampfires.empty();
}

// ------------------------------------------------------------
// Function: LoadEnemySpawnData
// Purpose:
//   Load overworld enemy positions from data/overworld_spawns.json.
// Why:
//   Encounter placement is part of map pacing, so designers should be able
//   to move enemies without recompiling OverworldState.
// ------------------------------------------------------------
bool OverworldState::LoadEnemySpawnData(std::vector<OverworldEnemySpawnData>& outSpawns) const
{
    namespace fs = std::filesystem;

    fs::path path("data/overworld_spawns.json");
    std::ifstream file(path);
    if (!file.is_open())
    {
        path = fs::path("..") / "data/overworld_spawns.json";
        file.clear();
        file.open(path);
    }

    if (!file.is_open())
    {
        LOG("[OverworldState] Cannot open overworld spawn config.");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();
    JsonLoader::detail::WarnIfUTF16(src, path.string());

    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "spawns");

    for (const std::string& objectSrc : objects)
    {
        OverworldEnemySpawnData data{};
        data.id = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "id"));
        data.encounterPath = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "encounterPath"));
        data.worldX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "worldX"), 0.0f);
        data.worldY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "worldY"), 0.0f);

        if (data.id.empty() || data.encounterPath.empty())
        {
            LOG("[OverworldState] WARNING: Skipping invalid overworld spawn entry.");
            continue;
        }

        outSpawns.push_back(data);
    }

    LOG("[OverworldState] Loaded %zu overworld enemy spawn(s).", outSpawns.size());
    return !outSpawns.empty();
}

// ------------------------------------------------------------
// Function: LoadStaticPropData
// Purpose:
//   Load large overworld prop placements from data/overworld_props.json.
// Why:
//   Props that must sort against the player need entity rendering, but their
//   placement remains map data and should not be hardcoded in OverworldState.
// Parameters:
//   outProps - appended with validated static prop records.
// Returns:
//   true when at least one prop record was loaded.
// ------------------------------------------------------------
bool OverworldState::LoadStaticPropData(std::vector<OverworldStaticPropData>& outProps) const
{
    namespace fs = std::filesystem;

    fs::path path("data/overworld_props.json");
    std::ifstream file(path);
    if (!file.is_open())
    {
        path = fs::path("..") / "data/overworld_props.json";
        file.clear();
        file.open(path);
    }

    if (!file.is_open())
    {
        LOG("[OverworldState] Cannot open overworld prop config.");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();
    JsonLoader::detail::WarnIfUTF16(src, path.string());

    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "props");

    for (const std::string& objectSrc : objects)
    {
        OverworldStaticPropData data{};
        data.id = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "id"));

        const std::string texturePath = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "texturePath"));
        data.texturePath = std::wstring(texturePath.begin(), texturePath.end());

        data.sourceX = JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(objectSrc, "sourceX"), 0);
        data.sourceY = JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(objectSrc, "sourceY"), 0);
        data.sourceWidth = JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(objectSrc, "sourceWidth"), 0);
        data.sourceHeight = JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(objectSrc, "sourceHeight"), 0);
        data.worldX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "worldX"), 0.0f);
        data.worldY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "worldY"), 0.0f);
        data.pivotX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "pivotX"), 0.0f);
        data.pivotY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "pivotY"), 0.0f);
        data.scale = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "scale"), 1.0f);
        data.layer = JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(objectSrc, "layer"), 50);
        data.sortYOffset = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "sortYOffset"), 0.0f);

        if (data.id.empty() ||
            texturePath.empty() ||
            data.sourceWidth <= 0 ||
            data.sourceHeight <= 0)
        {
            LOG("[OverworldState] WARNING: Skipping invalid static prop entry.");
            continue;
        }

        outProps.push_back(data);
    }

    LOG("[OverworldState] Loaded %zu overworld static prop(s).", outProps.size());
    return !outProps.empty();
}

// ------------------------------------------------------------
// Function: LoadStoryData
// Purpose:
//   Load area names and objectives for the overworld story overlay.
// Why:
//   The objective text is player-facing narrative content, so it belongs in
//   data rather than in rendering code.
// ------------------------------------------------------------
bool OverworldState::LoadStoryData()
{
    namespace fs = std::filesystem;

    fs::path path("data/overworld_story.json");
    std::ifstream file(path);
    if (!file.is_open())
    {
        path = fs::path("..") / "data/overworld_story.json";
        file.clear();
        file.open(path);
    }

    if (!file.is_open())
    {
        LOG("[OverworldState] Cannot open overworld story config.");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();
    JsonLoader::detail::WarnIfUTF16(src, path.string());

    const std::string defaultAreaKey = JsonLoader::detail::CleanString(
        JsonLoader::detail::ValueOf(src, "defaultAreaKey"));
    const std::string defaultArea = JsonLoader::detail::CleanString(
        JsonLoader::detail::ValueOf(src, "defaultArea"));
    if (!defaultArea.empty() || !defaultAreaKey.empty())
    {
        mDefaultArea = LocalizationManager::Get().TextOrFallback(defaultAreaKey, defaultArea);
    }

    const std::string defaultObjectiveKey = JsonLoader::detail::CleanString(
        JsonLoader::detail::ValueOf(src, "defaultObjectiveKey"));
    const std::string defaultObjective = JsonLoader::detail::CleanString(
        JsonLoader::detail::ValueOf(src, "defaultObjective"));
    if (!defaultObjective.empty() || !defaultObjectiveKey.empty())
    {
        mDefaultObjective = LocalizationManager::Get().TextOrFallback(
            defaultObjectiveKey,
            defaultObjective);
    }

    const std::string defaultThemeId = JsonLoader::detail::CleanString(
        JsonLoader::detail::ValueOf(src, "defaultThemeId"));
    if (!defaultThemeId.empty())
    {
        mDefaultThemeId = defaultThemeId;
    }

    mStoryRegions.clear();
    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "regions");

    for (const std::string& objectSrc : objects)
    {
        OverworldStoryRegion region{};
        region.id = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "id"));
        region.name = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "name"));
        region.nameKey = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "nameKey"));
        region.objective = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "objective"));
        region.objectiveKey = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "objectiveKey"));
        region.themeId = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "themeId"));
        region.name = LocalizationManager::Get().TextOrFallback(region.nameKey, region.name);
        region.objective = LocalizationManager::Get().TextOrFallback(
            region.objectiveKey,
            region.objective);
        region.minX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "minX"), 0.0f);
        region.minY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "minY"), 0.0f);
        region.maxX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "maxX"), 0.0f);
        region.maxY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "maxY"), 0.0f);

        if (region.id.empty() || region.name.empty() || region.objective.empty())
        {
            LOG("[OverworldState] WARNING: Skipping invalid story region entry.");
            continue;
        }

        mStoryRegions.push_back(region);
    }

    LOG("[OverworldState] Loaded %zu overworld story region(s).", mStoryRegions.size());
    return !mStoryRegions.empty();
}

CheckpointCampfire* OverworldState::FindNearbyCampfire(float px, float py) const
{
    for (CheckpointCampfire* campfire : mCampfires)
    {
        if (campfire && campfire->IsPlayerNearby(px, py))
        {
            return campfire;
        }
    }
    return nullptr;
}

const OverworldStoryRegion* OverworldState::FindStoryRegion(float px, float py) const
{
    for (const OverworldStoryRegion& region : mStoryRegions)
    {
        if (px >= region.minX && px <= region.maxX &&
            py >= region.minY && py <= region.maxY)
        {
            return &region;
        }
    }
    return nullptr;
}

// ------------------------------------------------------------
// Function: UpdateStoryRegion
// Purpose:
//   Refresh the active area label and objective from the player's position.
// Why:
//   Region-specific objectives give the large map short-term goals without
//   adding a full quest system yet.
// ------------------------------------------------------------
void OverworldState::UpdateStoryRegion(float px, float py)
{
    if (const OverworldStoryRegion* region = FindStoryRegion(px, py))
    {
        mCurrentArea = region->name;
        mCurrentObjective = region->objective;
        mThemeManager.SetTheme(region->themeId.empty() ? mDefaultThemeId : region->themeId);
        return;
    }

    mCurrentArea = mDefaultArea;
    mCurrentObjective = mDefaultObjective;
    mThemeManager.SetTheme(mDefaultThemeId);
}

// ------------------------------------------------------------
// Function: UpdateSavedOverworldSnapshot
// Purpose:
//   Store the current overworld restore point in GameProgress.
// Why:
//   SaveManager is intentionally state-neutral; OverworldState owns the live
//   player position and must publish it before a slot is written.
// ------------------------------------------------------------
void OverworldState::UpdateSavedOverworldSnapshot(const std::string& checkpointId, float px, float py)
{
    OverworldProgressSnapshot snapshot{};
    snapshot.sceneId = SaveManager::Get().GetConfig().autoSceneId;
    snapshot.checkpointId = checkpointId;
    snapshot.playerX = px;
    snapshot.playerY = py;
    snapshot.hasPlayerPosition = true;
    GameProgress::Get().SetOverworldSnapshot(snapshot);
}

bool OverworldState::HandleCampfireInput(float px, float py)
{
    const bool fDown = (GetAsyncKeyState('F') & 0x8000) != 0;
    const bool cDown = (GetAsyncKeyState('C') & 0x8000) != 0;
    const bool uDown = (GetAsyncKeyState('U') & 0x8000) != 0;
    const bool lDown = (GetAsyncKeyState('L') & 0x8000) != 0;

    const bool fPressed = fDown && !mFWasDown;
    const bool cPressed = cDown && !mCWasDown;
    const bool uPressed = uDown && !mUWasDown;
    const bool lPressed = lDown && !mLWasDown;

    mFWasDown = fDown;
    mCWasDown = cDown;
    mUWasDown = uDown;
    mLWasDown = lDown;

    CheckpointCampfire* campfire = FindNearbyCampfire(px, py);
    if (!campfire)
    {
        if (lPressed)
        {
            LOG("[OverworldState] L pressed away from campfire; lineup is campfire-only.");
        }
        return false;
    }

    const CheckpointCampfireData& data = campfire->GetData();

    if (lPressed)
    {
        StateManager::Get().PushState(std::make_unique<LineupState>());
        LOG("[OverworldState] Campfire '%s' opened the party lineup.", data.id.c_str());
        return true;
    }

    if (fPressed)
    {
        UpdateSavedOverworldSnapshot("campfire:" + data.id, px, py);
        SaveManager::Get().SaveCheckpoint("campfire_save:" + data.id);
        LOG("[OverworldState] Campfire '%s' saved the active slot.", data.id.c_str());
        return false;
    }

    if (cPressed)
    {
        std::string sceneId;
        if (!SaveManager::Get().LoadCheckpoint(&sceneId))
        {
            LOG("[OverworldState] Campfire '%s' could not load the active slot.", data.id.c_str());
            return false;
        }

        if (sceneId != SaveManager::Get().GetConfig().autoSceneId)
        {
            LOG("[OverworldState] Loaded scene '%s' is not implemented; using overworld.",
                sceneId.c_str());
        }

        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
        return true;
    }

    if (uPressed)
    {
        StateManager::Get().PushState(
            std::make_unique<CampfireState>(data.id, data.upgradeExpReward, px, py));
        LOG("[OverworldState] Campfire '%s' opened the campfire menu.", data.id.c_str());
        return true;
    }

    return false;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   1. Handle ESC -> push PauseState when no battle transition is active.
//   2. Delegate all entity logic to SceneGraph::Update(dt).
//   3. Refresh story region and blend its world color theme.
//   4. Check proximity to overworld enemies; if B pressed near one -> start transition.
//   5. Handle pincushion phase: ramp filter intensity using UI clock dt.
//   6. Push BattleState directly when pincushion completes (no iris in overworld).
//   7. Camera follow via the narrow GetX()/GetY() interface.
//
// Battle trigger sequence:
//   Phase IDLE:
//     B pressed + IsPlayerNearby() (no already-transitioning guard needed):
//       -> set mBattleTransitionPhase = PINCUSHION
//       -> reset mPincushionTimer to 0
//       -> TimeSystem::SetSlowMotion(0.25) - gameplay slows to 25%
//
//   Phase PINCUSHION (each frame):
//     -> mPincushionTimer += UI clock dt (wall-accurate, ignores slow-mo)
//     -> intensity = mPincushionTimer / kPincushionDuration  clamped to [0,1]
//     -> mPincushionFilter->Update(uiDt, intensity)
//     -> when timer >= kPincushionDuration:
//         -> TimeSystem::SetSlowMotion(1.0) - restore normal speed
//         -> StateManager::PushState(BattleState) immediately
//         -> mBattleTransitionPhase = IDLE
//
//   Why use UI clock for pincushion timer?
//     The pincushion should last kPincushionDuration real-world seconds regardless
//     of the slow-motion scale.  Using the gameplay clock (which runs at 0.25x)
//     would stretch the distortion to 2.4s instead of 0.6s - inconsistent feel.
// ------------------------------------------------------------
void OverworldState::Update(float dt)
{
    if (mReloadFromCheckpoint)
    {
        TimeSystem::Get().SetSlowMotion(1.0f);
        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
        return;
    }

    if (InputManager::Get().IsKeyPressed(VK_ESCAPE) &&
        mBattleTransitionPhase == BattleTransitionPhase::IDLE)
    {
        StateManager::Get().PushState(std::make_unique<PauseState>());
        return;
    }

    // ---------------------------------------------------------------
    // 'I' key - open the inventory.  One-press semantics via mIWasDown
    // so the same press that opens InventoryState does not also
    // immediately close it on the next frame (InventoryState's own
    // OnEnter starts with mIWasDown=true to absorb the held key).
    //
    // PushState rather than ChangeState so the overworld is preserved
    // beneath the stack and resumes unchanged on PopState.  Same
    // pattern BattleState uses to overlay on top of overworld.
    // ---------------------------------------------------------------
    {
        const bool iDown    = (GetAsyncKeyState('I') & 0x8000) != 0;
        const bool iPressed = iDown && !mIWasDown;
        mIWasDown = iDown;
        if (iPressed)
        {
            const bool allowEquipmentChanges =
                mPlayer &&
                FindNearbyCampfire(mPlayer->GetX(), mPlayer->GetY()) != nullptr;
            StateManager::Get().PushState(
                std::make_unique<InventoryState>(allowEquipmentChanges));
            LOG("[OverworldState] Inventory opened with equipment changes %s.",
                allowEquipmentChanges ? "enabled" : "locked outside campfire");
            return;  // do NOT run the rest of Update - the new state owns this frame
        }
    }

    // All entity logic (WASD, physics, animation, enemy idle) runs here.
    // dt is gameplay-clock-scaled so entities respect slow-motion automatically.
    mScene.Update(dt);

    if (mPlayer && mBattleTransitionPhase == BattleTransitionPhase::IDLE)
    {
        UpdateStoryRegion(mPlayer->GetX(), mPlayer->GetY());
    }

    mThemeManager.Update(dt);
    if (mColorGradeFilter)
    {
        mColorGradeFilter->SetSettings(mThemeManager.GetCurrentGrade());
        mColorGradeFilter->Update(dt, 1.0f);
    }

    if (mPlayer && mBattleTransitionPhase == BattleTransitionPhase::IDLE)
    {
        if (HandleCampfireInput(mPlayer->GetX(), mPlayer->GetY()))
        {
            return;
        }
    }

    // ---------------------------------------------------------------
    // Battle trigger: B key + enemy proximity + IDLE phase only.
    // The IDLE phase guard prevents re-triggering while a transition is active.
    // ---------------------------------------------------------------
    const bool bDown    = (GetAsyncKeyState('B') & 0x8000) != 0;
    const bool bPressed = bDown && !mBWasDown;
    mBWasDown = bDown;

    if (bPressed && mBattleTransitionPhase == BattleTransitionPhase::IDLE && mPlayer)
    {
        const float px = mPlayer->GetX();
        const float py = mPlayer->GetY();

        // Find the closest enemy within contact radius.
        // First match wins - ties resolved by vector order (spawn order).
        OverworldEnemy* target = nullptr;
        for (OverworldEnemy* enemy : mOverworldEnemies)
        {
            if (enemy && enemy->IsAlive() && enemy->IsPlayerNearby(px, py))
            {
                target = enemy;
                break;
            }
        }

        if (target)
        {
            // Copy encounter data before beginning the transition; the enemy
            // entity must NOT be accessed from the iris-close callback because
            // it might have been purged by the time the callback fires.
            mPendingEncounter   = target->GetEncounterData();
            mPendingEnemySource = target;
            const auto idIt = mEnemySpawnIds.find(target);
            mPendingEnemySpawnId = (idIt != mEnemySpawnIds.end()) ? idIt->second : "";

            // Start the transition phase: slow gameplay and begin distortion ramp.
            mBattleTransitionPhase = BattleTransitionPhase::PINCUSHION;
            if (mTransitionController)
            {
                mTransitionController->StartTransition(mPendingEncounter, mPendingEnemySource);
            }
            TimeSystem::Get().SetSlowMotion(0.25f);

            LOG("[OverworldState] Battle triggered vs '%s' - transition started.",
                mPendingEncounter.name.c_str());
        }
        else
        {
            LOG("[OverworldState] B pressed but no enemy in range (contactRadius check failed).");
        }
    }

    // ---------------------------------------------------------------
    // PINCUSHION phase: transition visual effect update.
    // ---------------------------------------------------------------
    if (mBattleTransitionPhase == BattleTransitionPhase::PINCUSHION)
    {
        // UI clock dt: ignores slow-motion
        const float uiDt = TimeSystem::Get().GetUIClock().GetDeltaTime();

        if (mTransitionController)
        {
            mTransitionController->Update(uiDt, mCamera.get());

            if (mTransitionController->IsFinished())
            {
                // Restore normal speed before handing control to BattleState.
                TimeSystem::Get().SetSlowMotion(1.0f);

                // Push BattleState - OverworldState stays alive underneath the stack.
                StateManager::Get().PushState(
                    std::make_unique<BattleState>(D3DContext::Get(), mPendingEncounter));

                // Reset transition state so OverworldState is ready when it resumes.
                mBattleTransitionPhase = BattleTransitionPhase::IDLE;
                mTransitionController->ClearPending();

                if (mCamera)
                {
                    mCamera->SetZoom(1.0f);
                    mCamera->SetRotation(0.0f);
                }

                LOG("[OverworldState] Transition complete - BattleState pushed, slow-motion restored.");
            }
        }
        else
        {
            // Fallback if no transition controller exists
            TimeSystem::Get().SetSlowMotion(1.0f);
            StateManager::Get().PushState(
                std::make_unique<BattleState>(D3DContext::Get(), mPendingEncounter));
            mBattleTransitionPhase = BattleTransitionPhase::IDLE;
            LOG("[OverworldState] Filter missing - BattleState pushed immediately.");
        }
    }

    // Camera follow - only valid use of mPlayer* here.
    if (mPlayer && mCamera && mBattleTransitionPhase == BattleTransitionPhase::IDLE) {
        mCamera->Follow(mPlayer->GetX(), mPlayer->GetY(), kCameraSmoothing, dt);
        mCamera->Update();
    }
}

// ------------------------------------------------------------
// Function: RenderStoryOverlay
// Purpose:
//   Draw the current area title and objective.
// Why:
//   The expanded overworld needs visible narrative direction so the player
//   understands why each road and landmark matters.
// ------------------------------------------------------------
void OverworldState::RenderStoryOverlay()
{
    if (!mStoryTextRenderer.IsReady()) return;

    ID3D11DeviceContext* ctx = D3DContext::Get().GetContext();
    constexpr float x = 24.0f;
    constexpr float titleY = 22.0f;
    constexpr float objectiveY = 48.0f;

    mStoryTextRenderer.BeginBatch(ctx);
    mStoryTextRenderer.DrawStringRaw(mCurrentArea.c_str(), x + 2.0f, titleY + 2.0f,
                                     DirectX::Colors::Black);
    mStoryTextRenderer.DrawStringRaw(mCurrentArea.c_str(), x, titleY,
                                     DirectX::Colors::White);
    mStoryTextRenderer.DrawStringRaw(mCurrentObjective.c_str(), x + 2.0f, objectiveY + 2.0f,
                                     DirectX::Colors::Black);
    mStoryTextRenderer.DrawStringRaw(mCurrentObjective.c_str(), x, objectiveY,
                                     DirectX::Colors::PaleGoldenrod);
    mStoryTextRenderer.EndBatch();
}

void OverworldState::RenderCurrencyOverlay()
{
    ID3D11DeviceContext* ctx = D3DContext::Get().GetContext();
    mCurrencyHud.RenderTopRight(ctx, Wallet::Get().GetCoins());
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   1. Let the transition controller prepare any active world effect.
//   2. Draw background map layers behind SceneGraph entities.
//   3. Call SceneGraph::Render(ctx) - draws entities in layer and Y order.
//   4. Draw foreground map layers that should occlude entities.
//   5. Apply world-only color grade before screen-space UI.
//   6. If pincushion filter is active: restore back buffer, apply warp.
//
// Draw order:
//   [BeginCapture if transition controller needs it]
//   TileMap background -> ground, roads, normal static map objects
//   SceneGraph         -> ascending layer order (enemies @48, player @50)
//   TileMap foreground -> canopies, roofs, and above-player map overlays
//   ColorGradeFilter   -> biome mood on world content only
//   [EndCapture + transition render if active - applies battle transition]
//
// No iris in OverworldState - BattleState owns its own iris that opens on entry.
// ------------------------------------------------------------
void OverworldState::Render()
{
    if (!mCamera) return;

    ID3D11DeviceContext* ctx = D3DContext::Get().GetContext();
    // Determine whether the transition effect should run this frame.
    // The concrete controller decides internally whether it captures or uses
    // a copy-based post-process path.
    const bool filterActive = mTransitionController && mTransitionController->IsActive();

    // --- Redirect scene draws to offscreen render target ---
    if (filterActive)
        mTransitionController->BeginCapture(ctx);

    mTileMap.RenderBackground(ctx, *mCamera);

    // --- All SceneGraph entities (sorted by layer, self-rendering) ---
    mScene.Render(ctx);

    // --- Above-player map layers ---
    mTileMap.RenderForeground(ctx, *mCamera);

    if (mColorGradeFilter && mColorGradeFilter->IsActive())
    {
        mColorGradeFilter->Render(ctx);
    }

    // --- Restore back buffer and apply pincushion distortion ---
    // EndCapture restores the saved RTV; Render draws the warped scene quad.
    if (filterActive)
    {
        mTransitionController->EndCaptureAndRender(ctx);
    }

    RenderStoryOverlay();
    RenderCurrencyOverlay();
}
