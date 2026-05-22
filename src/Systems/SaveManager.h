// ============================================================
// File: SaveManager.h
// Responsibility: Own save-slot orchestration and JSON persistence for
//                 durable game progress.
//
// Owns:
//   - Save-slot configuration loaded from data/save_checkpoints.json.
//   - Event subscription that writes an auto-save after battle victory.
//
// Does not own:
//   - Party stats or equipment. PartyManager is the authority.
//   - Item counts. Inventory is the authority.
//   - Live overworld entities. GameProgress carries only restore metadata.
//   - Active state transitions. MenuState decides where to go after load.
//
// Lifetime:
//   Created on first Get() call as a Meyers singleton.
//   Lives for the process lifetime so its battle-victory listener remains valid.
// ============================================================
#pragma once

#include <string>
#include <vector>

struct SaveCheckpointConfig
{
    std::string slotPath = "save/checkpoint_slot_0.json";
    std::string slotDirectory = "save";
    std::string slotFilePrefix = "checkpoint_slot_";
    std::string slotFileExtension = ".json";
    int slotCount = 3;
    int defaultSlotIndex = 0;
    std::string autoCheckpointId = "overworld_after_battle";
    std::string autoSceneId = "overworld";
    std::string defaultCheckpointId = "new_game";
    float defaultPlayerX = 0.0f;
    float defaultPlayerY = 0.0f;
    std::string iconPath = "assets/UI/save_checkpoint_badge.png";
};

struct SaveSlotInfo
{
    int slotIndex = 0;
    std::string path;
    bool exists = false;
    int schemaVersion = 0;
    std::string checkpointId;
    std::string sceneId;
    std::string reason;
    std::string leadMemberId;
    int leadLevel = 1;
    int coins = 0;
    float playerX = 0.0f;
    float playerY = 0.0f;
    bool hasPlayerPosition = false;
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
    bool SlotExists(int slotIndex) const;
    bool SaveCheckpoint(const std::string& reason) const;
    bool LoadCheckpoint(std::string* outSceneId = nullptr) const;
    bool SaveCheckpointToSlot(int slotIndex, const std::string& reason) const;
    bool LoadCheckpointFromSlot(int slotIndex, std::string* outSceneId = nullptr) const;

    int GetSlotCount() const { return mConfig.slotCount; }
    int GetActiveSlotIndex() const { return mActiveSlotIndex; }
    int FindFirstExistingSlot() const;
    std::string GetSlotPath(int slotIndex) const;
    SaveSlotInfo GetSlotInfo(int slotIndex) const;
    std::vector<SaveSlotInfo> GetSlotInfos() const;

    const SaveCheckpointConfig& GetConfig() const { return mConfig; }

private:
    SaveManager();

    SaveManager(const SaveManager&)            = delete;
    SaveManager& operator=(const SaveManager&) = delete;

    void LoadConfig();
    void SubscribeAutoCheckpoint();
    bool IsValidSlotIndex(int slotIndex) const;

    SaveCheckpointConfig mConfig;
    mutable int mActiveSlotIndex = 0;
    int mVictoryListenerId = -1;
};
