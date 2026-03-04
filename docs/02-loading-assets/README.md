# 02 — Loading Assets

This chapter covers everything that sits between a file on disk and a pixel on
screen: the `SpriteSheet` data contract, the minimal JSON parser, and how
`WorldSpriteRenderer` uploads a texture to the GPU exactly once and reuses it
every frame.

---

## Files in this chapter

| Document | What it covers |
|---|---|
| [sprite-sheet.md](sprite-sheet.md) | `SpriteSheet` / `AnimationClip` structs — the data contract |
| [json-loader.md](json-loader.md) | `JsonLoader::LoadSpriteSheet()` — parsing animation JSON |
| [world-sprite-renderer.md](world-sprite-renderer.md) | `WorldSpriteRenderer` — GPU upload, SpriteBatch, clip playback |

---

## The loading pipeline

```
assets/animations/verso.json          assets/animations/verso.png
         │                                       │
         ▼                                       ▼
 JsonLoader::LoadSpriteSheet()       (still on disk until needed)
         │
         ▼
     SpriteSheet struct
  (AnimationClip list, frame dims)
         │                                       │
         └──────────── both passed to ───────────┘
                                │
                                ▼
              WorldSpriteRenderer::Initialize()
                   │                   │
                   ▼                   ▼
          GPU texture upload     SpriteBatch + D3D states
          (once, in VRAM)        (once, never re-created)
                                │
                        called every frame
                                │
                                ▼
           WorldSpriteRenderer::Update(dt)  ← advance frame timer
           WorldSpriteRenderer::Draw(ctx, camera, worldX, worldY)
```

The JSON file is read once during `PlayState::OnEnter()`.  After that, the
`SpriteSheet` struct lives entirely in CPU RAM.  The PNG is decompressed and
uploaded to VRAM once; every `Draw()` call reads from that VRAM texture — no
disk access after initialization.

---

## Where this fits in the architecture

```
PlayState::OnEnter()
  ├── JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet)
  │       └── returns SpriteSheet populated with AnimationClips
  │
  └── SceneGraph::Spawn<ControllableCharacter>(device, ctx, L"verso.png", sheet, ...)
            └── ControllableCharacter constructor
                    └── WorldSpriteRenderer::Initialize(device, ctx, texPath, sheet)
                              ├── CreateWICTextureFromFile()   ← PNG → VRAM
                              ├── new SpriteBatch(context)     ← draw batching
                              └── CreateDepthStencilState()    ← depth OFF
                                  CreateBlendState()           ← alpha blend ON
```
