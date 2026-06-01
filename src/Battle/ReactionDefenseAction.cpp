// ============================================================
// File: ReactionDefenseAction.cpp
// Responsibility: Implement data-driven defensive timing windows.
// ============================================================
#define NOMINMAX
#include "ReactionDefenseAction.h"
#include "BattleContext.h"
#include "BattleResourceRules.h"
#include "DefaultDamageCalculator.h"
#include "../Core/InputManager.h"
#include "../Events/EventManager.h"
#include <algorithm>

namespace
{
    int KeyFromName(const std::string& keyName)
    {
        if (keyName == "space") return 0x20;
        if (keyName == "enter") return 0x0D;
        if (keyName == "f") return 0x46;
        if (keyName == "e") return 0x45;
        return VK_SPACE;
    }

    float Clamp01(float value)
    {
        return (std::max)(0.0f, (std::min)(1.0f, value));
    }
}

ReactionDefenseAction::ReactionDefenseAction(
    const DamageRequest& request,
    const JsonLoader::ReactionWindowData& window,
    CombatantAnim animType,
    const BattleContext* context)
    : mRequest(request)
    , mWindow(window)
    , mAnimType(animType)
    , mContext(context)
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Drive one incoming attack animation, expose a timing prompt, and apply
//   scaled damage after the configured reaction window resolves.
// Why:
//   Enemy defensive timing should be data-authored per attack while still
//   using the existing animation and result tracking event flow.
// Parameters:
//   dt - Unused directly; animation progress comes from BattleRenderer.
// ------------------------------------------------------------
bool ReactionDefenseAction::Execute(float /*dt*/)
{
    if (!mHasStarted)
    {
        PlayAnimPayload animPayload{ mRequest.attacker, mAnimType };
        EventData animEvent;
        animEvent.payload = &animPayload;
        EventManager::Get().Broadcast("battler_play_anim", animEvent);
        mHasStarted = true;
    }

    GetAnimProgressPayload progressPayload{ mRequest.attacker, 0.0f };
    EventData progressEvent;
    progressEvent.payload = &progressPayload;
    EventManager::Get().Broadcast("battler_get_anim_progress", progressEvent);

    const float animationProgress = progressPayload.progress;
    const float promptProgress = CurrentPromptProgress(animationProgress);

    if (!mPromptResolved &&
        animationProgress >= mWindow.startMoment &&
        animationProgress < mWindow.damageMoment)
    {
        if (InputManager::Get().IsKeyPressed(ResolveInputKey()))
        {
            const QTEResult result = promptProgress >= mWindow.perfectThreshold
                ? QTEResult::Perfect
                : (promptProgress >= mWindow.goodThreshold ? QTEResult::Good : QTEResult::Miss);
            ResolvePrompt(result);
        }
        else
        {
            PublishPrompt(true, QTEResult::None, promptProgress);
        }
    }

    if (!mPromptResolved && animationProgress >= mWindow.damageMoment)
    {
        ResolvePrompt(QTEResult::Miss);
    }

    if (!mDamageApplied && animationProgress >= mWindow.damageMoment)
    {
        ApplyDamage();
        mDamageApplied = true;
    }

    IsAnimDonePayload donePayload{ mRequest.attacker, false };
    EventData doneEvent;
    doneEvent.payload = &donePayload;
    EventManager::Get().Broadcast("battler_is_anim_done", doneEvent);
    if (donePayload.isDone)
    {
        PublishPrompt(false, QTEResult::None, 1.0f);
    }
    return donePayload.isDone;
}

int ReactionDefenseAction::ResolveInputKey() const
{
    return KeyFromName(mWindow.inputKey);
}

float ReactionDefenseAction::CurrentPromptProgress(float animationProgress) const
{
    const float windowLength = (std::max)(0.001f, mWindow.damageMoment - mWindow.startMoment);
    return Clamp01((animationProgress - mWindow.startMoment) / windowLength);
}

void ReactionDefenseAction::PublishPrompt(bool isActive, QTEResult result, float progressRatio) const
{
    QTEStatePayload payload{};
    payload.isActive = isActive;
    payload.progressRatios[0] = Clamp01(progressRatio);
    payload.results[0] = result;
    payload.result = result;
    payload.target = mRequest.defender;
    payload.activeIndex = 0;
    payload.totalCount = 1;
    payload.fadeInRatio = mWindow.fadeInRatio;
    payload.fadeOutDuration = mWindow.fadeOutDuration;

    EventData eventData;
    eventData.payload = &payload;
    EventManager::Get().Broadcast("battler_qte_update", eventData);
}

void ReactionDefenseAction::ResolvePrompt(QTEResult result)
{
    if (mPromptResolved) return;
    mPromptResolved = true;
    mResult = result;
    PublishPrompt(true, result, 1.0f);
}

void ReactionDefenseAction::ApplyDamage()
{
    if (!mRequest.defender) return;

    switch (mResult)
    {
    case QTEResult::Perfect:
        mRequest.qteMultiplier = mWindow.perfectDamageMultiplier;
        break;
    case QTEResult::Good:
        mRequest.qteMultiplier = mWindow.goodDamageMultiplier;
        break;
    case QTEResult::Miss:
    default:
        mRequest.qteMultiplier = mWindow.missDamageMultiplier;
        break;
    }

    const BattleContext fallbackContext;
    const BattleContext& damageContext = mContext ? *mContext : fallbackContext;
    DefaultDamageCalculator calculator;
    DamageResult result = calculator.Calculate(mRequest, damageContext);
    if (mResult == QTEResult::Perfect && mWindow.perfectDamageMultiplier <= 0.0f)
    {
        result.effectiveDamage = 0;
        result.rawDamage = 0;
    }

    const bool defenderWasAlive = mRequest.defender->IsAlive();
    if (result.effectiveDamage > 0)
    {
        mRequest.defender->TakeDamage(result, mRequest.attacker);
    }

    const bool defenderWasKilled = defenderWasAlive && !mRequest.defender->IsAlive();
    if (mRequest.grantsRage && result.effectiveDamage > 0)
    {
        BattleResourceRules::Get().EnsureLoaded();
        BattleResourceRules::Get().GrantDamageRage(
            mRequest.attacker,
            mRequest.defender,
            result,
            defenderWasKilled);
    }

    BattleDodgeResultPayload dodgePayload{};
    dodgePayload.attacker = mRequest.attacker;
    dodgePayload.defender = mRequest.defender;
    dodgePayload.completed = true;
    dodgePayload.hitsTaken = result.effectiveDamage > 0 ? 1 : 0;

    EventData dodgeEvent;
    dodgeEvent.payload = &dodgePayload;
    EventManager::Get().Broadcast("battle_dodge_result", dodgeEvent);
}
