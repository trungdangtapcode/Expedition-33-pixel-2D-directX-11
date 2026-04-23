// ============================================================
// File: BattleEvents.h
// Responsibility: Define event payload structs for decoupling
//   logic (IAction/BattleManager) from presentation (BattleRenderer).
// ============================================================
#pragma once
#include "IBattler.h"
#include "CombatantAnim.h"
#include "BattleCameraController.h"

struct PlayAnimPayload
{
    IBattler*     target;
    CombatantAnim anim;
};

struct IsAnimDonePayload
{
    IBattler* target;
    bool      isDone; // Out parameter
};

struct GetAnimProgressPayload
{
    IBattler* target;
    float     progress; // Out parameter
};

struct MoveOffsetPayload
{
    IBattler* target;
    float     offsetX;
    float     offsetY;
};

struct GetWorldPosPayload
{
    IBattler* target;
    float     x; // Out parameter
    float     y; // Out parameter
};

enum class QTEResult {
    None,
    Pending,
    Miss,
    Good,
    Perfect
};

#define MAX_QTE_NODES 8

struct QTEStatePayload
{
    bool isActive = false;
    float progressRatios[MAX_QTE_NODES] = {0}; // 0.0 to 1.0 per node
    QTEResult results[MAX_QTE_NODES];
    QTEResult result = QTEResult::None; // Legacy callback compat
    IBattler* target = nullptr; // the character currently performing the attack
    int activeIndex = 0;
    int totalCount = 1;
    float fadeInRatio = 0.15f; 
    float fadeOutDuration = 0.20f;
};

struct DamageTakenPayload
{
    IBattler* target = nullptr;
    int damage = 0;
    bool isCrit = false;
    bool isPerfectQte = false;
};

// Must forward declare enum wrapper since we can't cleanly drag camera into events
enum class BattleCameraPhase;

struct CameraPhasePayload
{
    BattleCameraPhase phase;
    IBattler* targetToFollow = nullptr;
    float dynamicZoom = 1.4f;
};
