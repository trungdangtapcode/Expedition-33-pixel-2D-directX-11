// Bắt buộc định nghĩa UNICODE để các hàm Windows dùng chuỗi ký tự rộng (wchar_t)
// thay vì chuỗi ASCII thông thường. Ví dụ: L"Hello" thay vì "Hello"
#define UNICODE
#define _UNICODE  // Phiên bản UNICODE dành cho thư viện chuẩn C (như _tprintf, _T("..."))

#include <windows.h>  // Thư viện lõi của Windows: tạo cửa sổ, xử lý sự kiện, v.v.
#include <d3d11.h>    // Thư viện DirectX 11: API đồ họa 3D của Microsoft


// Yêu cầu trình liên kết (linker) tự động nạp file .lib khi biên dịch,
// thay vì phải cấu hình thủ công trong Project Settings
#pragma comment(lib, "user32.lib")  // Thư viện Windows để tạo cửa sổ, xử lý input bàn phím/chuột
#pragma comment(lib, "d3d11.lib")   // Thư viện DirectX 11 để vẽ đồ họa

// ============================================================
// CÁC BIẾN TOÀN CỤC CHO DIRECTX (dùng chung trong toàn bộ chương trình)
// ============================================================

// IDXGISwapChain: Quản lý "chuỗi hoán đổi" (swap chain)
// Swap chain gồm 2 buffer: front buffer (đang hiển thị) và back buffer (đang vẽ)
// Khi vẽ xong, hai buffer sẽ hoán đổi để hiển thị hình ảnh mới - tránh hiện tượng giật hình
IDXGISwapChain* swapchain = nullptr;

// ID3D11Device: Đại diện cho GPU (card đồ họa)
// Dùng để tạo các tài nguyên đồ họa: texture, shader, buffer...
// Giống như "nhà máy sản xuất" các đối tượng DirectX
ID3D11Device* dev = nullptr;

// ID3D11DeviceContext: Là "người điều phối lệnh vẽ"
// Dùng để gửi lệnh vẽ (draw calls) và cấu hình pipeline đồ họa đến GPU
// Khác với Device (tạo tài nguyên), DeviceContext thực thi các lệnh render
ID3D11DeviceContext* devcon = nullptr;

// ID3D11RenderTargetView: Đây là "vùng đích" mà GPU sẽ vẽ lên
// Ở đây nó trỏ vào back buffer của swap chain - tức là vẽ vào màn hình
ID3D11RenderTargetView* backbuffer = nullptr;

