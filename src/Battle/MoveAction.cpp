#define NOMINMAX
// ============================================================
// File: MoveAction.cpp
// ============================================================
#include "MoveAction.h"
#include "BattleEvents.h"
#include "../Events/EventManager.h"
#include <DirectXMath.h>

MoveAction::MoveAction(IBattler* mover, IBattler* target, TargetType type, float duration, float meleeOffset, CombatantAnim movingAnim, CombatantAnim stopAnim)
    : mMover(mover), mTarget(target), mType(type), mDuration(duration), mMeleeOffset(meleeOffset), mMovingAnim(movingAnim), mStopAnim(stopAnim)
{}

bool MoveAction::Execute(float dt)
{
    if (!mHasStarted)
    {
        // Get mover's starting world pos
        GetWorldPosPayload pMover = { mMover, 0.f, 0.f };
        EventData eMover; eMover.payload = &pMover;
        EventManager::Get().Broadcast("battler_get_world_pos", eMover);
        
        mStartX = 0.f; // The draw offset starts at 0 relative to where they are
        mStartY = 0.f;

        if (mType == TargetType::MeleeRange && mTarget != nullptr)
        {
            // Get target's world pos
            GetWorldPosPayload pTarget = { mTarget, 0.f, 0.f };
            EventData eTarget; eTarget.payload = &pTarget;
            EventManager::Get().Broadcast("battler_get_world_pos", eTarget);

            // Calculate destination offset
            // We want to be slightly in front of the target.
            // If target is right of us, stand to their left.
            float dirX = (pTarget.x > pMover.x) ? -mMeleeOffset : mMeleeOffset;
            
            float targetWorldX = pTarget.x + dirX;
            float targetWorldY = pTarget.y;

            // Offset is difference in world bounds
            mTargetOffsetX = targetWorldX - pMover.x;
            mTargetOffsetY = targetWorldY - pMover.y;
        }
        else // Origin
        {
            // If we are currently offset (e.g. from previous move), we need to lerp back to 0.
            // But wait, the Action only knows current timer.
            // We assume the user has a current offset. How to get current offset?
            // For now, assume returning from MeleeRange -> lerping to (0,0).
            // But we don't know the exact starting offset!
            // Let's query current offset:
            MoveOffsetPayload pOffset = { mMover, 0.f, 0.f };
            EventData eOffset; eOffset.payload = &pOffset;
            EventManager::Get().Broadcast("battler_get_offset", eOffset);

            mStartX = pOffset.offsetX;
            mStartY = pOffset.offsetY;

            mTargetOffsetX = 0.f;
            mTargetOffsetY = 0.f;
        }

        mHasStarted = true;

        if (mMovingAnim != CombatantAnim::Idle) {
            PlayAnimPayload pAnim = { mMover, mMovingAnim };
            EventData eAnim; eAnim.payload = &pAnim;
            EventManager::Get().Broadcast("battler_play_anim", eAnim);
        }
    }

    mTimer += dt;
    float t = mDuration > 0.f ? (mTimer / mDuration) : 1.f;
    if (t > 1.0f) t = 1.0f;

    // Smoothstep easing: 3t^2 - 2t^3
    float easeT = t * t * (3.0f - 2.0f * t);

    float currentX = mStartX + (mTargetOffsetX - mStartX) * easeT;
    float currentY = mStartY + (mTargetOffsetY - mStartY) * easeT;

    MoveOffsetPayload pMove = { mMover, currentX, currentY };
    EventData eMove; eMove.payload = &pMove;
    EventManager::Get().Broadcast("battler_set_offset", eMove);

    if (t >= 1.0f) {
        if (mStopAnim != CombatantAnim::Idle) {
            PlayAnimPayload pStop = { mMover, mStopAnim };
            EventData eStop; eStop.payload = &pStop;
            EventManager::Get().Broadcast("battler_play_anim", eStop);
        }
        return true;
    }
    return false;
}
