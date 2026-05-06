# Item Icon Rendering — From Colored Placeholders to Real PNGs

## 1. The Problem

Every consumable and equipment item in the game declares an `iconPath` in its
JSON file:

```json
{
  "id": "potion_small",
  "name": "Small Potion",
  "iconPath": "assets/items/potion_small.png",
  ...
}
```

`ItemRegistry::LoadFile` parsed the field, warned when the file was missing,
and then **did nothing else with it**. Every UI surface that needed to render
an item drew a tinted 9-slice quad from `mDialogBox` instead — a colored square
whose hue encoded the item's effect kind:

| Color  | Effect kind         |
|--------|---------------------|
| green  | HealHp / FullHeal   |
| blue   | HealMp              |
| pink   | Revive              |
| orange | RestoreRage         |
| red    | DealDamage          |
| yellow | StatBuff            |
| cyan   | Cleanse             |

This was deliberate — the placeholder shipped a playable game while item art
was still being authored. The trade-off: even after artists delivered the
first six PNGs (`potion_small`, `potion_medium`, `potion_large`, `ether_small`,
`ether_medium`, `elixir`) into `assets/items/`, the menu kept rendering
colored squares because no code path actually loaded the PNG into a texture.

Five render sites were affected:
1. `InventoryStateRender.cpp` — the 4-column consumables grid.
2. `InventoryStateRender.cpp` — the equipment picker rows.
3. `InventoryStateRender.cpp` — the equipment slot list (right-edge swatch).
4. `InventoryStateDetailPanel.cpp` — the 96×96 detail-panel icon.
5. `BattleState.cpp` — the world-space in-battle item submenu.

## 2. Design Decisions

### 2.1 Two new components, single responsibility each

`ItemData` is a pure POD description loaded from JSON — adding a
`ComPtr<ID3D11ShaderResourceView>` to it would couple the data layer to D3D11.
Instead the new code is split along the data/draw boundary:

| Component                                          | Responsibility                                        |
|----------------------------------------------------|-------------------------------------------------------|
| `ItemIconCache` (`src/Battle/ItemIconCache.{h,cpp}`)        | Lazy-load + dedupe textures; **no draw calls**.       |
| `ItemIconRenderer` (`src/Renderer/ItemIconRenderer.{h,cpp}`) | Stretch an externally-supplied SRV onto a destination box; **no item knowledge**. |

This mirrors the existing `ItemRegistry` (data) ↔ `mDialogBox` (renderer)
relationship that was already used for the placeholder.

### 2.2 ItemIconCache — a path-keyed singleton

```cpp
class ItemIconCache {
public:
    static ItemIconCache& Get();
    void Initialize(ID3D11Device*, ID3D11DeviceContext*);    // idempotent
    ID3D11ShaderResourceView* GetIcon(const ItemData* item); // nullptr on failure
    void Shutdown();
private:
    ID3D11Device*        mDevice  = nullptr;
    ID3D11DeviceContext* mContext = nullptr;
    std::unordered_map<std::string,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> mCache;
};
```

Three properties to call out:

- **Keyed by `iconPath`, not item id.** Two `ItemData` entries that share the
  same PNG (e.g. a future "potion_small_red" / "potion_small_blue" reskin)
  produce one VRAM upload instead of two.
- **Tried-and-failed sentinel.** When a file is missing or WIC fails to
  decode, the cache stores an *empty* `ComPtr` in the map. Every subsequent
  `GetIcon` for that path returns `nullptr` without re-touching the
  filesystem — important because the call happens once per icon per frame in
  the inventory grid.
- **Idempotent `Initialize`.** Both `BattleState::OnEnter` and
  `InventoryState::OnEnter` call it. Only the first call wins, so the cache
  uses one consistent device for every cached SRV. Mismatched-device usage
  is undefined behaviour in D3D11.

### 2.3 ItemIconRenderer — agnostic SRV stretcher

```cpp
class ItemIconRenderer {
public:
    bool Initialize(ID3D11Device*, ID3D11DeviceContext*, int screenW, int screenH);
    void Draw(ID3D11DeviceContext* context,
              ID3D11ShaderResourceView* texSRV,
              float destX, float destY, float destW, float destH,
              DirectX::CXMMATRIX transform = DirectX::XMMatrixIdentity(),
              DirectX::XMVECTOR color = DirectX::Colors::White);
    void Shutdown();
};
```

The renderer owns a `SpriteBatch` + `CommonStates` and **nothing else**. The
SRV arrives per call, so a single instance drives every icon in the menu.

The `transform` parameter is the only reason the same renderer works in both
contexts:
- **Inventory** passes `XMMatrixIdentity()` — screen-space pixel coords.
- **Battle item submenu** passes `mBattleRenderer.GetCamera().GetViewMatrix()` —
  world-space, anchored to the active player's slot. SpriteBatch composes
  this with its internal pixel→NDC matrix, matching the SpriteBatch + Camera
  Transform Rule documented in `CLAUDE.md`.

### 2.4 LinearClamp sampling

NineSliceRenderer and PointerRenderer use `PointClamp` because they render
pixel-art UI with hard edges. ItemIconRenderer uses `LinearClamp` instead,
because authored item art (smooth-shaded vials, feathers, gems) renders at
non-integer scale ratios — a 32×32 PNG stretched to a ~62-pixel inventory
cell would stair-step badly under point sampling.

