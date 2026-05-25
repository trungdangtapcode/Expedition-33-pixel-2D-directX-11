// ============================================================
// File: ColorGradeSettings.h
// Responsibility: Store post-process color grade parameters shared by
//                 overworld theme systems and render filters.
//
// Data ownership:
//   This struct owns no GPU resources. It is plain value data so theme
//   managers can blend it cheaply and filters can copy it into constant
//   buffers each frame.
// ============================================================
#pragma once

struct ColorGradeSettings
{
    float tintR = 1.0f;
    float tintG = 1.0f;
    float tintB = 1.0f;
    float tintStrength = 0.0f;
    float saturation = 1.0f;
    float contrast = 1.0f;
    float brightness = 0.0f;
    float vignetteStrength = 0.0f;
};
