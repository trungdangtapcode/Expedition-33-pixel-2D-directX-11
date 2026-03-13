# Smart Architecture: SceneGraph for Battle Rendering

## 1. Vấn đề (The Problem)
Trong thiết kế ban đầu, `BattleRenderer` tự động quản lý các mảng `WorldSpriteRenderer` thủ công cho `Player` và `Enemy`:
```cpp
WorldSpriteRenderer mPlayerRenderers[3];
WorldSpriteRenderer mEnemyRenderers[3];
```
Khi render, nó dùng các vòng lặp `for` lồng nhau để vẽ từng phe. Tuy nhiên, khi muốn áp dụng **Y-Sorting (Painter's Algorithm)** giúp nhân vật đứng sau (Y nhỏ hơn) được vẽ trước để không đè lên nhân vật đứng trước, nếu code lại từ đầu trong `BattleRenderer`, chúng ta sẽ vi phạm nguyên tắc DRY (Don't Repeat Yourself) vì cơ chế này đã tồn tại sẵn trong `SceneGraph` dành cho `OverworldState`.

## 2. Giải pháp: Hợp nhất vào SceneGraph (The Solution)

Để tái sử dụng kiến trúc như một "Game Engine" thực thụ, chúng ta bọc (wrap) `WorldSpriteRenderer` vào trong một class kế thừa từ interface `IGameObject`.

### Bước 1: Tạo `BattleCombatantSprite`
Class mới `BattleCombatantSprite` implements `IGameObject`, đóng vai trò là một proxy entity chứa `WorldSpriteRenderer`. Điểm mấu chốt là nó override phương thức `GetSortY()`:
```cpp
class BattleCombatantSprite : public IGameObject {
    // ...
    int GetLayer() const override { return 50; } 
    float GetSortY() const override { return mWorldY; } // Y-Sorting quyết định ở đây
};
```

### Bước 2: `BattleRenderer` quản lý `SceneGraph`
Thay vì chứa list renderers trực tiếp, `BattleRenderer` giờ đây sở hữu `SceneGraph mScene`.
Khi spawn các characters, chúng ta đẩy vào `mScene`:
```cpp
mEnemySprites[i] = mScene.Spawn<BattleCombatantSprite>(
    device, context, info.texturePath, sheet, 
    &mCameraCtrl.GetCamera(), 
    mEnemyWorldX[i], mEnemyWorldY[i], 2.0f, true
);
```

### Bước 3: Đơn giản hóa quá trình Render
Vòng lặp Update và Render trong `BattleRenderer` được tối giản đến mức tối đa nhờ việc "ủy quyền" lại cho `SceneGraph`:
```cpp
void BattleRenderer::Update(float dt) {
    mCameraCtrl.Update(dt);
    mScene.Update(dt); // Quản lý logic toàn bộ entities
}

void BattleRenderer::Render(ID3D11DeviceContext* context) {
    mScene.Render(context); // Tự động Sort theo Y và vẽ hệ thống
}
```

## 3. Tại sao đây là thiết kế "Thông Minh"? (Benefits)
1. **Kiến trúc đồng nhất (Consistency):** Từ thế giới Overworld đến hệ thống Battle đều chia sẻ chung một pipeline render là `SceneGraph`.
2. **Khả năng mở rộng vô hạn (Scalability):** Trước đây nếu muốn thêm một Effect (Particle, Damage Numbers) vào trận đấu, bạn phải tạo 1 list loop mới. Bây giờ, chỉ việc gọi `mScene.Spawn<VFXSprite>(...)`, SceneGraph sẽ lo việc đè hình, sort và dọn dẹp bộ nhớ tự động.
3. **Clean Code:** Loại bỏ sạch sẽ các mảng tĩnh (static arrays) lằng nhằng và những điều kiện kiểm tra lặp rườm rà. Code logic đồ họa tách biệt hoàn toàn khỏi logic trận đấu.