## 3. The Frame-as-Overlay Decision

The first version of the integration replaced the colored placeholder with
the icon outright — `if icon: draw icon  else: draw colored square`. The
real visual win came from inverting that:

```cpp
// 1. Always draw the colored placeholder.  Doubles as the icon's frame.
mDialogBox.Draw(ctx, x, y, size, size, sliceScale,
                transform, IconTintFor(item, alpha));

// 2. If real art exists, overlay it with a 10% inset.
if (auto* srv = ItemIconCache::Get().GetIcon(item)) {
    const float inset = size * 0.10f;
    mIconRenderer.Draw(ctx, srv,
                       x + inset, y + inset,
                       size - inset * 2, size - inset * 2,
                       transform, /*tint=*/ {1, 1, 1, alpha});
}
```

This buys two things:
- **Effect-kind colour reads at a glance even when art is present.** Players
  can scan the menu by colour (green = heal, red = damage) without parsing
  the icon shape every time.
- **Items lacking authored art still show as the original colored square.**
  No regression — only 6 of ~20 items have PNGs today; the other 14 keep the
  exact pre-fix appearance.

The 10% inset means the colored frame stays visible as a 1-2 pixel halo
around the real icon at every menu size (24px picker rows, 96px detail
panel, ~62px grid cells, world-space battle rows).

`InventoryState::IconTintFor` was kept and its doc-comment rewritten to
reflect the dual role. The stale "Goes away when real per-item icon PNGs
land — both render files will then call SpriteBatch::Draw on item->iconSRV
directly" comment was removed.

## 4. Integration Pattern

Every patched call site follows the same shape:

```cpp
// Frame first (always), real PNG on top when available.
mDialogBox.Draw(ctx, fx, fy, fs, fs, sliceScale,
                transform, IconTintFor(item, alpha));
if (auto* srv = ItemIconCache::Get().GetIcon(item)) {
    const float inset = fs * 0.10f;
    mIconRenderer.Draw(ctx, srv,
                       fx + inset, fy + inset,
                       fs - inset * 2.0f, fs - inset * 2.0f,
                       transform,
                       DirectX::XMVectorSet(1, 1, 1, alpha));
}
```

Locations in the codebase:

| Site                                               | File                                                  | Transform        |
|----------------------------------------------------|-------------------------------------------------------|------------------|
| 4-col consumables grid                             | `InventoryStateRender.cpp` `RenderItemsTab`           | Identity         |
| Equipment picker rows                              | `InventoryStateRender.cpp` `RenderEquipmentTab`       | Identity         |
| Equipment slot-list right-edge swatch              | `InventoryStateRender.cpp` `RenderEquipmentTab`       | Identity         |
| Detail panel 96×96 icon                            | `InventoryStateDetailPanel.cpp`                       | Identity         |
| In-battle item submenu (per row)                   | `BattleState.cpp` ITEM_SELECT block                   | `cameraMatrix`   |

State-side wiring lives where the other UI renderers are wired:

- `InventoryState` declares `ItemIconRenderer mIconRenderer;`. Initialised
  in `OnEnter`, shut down in `OnExit`. `ItemIconCache::Get().Initialize`
  fires alongside.
- `BattleState` declares `ItemIconRenderer mItemIconRenderer;` with the
  same lifecycle. The cache is the same singleton, so its initialise call
  is a no-op the second time.

`ItemIconCache::Shutdown` is intentionally **not** called in either OnExit —
the cache survives across state transitions. Its ~6 small PNGs cost
negligible VRAM and re-loading them on every battle entry would churn WIC.
COM cleanup runs at static destruction.

## 5. Adding Art for a New Item

Zero C++ changes:
1. Drop the PNG at the path declared in the item's JSON `iconPath`.
2. Run the game.

The cache lazy-loads on first request. A missing file logs `[ItemIconCache]`
once via `ItemRegistry`'s existing missing-icon warning, then renders the
colored placeholder with no further filesystem access.

Format suggestion: 32×32 to 64×64 PNG with transparency. Anything bilinear
filtering can stretch cleanly.

## 6. Common Mistakes Avoided

1. **Storing a `ComPtr<ID3D11ShaderResourceView>` directly in `ItemData`.**
   That would have leaked D3D11 into the data layer and broken the "ItemData
   is JSON" invariant. The path-keyed cache keeps `ItemData` pure POD.
2. **Hammering the filesystem on missing files.** The "tried-and-failed"
   sentinel — an empty `ComPtr` in the cache map — guarantees `fs::exists`
   runs at most once per path per program run.
3. **Drawing the icon in place of the placeholder.** That made the menu
   visually inconsistent across items with art vs without. Drawing the
   placeholder *under* the icon keeps the colour-coded scan-by-effect
   behaviour intact for every item.
4. **Re-creating SpriteBatch per draw.** The renderer owns one batch for
   the lifetime of the State; only `Begin`/`End` happens per icon. Per-icon
   batch creation would have been an allocation per frame per visible cell.
5. **Per-state cache instances.** Sharing one singleton across BattleState
   and InventoryState means the player can scroll the inventory, enter a
   battle, and the same SRVs are reused — no second WIC upload.
