# SpriteSheet & AnimationClip

**Files:** `src/Renderer/SpriteSheet.h`

---

## Purpose

`SpriteSheet` and `AnimationClip` are **pure data structs** — no GPU resources,
no behavior, no methods.  They are the bridge between the JSON file on disk and
the renderer that issues draw calls.

Everything a renderer needs to slice one frame out of a sprite atlas and display
it correctly lives in these two structs.

---

## The sprite atlas model

A sprite atlas is a single PNG file that contains every animation frame for one
character, laid out in a grid:

```
verso.png  (1792 × 128 pixels)
┌──────────────────────────────────────────── ... ────────┐
│ frame 0 │ frame 1 │ frame 2 │ ... │ frame 13 │          │
│ 128×128 │ 128×128 │ 128×128 │     │  128×128 │          │
└──────────────────────────────────────────── ... ────────┘
                                       ↑ 14 frames of "idle"
```

`SpriteSheet` records the atlas dimensions and frame size.
`AnimationClip` records which frames belong to a named animation, how fast to
play them, and where the character's origin point is within each frame.

---

## Struct layout

### `AnimationClip`

```cpp
struct AnimationClip {
    std::string name;      // "idle", "walk", "attack", etc.
    int         numFrames; // how many frames this clip uses
    float       frameRate; // playback speed — frames per second
    bool        loop;      // restart from frame 0 after the last frame

    int         pivotX;   // horizontal origin within one frame (pixels)
    int         pivotY;   // vertical origin within one frame (pixels)

    SpriteAlign align;    // screen anchor hint (BottomCenter for characters)
};
```

### `SpriteSheet`

```cpp
struct SpriteSheet {
    std::string name;         // "verso"
    int         width;        // atlas total width  (e.g. 1792)
    int         height;       // atlas total height (e.g. 128)
    int         frameWidth;   // one frame width    (e.g. 128)
    int         frameHeight;  // one frame height   (e.g. 128)

    std::vector<AnimationClip> animations;

    // Computed helper: how many frames fit on one row of the atlas.
    int framesPerRow() const { return width / frameWidth; }
};
```

---

## UV slicing — how a frame index becomes a source rectangle

`WorldSpriteRenderer::Draw()` computes the `RECT` to pass to `SpriteBatch::Draw()`:

```cpp
const int framesPerRow = mSheet.framesPerRow();
const int col = mFrameIndex % framesPerRow;  // column in the atlas grid
const int row = mFrameIndex / framesPerRow;  // row   in the atlas grid

RECT srcRect;
srcRect.left   = col * mSheet.frameWidth;
srcRect.top    = row * mSheet.frameHeight;
srcRect.right  = srcRect.left + mSheet.frameWidth;
srcRect.bottom = srcRect.top  + mSheet.frameHeight;
```

For the Verso "idle" clip at `frameIndex=3` on a 1792×128 atlas with 128×128
frames:

```
framesPerRow = 1792 / 128 = 14
col = 3 % 14 = 3
row = 3 / 14 = 0

srcRect = { left=384, top=0, right=512, bottom=128 }
```

---

## The pivot — what `pivotX / pivotY` mean

The pivot is the point within the source frame that lands exactly on the world
coordinate passed to `Draw(worldX, worldY)`.

For a 128×128 frame with `pivot=[64, 128]` (bottom-center):

```
┌─────────────────────────┐  ← frame top    (y=0)
│                         │
│     128 × 128 frame     │
│                         │
│            ×            │  ← pivot (64, 128) = center-bottom
└─────────────────────────┘  ← frame bottom (y=128)
             ↑
       worldY lands here
       (character's feet)
```

`SpriteBatch::Draw()` accepts the pivot as an `XMFLOAT2 origin` in
**source-rect-local pixels**.  `WorldSpriteRenderer` passes it directly:

```cpp
const XMFLOAT2 origin{ (float)mActiveClip->pivotX,
                        (float)mActiveClip->pivotY };
mSpriteBatch->Draw(mTextureSRV.Get(), pos, &srcRect, Colors::White,
                   0.0f, origin, scale);
```

---

## `SpriteAlign` — screen anchoring for UI renderers

`SpriteAlign` is an enum that tells `UIRenderer` where on the screen to place
the sprite.  It is defined as a mirror of the JSON `"align"` string field:

```
"bottom-center"  →  SpriteAlign::BottomCenter   (default for standing characters)
"top-left"       →  SpriteAlign::TopLeft
"middle-center"  →  SpriteAlign::MiddleCenter
... etc.
```

`WorldSpriteRenderer` ignores `SpriteAlign` — world-space sprites are positioned
by `worldX / worldY` coordinates, not screen anchors.

---

## JSON schema this struct mirrors

```json
{
  "sprite_name": "verso",
  "character":   "verso",
  "width":       1792,
  "height":      128,
  "frame_width": 128,
  "frame_height":128,
  "animations": [
    {
      "name":        "idle",
      "num_frames":  14,
      "frame_rate":  8,
      "loop":        true,
      "pivot":       [64, 128],
      "align":       "bottom-center"
    }
  ]
}
```

---

## Common mistakes

| Mistake | Consequence | Fix |
|---|---|---|
| Hardcoding `srcRect` pixel values in `.cpp` | Breaks when atlas changes size | Always compute from `SpriteSheet` fields |
| Passing `frameIndex` past `numFrames` | Reads garbage pixels from wrong frame | `WorldSpriteRenderer::Update()` wraps on `loop=true`, stops on `loop=false` |
| Confusing `pivotX/Y` with screen position | Sprite appears at wrong anchor | Pivot is **frame-local** pixels, not screen pixels |
| Using `SpriteAlign` in world-space renderer | Has no effect | `SpriteAlign` is for `UIRenderer` only |
