// ============================================================
// File: SpriteSheet.h
// Responsibility: Pure data — describes a sprite sheet and its animations.
//
// This is a plain data struct with no GPU resources and no behavior.
// It is populated by JsonLoader::LoadSpriteSheet() and consumed by
// SpriteRenderer to slice UVs and advance frames.
//
// Data-driven design:
//   All values come from JSON (e.g. data/animations/verso.json).
//   Source code never contains hardcoded frame counts, rates, or sizes.
//
// JSON schema this struct mirrors:
//   {
//     "sprite_name": "verso",
//     "character":   "verso",
//     "width": 1792, "height": 128,
//     "frame_width": 128, "frame_height": 128,
//     "animations": [
//       {
//         "name": "idle",
//         "num_frames": 14,
//         "frame_rate": 8,          <- frames per second
//         "loop": true,
//         "pivot": [64, 128],       <- pivot point within one frame (pixels)
//         "align": "bottom-center"  <- screen anchor; drives Draw() position
//       }
//     ]
//   }
// ============================================================
#pragma once
#include <string>
#include <vector>

// ------------------------------------------------------------
// SpriteAlign — where on the screen the sprite is anchored.
//
// SpriteRenderer::Draw() reads this from the active clip and
// computes the final screen position automatically.
// The caller never passes hardcoded pixel coordinates.
//
// Supported values (mirror the JSON "align" strings):
//   "top-left"      "top-center"      "top-right"
//   "middle-left"   "middle-center"   "middle-right"
//   "bottom-left"   "bottom-center"   "bottom-right"
// ------------------------------------------------------------
enum class SpriteAlign {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,   // default for characters standing on the ground
    BottomRight,
    Unknown         // JSON value not recognized — falls back to BottomCenter
};

// ------------------------------------------------------------
// AnimationClip — one named animation within the sprite sheet.
// Frame indices are computed as:
//   col = frameIndex % framesPerRow
//   row = frameIndex / framesPerRow      (for multi-row sheets)
// For single-row sheets (height == frame_height), row is always 0.
// ------------------------------------------------------------
struct AnimationClip {
    std::string name;       // "idle", "walk", "attack", etc.
    int         numFrames;  // total frames in this clip
    float       frameRate;  // playback speed in frames per second
    bool        loop;       // whether to restart after the last frame

    int         pivotX;     // pivot x within one frame in pixels (local space)
    int         pivotY;     // pivot y within one frame in pixels (local space)

    // Screen-anchor for this clip.
    // SpriteRenderer::Draw() maps this to a pixel position on the screen.
    // No caller code ever computes or hardcodes a pixel position.
    SpriteAlign align = SpriteAlign::BottomCenter;
};

// ------------------------------------------------------------
// SpriteSheet — describes the full texture atlas and all clips.
// ------------------------------------------------------------
struct SpriteSheet {
    std::string spriteName;     // matches the JSON "sprite_name" field
    std::string character;      // which character this sheet belongs to

    int sheetWidth;             // full texture width  in pixels
    int sheetHeight;            // full texture height in pixels
    int frameWidth;             // width  of a single frame in pixels
    int frameHeight;            // height of a single frame in pixels

    // Derived: how many frames fit in one row.
    // Used by SpriteRenderer to convert a linear frameIndex to (col, row).
    int framesPerRow() const {
        return (frameWidth > 0) ? (sheetWidth / frameWidth) : 1;
    }

    std::vector<AnimationClip> animations;

    // ------------------------------------------------------------
    // Helper: look up a clip by name.
    // Returns nullptr if the name is not found.
    // ------------------------------------------------------------
    const AnimationClip* FindClip(const std::string& clipName) const {
        for (const auto& clip : animations) {
            if (clip.name == clipName) return &clip;
        }
        return nullptr;
    }
};
