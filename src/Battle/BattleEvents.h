// ============================================================
// File: BattleEvents.h
// Responsibility: Define event payload structs for decoupling
//   logic (IAction/BattleManager) from presentation (BattleRenderer).
// ============================================================
#pragma once
#include "IBattler.h"
#include "CombatantAnim.h"

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