// ============================================================
// HÀM KHỞI TẠO DIRECT3D
// Nhận vào handle của cửa sổ (HWND) để DirectX biết vẽ vào cửa sổ nào
// ============================================================
void InitD3D(HWND hWnd) {
    // DXGI_SWAP_CHAIN_DESC: Cấu trúc mô tả thông số cho swap chain
    // = { 0 } nghĩa là khởi tạo toàn bộ về 0 trước khi thiết lập từng trường
    DXGI_SWAP_CHAIN_DESC scd = { 0 };

    // Số lượng back buffer: 1 back buffer + 1 front buffer = double buffering
    scd.BufferCount = 1;

    // Định dạng màu của mỗi pixel: R8G8B8A8_UNORM
    // = mỗi kênh màu (Đỏ, Xanh lá, Xanh dương, Alpha) dùng 8-bit, giá trị từ 0.0 đến 1.0
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Mục đích sử dụng buffer: dùng làm Render Target (vùng để vẽ hình ảnh lên)
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    // Gắn swap chain với cửa sổ Windows cụ thể này (hWnd)
    scd.OutputWindow = hWnd;

    // Cấu hình Anti-Aliasing (khử răng cưa): Count = 1 nghĩa là tắt AA
    // Nếu Count = 4 thì bật 4x MSAA (hình mịn hơn nhưng tốn tài nguyên hơn)
    scd.SampleDesc.Count = 1;

    // Chạy ở chế độ cửa sổ (Windowed), không phải toàn màn hình
    scd.Windowed = TRUE;

    // Tạo đồng thời: Device (GPU), DeviceContext (bộ điều phối lệnh), và SwapChain (hệ thống buffer)
    // - nullptr đầu: không dùng adapter cụ thể, để hệ thống tự chọn GPU mặc định
    // - D3D_DRIVER_TYPE_HARDWARE: dùng GPU thật (phần cứng), nhanh nhất
    // - nullptr (flag): không bật thêm tính năng đặc biệt nào
    // - 0 (flags): không bật debug layer hay các tùy chọn khác
    // - nullptr, 0 (feature levels): tự động chọn feature level cao nhất có thể
    // - D3D11_SDK_VERSION: phiên bản SDK, luôn dùng hằng số này
    // - &scd: truyền vào cấu hình swap chain đã thiết lập ở trên
    // - &swapchain, &dev, nullptr, &devcon: nhận kết quả trả về
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &scd, &swapchain, &dev, nullptr, &devcon);

    // Lấy con trỏ tới back buffer (texture 2D mà GPU sẽ vẽ lên)
    // - GetBuffer(0, ...): lấy buffer số 0 (back buffer đầu tiên)
    // - __uuidof(ID3D11Texture2D): ID định danh kiểu dữ liệu muốn lấy (một Texture 2D)
    // - (LPVOID*)&pBackBuffer: nơi lưu con trỏ kết quả
    ID3D11Texture2D* pBackBuffer = nullptr;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

    // Tạo RenderTargetView từ back buffer
    // RenderTargetView là "cửa sổ nhìn vào texture" để GPU biết nơi cần vẽ
    // - pBackBuffer: texture nguồn
    // - nullptr: dùng toàn bộ texture, không giới hạn vùng con
    // - &backbuffer: lưu kết quả vào biến toàn cục
    dev->CreateRenderTargetView(pBackBuffer, nullptr, &backbuffer);

    // Giải phóng con trỏ tạm pBackBuffer vì ta đã tạo xong RenderTargetView
    // DirectX dùng COM (Component Object Model) - mọi đối tượng phải được Release() khi không dùng nữa
    pBackBuffer->Release();

    // Đăng ký RenderTargetView với pipeline đồ họa
    // OMSetRenderTargets: OM = Output Merger (giai đoạn cuối của pipeline đồ họa)
    // - 1: số lượng render target (ở đây chỉ 1)
    // - &backbuffer: mảng các render target views
    // - nullptr: không dùng Depth-Stencil buffer (kiểm tra độ sâu 3D) - vì đây là ví dụ 2D đơn giản
    devcon->OMSetRenderTargets(1, &backbuffer, nullptr);
}

// ============================================================
// HÀM VẼ MỖI KHUNG HÌNH (FRAME)
// Được gọi liên tục trong vòng lặp chính - càng gọi nhanh thì FPS càng cao
// ============================================================
void RenderFrame() {
    // Định nghĩa màu nền dạng mảng 4 phần tử: { R, G, B, A }
    // Mỗi giá trị từ 0.0 (tối) đến 1.0 (sáng nhất)
    // Ở đây: R=0, G=0.2, B=0.4, A=1.0 → màu xanh dương đậm, không trong suốt
    float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

    // Xóa toàn bộ back buffer bằng màu clearColor
    // Giống như tô lại nền trước khi vẽ hình mới lên - tránh hiện tượng "bóng ma" (ghosting)
    devcon->ClearRenderTargetView(backbuffer, clearColor);

    // Hoán đổi front buffer và back buffer để hiển thị kết quả ra màn hình
    // - Tham số 1 (0): VSync - 0 = không đồng bộ với màn hình (vẽ nhanh nhất có thể)
    //                          1 = đồng bộ với tần số màn hình (giới hạn FPS, tránh tearing)
    // - Tham số 2 (0): Cờ đặc biệt, 0 = không dùng cờ nào
    swapchain->Present(0, 0);
}

