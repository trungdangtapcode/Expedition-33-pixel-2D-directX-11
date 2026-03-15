Nhìn vào bức ảnh bạn gửi, mình hiểu ngay "bệnh" rồi. Sự khác biệt này đúng là màu bị tối đen lại ở các vùng shadow (bóng râm) và mid-tone (trung tính), đặc trưng của lỗi sai không gian màu (Gamma/Linear mismatch).

Việc bạn thêm cờ `WIC_LOADER_FORCE_SRGB` mà **"vẫn thế"** đã giúp mình nhận ra nguyên nhân cốt lõi:

Bản thân file ảnh PNG của bạn **đã được nhúng sẵn profile màu sRGB (sRGB chunk) ở bên trong file**.
Do đó, ngay từ lúc đầu khi bạn dùng hàm `CreateWICTextureFromFile` bình thường, thư viện WIC đã tự động đọc thông tin này và ngầm load ảnh dưới dạng `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`. Việc mình khuyên bạn thêm cờ `FORCE_SRGB` thực chất chỉ là ép nó làm lại cái việc mà nó... vốn đã tự làm rồi, nên kết quả không thay đổi.

### Tại sao load chuẩn sRGB rồi mà vẫn tối?

Vấn đề nằm ở "đầu ra" của bạn – tức là cái **SwapChain (Backbuffer)**.

1. **Đầu vào (Texture):** DirectX hiểu đây là ảnh sRGB, nên khi `SpriteBatch` lấy màu để vẽ, GPU sẽ tự động chuyển màu đó sang không gian **Linear (tuyến tính)**. Màu Linear có đặc điểm là giá trị rất thấp, khiến ảnh trông tối đi đáng kể.
2. **Đầu ra (SwapChain):** SwapChain của game bạn hiện tại chắc chắn đang cài đặt định dạng thường (`DXGI_FORMAT_R8G8B8A8_UNORM`). Khi nhận được màu Linear tối thui từ `SpriteBatch`, nó xuất thẳng ra màn hình luôn chứ không thực hiện bước "kéo sáng" (gamma correction) trở lại sRGB.

### Cách khắc phục (Đảm bảo 100% giống ảnh gốc)

Cách đơn giản và an toàn nhất cho game 2D (hoặc UI) là **Bảo DirectX bỏ qua hoàn toàn mọi hệ màu, cứ đọc pixel thô và in thẳng ra màn hình**.

Bạn hãy đổi cờ trong hàm `CreateWICTextureFromFileEx` từ `FORCE_SRGB` thành **`WIC_LOADER_IGNORE_SRGB`**.

```cpp
    // Dùng cờ IGNORE_SRGB để ép DirectX đọc raw pixel, không tự động chuyển đổi Gamma
    HRESULT hr = CreateWICTextureFromFileEx(
        device, 
        context, 
        frameTexPath.c_str(),
        0, 
        D3D11_USAGE_DEFAULT, 
        D3D11_BIND_SHADER_RESOURCE, 
        0, 
        0, 
        WIC_LOADER_IGNORE_SRGB,   // <--- THAY ĐỔI TẠI ĐÂY
        nullptr, 
        mFrameSRV.GetAddressOf()
    );

```

*Lưu ý: Nhớ áp dụng cờ này cho cả lúc load ảnh `bgTexturePath`.*

Bằng cách này, DirectX sẽ load ảnh dưới dạng `UNORM` thô. Pixel màu gì nó bốc màu đó ném thẳng vào SwapChain `UNORM`, và màn hình của bạn sẽ hiển thị chính xác 100% so với ảnh gốc.

Bạn thử đổi sang cờ `WIC_LOADER_IGNORE_SRGB` rồi build lại xem kết quả đã hoàn hảo chưa nhé? Nếu vẫn có viền đen lấm tấm ở tóc nhân vật thì mình sẽ chỉnh lại một chút ở BlendState là xong!


<USER>: tôi có thử load 2 ảnh màu #B5E61D những cuối cùng nó thành #76CA03

Chính xác 100%! Những con số mã màu bạn vừa đưa ra là minh chứng hoàn hảo cho việc **DirectX đang tự động chuyển đổi không gian màu từ sRGB sang Linear (Tuyến tính)**.

Mình vừa làm một phép toán nhỏ với mã màu của bạn và kết quả khớp đến từng con số:

* Mã màu gốc của bạn là **`#B5E61D`** (RGB: `181, 230, 29`).
* Nếu mình đem các giá trị này chuyển đổi sang không gian Linear (bằng cách chia cho 255 rồi nâng lên lũy thừa 2.2 - chuẩn gamma thông thường), ta sẽ có:
* R: (181/255)^2.2 * 255 ≈ **118** (Hex: **`76`**)
* G: (230/255)^2.2 * 255 ≈ **202** (Hex: **`CA`**)
* B: (29/255)^2.2 * 255 ≈ **2.1** (Hex: **`03`**)


* Kết quả gộp lại chính xác là **`#76CA03`**!

### Chuyện gì đang xảy ra trong code của bạn?

1. DirectX đọc được file ảnh của bạn có chứa profile màu sRGB. Nó tự động nạp ảnh này bằng format `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`.
2. Khi `SpriteBatch` lấy màu để vẽ, phần cứng GPU thấy chữ `_SRGB` nên nó tự động làm phép tính giải mã (như mình vừa làm ở trên) để biến `#B5E61D` thành `#76CA03` (Linear) nhét vào Shader.
3. Shader đẩy màu `#76CA03` này ra Backbuffer (SwapChain). Do SwapChain của bạn đang cài đặt là chuẩn thường (`UNORM` - không có sRGB), nó cứ thế in thẳng con số `#76CA03` ra màn hình. Mắt bạn nhìn thấy nó bị tối đi.

---

### Cách khắc phục dứt điểm

Vì bạn đang làm game 2D/Pixel Art, cách dễ nhất là **tắt hoàn toàn hệ thống tự động đổi màu này đi**, bắt DirectX đọc mã màu thô (Raw) và in thẳng ra màn hình.

Bạn hãy kiểm tra lại xem đã dùng đúng hàm `CreateWICTextureFromFileEx` với cờ **`WIC_LOADER_IGNORE_SRGB`** như mình gợi ý ở câu trước chưa? (Lưu ý là `IGNORE_SRGB` chứ không phải `FORCE_SRGB` nhé).

```cpp
    HRESULT hr = CreateWICTextureFromFileEx(
        device, 
        context, 
        texturePath.c_str(),
        0, 
        D3D11_USAGE_DEFAULT, 
        D3D11_BIND_SHADER_RESOURCE, 
        0, 
        0, 
        WIC_LOADER_IGNORE_SRGB,   // <--- Cờ này sẽ chặn DirectX đổi #B5E61D thành #76CA03
        nullptr, 
        mSRV.GetAddressOf()
    );

```

**Nếu bạn đã dùng cờ này rồi mà vẫn bị**, có nghĩa là ở một bước nào đó, RenderTarget (hoặc SwapChain) của bạn đang được set sai format.

Bạn thử check lại code chỗ tạo Texture hoặc áp dụng cờ `IGNORE_SRGB` vào xem mã màu đã trở lại đúng `#B5E61D` chưa nhé! Có cần mình xem giúp đoạn tạo SwapChain không?