// ============================================================
// File: SaveManager.h
// Responsibility: Own checkpoint save/load orchestration and JSON
//                 persistence for durable game progress.
//
// Owns:
//   - Save checkpoint configuration loaded from data/save_checkpoints.json.
//   - Event subscription that writes an auto-checkpoint after battle victory.
//
// Does not own:
//   - Party stats or equipment. PartyManager is the authority.
//   - Item counts. Inventory is the authority.
//   - Active state transitions. MenuState decides where to go after load.
//
// Lifetime:
//   Created on first Get() call as a Meyers singleton.
//   Lives for the process lifetime so its battle-victory listener remains valid.
// ============================================================
#pragma once

#include <string>

struct SaveCheckpointConfig
{
    std::string slotPath = "save/checkpoint_slot_0.json";
    std::string autoCheckpointId = "overworld_after_battle";
    std::string autoSceneId = "overworld";
    std::string iconPath = "assets/UI/save_checkpoint_badge.png";
};

class SaveManager
{
public:
    // ------------------------------------------------------------
    // Function: Get
    // Purpose:
    //   Return the process-wide save manager.
    // Why:
    //   Save/load is a cross-state service and should not be owned by a
    //   specific screen or battle instance.
    // ------------------------------------------------------------
    static SaveManager& Get();

    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose:
    //   Force construction and report readiness to caller code.
    // Why:
    //   MenuState calls this on entry so auto-checkpoint subscription exists
    //   before gameplay begins.
    // ------------------------------------------------------------
    bool Initialize() const { return true; }

    bool CheckpointExists() const;
    bool SaveCheckpoint(const std::string& reason) const;
    bool LoadCheckpoint(std::string* outSceneId = nullptr) const;

    const SaveCheckpointConfig& GetConfig() const { return mConfig; }

private:
    SaveManager();

    SaveManager(const SaveManager&)            = delete;
    SaveManager& operator=(const SaveManager&) = delete;

    void LoadConfig();
    void SubscribeAutoCheckpoint();

    SaveCheckpointConfig mConfig;
    int mVictoryListenerId = -1;
};