// ============================================================
// HÀM DỌN DẸP BỘ NHỚ KHI ĐÓNG ỨNG DỤNG
// QUAN TRỌNG: Mọi đối tượng DirectX (COM objects) phải được Release() để tránh memory leak
// Thứ tự release ngược lại với thứ tự tạo ra
// ============================================================
void CleanD3D() {
    // Tắt chế độ toàn màn hình trước khi giải phóng swap chain
    // Nếu không làm bước này, ứng dụng có thể crash khi đóng ở chế độ fullscreen
    if (swapchain) swapchain->SetFullscreenState(FALSE, nullptr);

    // Giải phóng Render Target View (vùng đích vẽ)
    if (backbuffer) backbuffer->Release();

    // Giải phóng Swap Chain (hệ thống buffer đôi)
    if (swapchain) swapchain->Release();

    // Giải phóng Device (đại diện GPU)
    if (dev) dev->Release();

    // Giải phóng Device Context (bộ điều phối lệnh vẽ)
    if (devcon) devcon->Release();
}

// ============================================================
// HÀM XỬ LÝ SỰ KIỆN CỬA SỔ WINDOWS (Window Procedure / Callback)
// Windows gọi hàm này mỗi khi có sự kiện xảy ra với cửa sổ:
// click chuột, gõ phím, di chuyển cửa sổ, đóng cửa sổ...
// ============================================================
// - hWnd: handle của cửa sổ nhận sự kiện
// - message: mã số loại sự kiện (WM_DESTROY, WM_KEYDOWN, WM_MOUSEMOVE...)
// - wParam, lParam: thông tin bổ sung tùy theo từng loại sự kiện
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // WM_DESTROY: gửi khi cửa sổ bị đóng (người dùng nhấn nút X)
    if (message == WM_DESTROY) {
        // Gửi thông điệp WM_QUIT vào message queue để thoát vòng lặp chính
        // Tham số 0 là exit code của ứng dụng (0 = thành công)
        PostQuitMessage(0);
        return 0;  // Trả về 0 = đã xử lý xong sự kiện này
    }
    // Với các sự kiện khác (WM_SIZE, WM_PAINT...), nhờ Windows xử lý mặc định
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// ============================================================
// HÀM WINMAIN - ĐIỂM KHỞI ĐẦU CỦA ỨNG DỤNG WINDOWS
// Giống như main() trong C++ console, nhưng dành cho ứng dụng có giao diện đồ họa
// - hInstance: handle định danh duy nhất của ứng dụng này trong hệ thống
// - hPrevInstance: luôn là nullptr trong Windows hiện đại (không còn dùng)
// - lpCmdLine: chuỗi các tham số dòng lệnh (như argv trong main)
// - nCmdShow: cách hiển thị cửa sổ ban đầu (thu nhỏ, bình thường, phóng to...)
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // -------------------------------------------------------
    // BƯỚC 1: Đăng ký "class cửa sổ" với Windows
    // Class cửa sổ là bản thiết kế (blueprint) định nghĩa hành vi và ngoại hình
    // của loại cửa sổ này. Nhiều cửa sổ có thể dùng chung 1 class.
    // -------------------------------------------------------
    WNDCLASSEX wc = { 0 };  // Khởi tạo toàn bộ về 0
    wc.cbSize = sizeof(WNDCLASSEX);         // Kích thước struct (bắt buộc phải set)
    wc.style = CS_HREDRAW | CS_VREDRAW;     // Vẽ lại cửa sổ khi thay đổi kích thước ngang/dọc
    wc.lpfnWndProc = WindowProc;            // Con trỏ hàm xử lý sự kiện (đã định nghĩa ở trên)
    wc.hInstance = hInstance;               // Gắn class này với ứng dụng hiện tại
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);  // Con trỏ chuột mặc định (mũi tên)
    wc.lpszClassName = L"DX11WindowClass";  // Tên định danh của class (dùng để tạo cửa sổ sau)
    RegisterClassEx(&wc);                   // Đăng ký class với hệ điều hành Windows

    // -------------------------------------------------------
    // BƯỚC 2: Tạo cửa sổ thực tế dựa trên class đã đăng ký
    // -------------------------------------------------------
    // CreateWindowEx trả về HWND - handle (định danh) của cửa sổ vừa tạo
    // - 0: không dùng kiểu mở rộng nào (Extended style)
    // - L"DX11WindowClass": tên class đã đăng ký ở bước 1
    // - L"DirectX 11 - Cửa sổ đầu tiên": tiêu đề hiển thị trên thanh tiêu đề
    // - WS_OVERLAPPEDWINDOW: kiểu cửa sổ tiêu chuẩn (có thanh tiêu đề, nút thu phóng, đóng)
    // - 300, 300: tọa độ X, Y của góc trên-trái cửa sổ so với màn hình (pixel)
    // - 800, 600: chiều rộng và chiều cao cửa sổ (pixel)
    // - nullptr: không có cửa sổ cha (parent window)
    // - nullptr: không có menu
    // - hInstance: ứng dụng sở hữu cửa sổ này
    // - nullptr: không truyền dữ liệu bổ sung khi tạo
    HWND hWnd = CreateWindowEx(0, L"DX11WindowClass", L"DirectX 11 - Cửa sổ đầu tiên",
        WS_OVERLAPPEDWINDOW, 300, 300, 800, 600,
        nullptr, nullptr, hInstance, nullptr);

    // Hiển thị cửa sổ lên màn hình theo cách được chỉ định bởi nCmdShow
    // (SW_SHOW = hiển thị bình thường, SW_MINIMIZE = thu nhỏ, SW_MAXIMIZE = phóng to...)
    ShowWindow(hWnd, nCmdShow);

    // -------------------------------------------------------
    // BƯỚC 3: Khởi tạo DirectX 11
    // -------------------------------------------------------
    InitD3D(hWnd);  // Truyền handle cửa sổ để DirectX biết nơi vẽ

    // -------------------------------------------------------
    // BƯỚC 4: VÒNG LẶP CHÍNH (Message Loop / Game Loop)
    // Chạy liên tục cho đến khi người dùng đóng cửa sổ.
    // Đây là "trái tim" của mọi ứng dụng Windows và game engine.
    // -------------------------------------------------------
    MSG msg = { 0 };  // Cấu trúc chứa thông tin về sự kiện Windows
    while (TRUE) {  // Lặp vô hạn cho đến khi gặp lệnh break
        // PeekMessage: kiểm tra xem có sự kiện nào đang chờ trong hàng đợi không
        // - PM_REMOVE: lấy sự kiện ra khỏi hàng đợi sau khi đọc
        // Khác với GetMessage (chặn chương trình chờ sự kiện),
        // PeekMessage trả về ngay lập tức → không làm đứng game loop
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // Chuyển đổi sự kiện bàn phím thô thành ký tự (WM_CHAR)
            TranslateMessage(&msg);
            // Gửi sự kiện đến hàm WindowProc để xử lý
            DispatchMessage(&msg);
            // Nếu nhận được WM_QUIT (người dùng đóng cửa sổ) → thoát vòng lặp
            if (msg.message == WM_QUIT) break;
        }
        else {
            // Không có sự kiện nào → vẽ khung hình tiếp theo
            // Đây là nơi game/animation chạy liên tục
            RenderFrame();
        }
    }

    // -------------------------------------------------------
    // BƯỚC 5: Dọn dẹp tài nguyên DirectX trước khi thoát
    // -------------------------------------------------------
    CleanD3D();

    // Trả về exit code cho Windows (0 = thoát bình thường)
    return msg.wParam;
}