// ============================================================
// File: UIEffectState.h
// Responsibility: Encapsulate common UI animations like scaling 
//                 and screen-shake to prevent code duplication 
//                 across different UI renderers.
// ============================================================
#pragma once
#include <cstdlib>
#include <cmath>

class UIEffectState
{
public:
    void Update(float dt)
    {
        // Scale lerping
        mCurrentScale += (mTargetScale - mCurrentScale) * mScaleLerpSpeed * dt;

        // Shake logic
        if (mShakeTimer > 0.0f)
        {
            mShakeTimer -= dt;
            if (mShakeTimer <= 0.0f)
            {
                mShakeTimer = 0.0f;
                mOffsetX = 0.0f;
                mOffsetY = 0.0f;
            }
            else
            {
                // Randomly perturb offset within [-intensity, intensity]
                float rx = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f;
                float ry = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f;
                
                // Fade out intensity as timer approaches 0
                float currentIntensity = mShakeIntensity * (mShakeTimer / mShakeDuration);

                mOffsetX = rx * currentIntensity;
                mOffsetY = ry * currentIntensity;
            }
        }
    }

    void SetTargetScale(float scale) { mTargetScale = scale; }
    float GetScale() const { return mCurrentScale; }
    
    void TriggerShake(float duration = 0.3f, float intensity = 8.0f)
    {
        mShakeTimer = duration;
        mShakeDuration = duration;
        mShakeIntensity = intensity;
    }

    float GetOffsetX() const { return mOffsetX; }
    float GetOffsetY() const { return mOffsetY; }

private:
    float mTargetScale = 1.0f;
    float mCurrentScale = 1.0f;
    float mScaleLerpSpeed = 15.0f;

    float mShakeTimer = 0.0f;
    float mShakeDuration = 0.3f;
    float mShakeIntensity = 0.0f;
    float mOffsetX = 0.0f;
    float mOffsetY = 0.0f;
};
