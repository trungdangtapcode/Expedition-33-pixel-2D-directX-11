// ============================================================
// File: ObjectiveTrackerRenderer.h
// Responsibility: Render compact overworld objective text.
//
// Owns:
//   ObjectiveTrackerConfig - screen-space layout and color tuning only.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter()
//   Destroyed in -> OverworldState::OnExit() with OverworldState.
//
// Important:
//   - This renderer does not own GPU resources.
//   - Text is measured through BattleTextRenderer so wrapping matches the
//     active localization font.
//   - Layout comes from JSON to keep long localized objectives out of code.
// ============================================================
#pragma once

#include "BattleTextRenderer.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <string>
#include <vector>

struct ObjectiveTrackerConfig
{
    float x = 24.0f;
    float y = 20.0f;
    float maxWidth = 940.0f;
    float rightReserveWidth = 240.0f;
    float minWidth = 360.0f;
    float titleScale = 1.0f;
    float bodyScale = 0.88f;
    float hintScale = 0.86f;
    float titleLineHeight = 25.0f;
    float bodyLineHeight = 23.0f;
    float titleBodyGap = 2.0f;
    float bodyHintGap = 2.0f;
    float shadowOffset = 2.0f;
    int bodyMaxLines = 2;

    float titleR = 1.0f;
    float titleG = 1.0f;
    float titleB = 1.0f;
    float titleA = 1.0f;

    float bodyR = 0.93f;
    float bodyG = 0.86f;
    float bodyB = 0.58f;
    float bodyA = 1.0f;

    float hintR = 0.98f;
    float hintG = 0.78f;
    float hintB = 0.32f;
    float hintA = 1.0f;

    float shadowA = 0.90f;
};

class ObjectiveTrackerRenderer
{
public:
    bool Initialize(const std::string& configPath);

    void Render(ID3D11DeviceContext* context,
                BattleTextRenderer& textRenderer,
                int screenWidth,
                const std::string& area,
                const std::string& objectiveBody,
                const std::string& objectiveHint) const;

private:
    bool LoadConfig(const std::string& configPath);
    std::vector<std::string> WrapText(BattleTextRenderer& textRenderer,
                                      const std::string& text,
                                      float maxWidth,
                                      float scale,
                                      int maxLines) const;
    std::string FitWithEllipsis(BattleTextRenderer& textRenderer,
                                const std::string& line,
                                float maxWidth,
                                float scale) const;
    void DrawLine(BattleTextRenderer& textRenderer,
                  const std::string& text,
                  float x,
                  float y,
                  DirectX::XMVECTORF32 color,
                  float scale) const;
    DirectX::XMVECTORF32 MakeColor(float r,
                                   float g,
                                   float b,
                                   float a) const;

    ObjectiveTrackerConfig mConfig;
};
