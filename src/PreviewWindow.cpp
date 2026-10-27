#include <PreviewWindow.hpp>
#include <Renderer.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <system_error>
#include <vector>


namespace {

constexpr wchar_t kWindowClassName[] = L"RayTracerPreviewWindow";
constexpr wchar_t kWindowTitle[] = L"RayTracer";

constexpr UINT_PTR kRefreshTimerId = 1;
constexpr UINT kRefreshIntervalMs = 100;

}  // namespace


struct PreviewWindow::Impl {
    explicit Impl(const Renderer& source);

    ~Impl();

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    void run();

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle_message(UINT message, WPARAM wParam, LPARAM lParam);

    void create_window();
    void refresh_image();
    void update_title();
    void paint(HDC target);
    bool ensure_back_buffer(HDC target, int width, int height);
    void release_back_buffer();

    const Renderer& renderer;

    HWND window{nullptr};
    bool timer_running{false};

    // Display copy in GDI's bottom-up-agnostic layout: tightly packed BGR
    // rows padded to a four-byte boundary, top row first (negative biHeight).
    std::vector<unsigned char> pixels;
    std::vector<unsigned char> row_scratch;
    std::vector<unsigned char> row_copied;
    size_t stride{0};
    size_t copied_rows{0};
    BITMAPINFO bitmap_info{};

    // Back buffer used to compose a whole frame before it reaches the screen,
    // which keeps resizing and the periodic updates flicker-free.
    HDC memory_dc{nullptr};
    HBITMAP memory_bitmap{nullptr};
    HGDIOBJ previous_bitmap{nullptr};
    int buffer_width{0};
    int buffer_height{0};

    int shown_percent{-1};
};


PreviewWindow::Impl::Impl(const Renderer& source) : renderer(source) {
    const size_t image_width = renderer.width();
    const size_t image_height = renderer.height();

    // GDI scanlines of a 24-bit DIB are DWORD aligned; the renderer's rows are
    // tightly packed, so the display copy has its own padded stride.
    const size_t bytes_per_row = image_width * 3;
    stride = (bytes_per_row + 3) & ~size_t{3};

    pixels.assign(stride * image_height, 0);
    row_scratch.resize(bytes_per_row);
    row_copied.assign(image_height, 0);

    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = static_cast<LONG>(image_width);
    bitmap_info.bmiHeader.biHeight = -static_cast<LONG>(image_height);  // top-down
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 24;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    bitmap_info.bmiHeader.biSizeImage = static_cast<DWORD>(stride * image_height);

    create_window();
}


PreviewWindow::Impl::~Impl() {
    if (window != nullptr) {
        KillTimer(window, kRefreshTimerId);
        DestroyWindow(window);
        window = nullptr;
    }

    release_back_buffer();
}


void PreviewWindow::Impl::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}


void PreviewWindow::Impl::create_window() {
    // Physical pixels keep the preview crisp on scaled displays; failure only
    // means Windows keeps its default DPI virtualisation.
    SetProcessDPIAware();

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &Impl::window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;
    window_class.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    if (RegisterClassExW(&window_class) == 0) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            throw std::system_error(
                static_cast<int>(error), std::system_category(), "RegisterClassExW failed");
        }
    }

    RECT work_area{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.top = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
        work_area.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    const int work_width = work_area.right - work_area.left;
    const int work_height = work_area.bottom - work_area.top;

    int client_width = static_cast<int>(renderer.width());
    int client_height = static_cast<int>(renderer.height());

    // Fit the render resolution on screen without enlarging it.
    const double fit = std::min({
        1.0,
        static_cast<double>(work_width) * 0.9 / std::max(client_width, 1),
        static_cast<double>(work_height) * 0.9 / std::max(client_height, 1),
    });
    client_width = std::max(1, static_cast<int>(client_width * fit));
    client_height = std::max(1, static_cast<int>(client_height * fit));

    RECT window_rect{0, 0, client_width, client_height};
    AdjustWindowRectEx(&window_rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const int window_width = window_rect.right - window_rect.left;
    const int window_height = window_rect.bottom - window_rect.top;
    const int window_x = work_area.left + std::max(0, (work_width - window_width) / 2);
    const int window_y = work_area.top + std::max(0, (work_height - window_height) / 2);

    window = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        window_x,
        window_y,
        window_width,
        window_height,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (window == nullptr) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(), "CreateWindowExW failed");
    }

    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);

    timer_running = SetTimer(window, kRefreshTimerId, kRefreshIntervalMs, nullptr) != 0;
}


LRESULT CALLBACK PreviewWindow::Impl::window_proc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<Impl*>(create->lpCreateParams);
        self->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }

    if (self != nullptr) {
        return self->handle_message(message, wParam, lParam);
    }

    return DefWindowProcW(window, message, wParam, lParam);
}


LRESULT PreviewWindow::Impl::handle_message(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint_state{};
            HDC dc = BeginPaint(window, &paint_state);
            paint(dc);
            EndPaint(window, &paint_state);
            return 0;
        }

        case WM_ERASEBKGND:
            // paint() always covers the complete client area.
            return 1;

        case WM_SIZE:
            InvalidateRect(window, nullptr, FALSE);
            return 0;

        case WM_TIMER:
            if (wParam == kRefreshTimerId) {
                refresh_image();
                update_title();
                return 0;
            }
            break;

        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            limits->ptMinTrackSize.x = 200;
            limits->ptMinTrackSize.y = 150;
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(window);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            KillTimer(window, kRefreshTimerId);
            timer_running = false;
            release_back_buffer();
            window = nullptr;
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}


void PreviewWindow::Impl::refresh_image() {
    const size_t image_width = renderer.width();
    const size_t image_height = renderer.height();

    bool changed = false;

    for (size_t row = 0; row < image_height; ++row) {
        if (row_copied[row] != 0) {
            continue;
        }

        if (!renderer.copy_row_if_complete(row, row_scratch.data())) {
            continue;
        }

        const unsigned char* source = row_scratch.data();
        unsigned char* destination = pixels.data() + row * stride;

        // The renderer emits RGB; a 24-bit BI_RGB DIB expects BGR.
        for (size_t x = 0; x < image_width; ++x) {
            destination[x * 3 + 0] = source[x * 3 + 2];
            destination[x * 3 + 1] = source[x * 3 + 1];
            destination[x * 3 + 2] = source[x * 3 + 0];
        }

        row_copied[row] = 1;
        ++copied_rows;
        changed = true;
    }

    if (changed) {
        InvalidateRect(window, nullptr, FALSE);
    }

    // Nothing more can arrive, so stop waking the UI thread.
    if (timer_running && copied_rows == image_height) {
        KillTimer(window, kRefreshTimerId);
        timer_running = false;
    }
}


void PreviewWindow::Impl::update_title() {
    const size_t image_height = renderer.height();
    const int percent = image_height == 0
        ? 100
        : static_cast<int>(renderer.completed_rows() * 100 / image_height);

    if (percent == shown_percent) {
        return;
    }
    shown_percent = percent;

    std::wstring title = kWindowTitle;
    if (percent < 100) {
        title += L" - rendering ";
        title += std::to_wstring(percent);
        title += L'%';
    }

    SetWindowTextW(window, title.c_str());
}


void PreviewWindow::Impl::paint(HDC target) {
    if (target == nullptr) {
        return;
    }

    RECT client{};
    if (!GetClientRect(window, &client)) {
        return;
    }

    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    if (client_width <= 0 || client_height <= 0) {
        return;
    }

    if (!ensure_back_buffer(target, client_width, client_height)) {
        return;
    }

    const RECT full_client{0, 0, client_width, client_height};
    FillRect(memory_dc, &full_client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    const int image_width = static_cast<int>(renderer.width());
    const int image_height = static_cast<int>(renderer.height());

    if (image_width > 0 && image_height > 0) {
        // Letterbox the image, preserving the render's aspect ratio.
        const double scale = std::min(
            static_cast<double>(client_width) / image_width,
            static_cast<double>(client_height) / image_height);
        const int draw_width = std::max(1, static_cast<int>(image_width * scale));
        const int draw_height = std::max(1, static_cast<int>(image_height * scale));
        const int draw_x = (client_width - draw_width) / 2;
        const int draw_y = (client_height - draw_height) / 2;

        SetStretchBltMode(memory_dc, HALFTONE);
        SetBrushOrgEx(memory_dc, 0, 0, nullptr);

        StretchDIBits(
            memory_dc,
            draw_x,
            draw_y,
            draw_width,
            draw_height,
            0,
            0,
            image_width,
            image_height,
            pixels.data(),
            &bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);
    }

    BitBlt(target, 0, 0, client_width, client_height, memory_dc, 0, 0, SRCCOPY);
}


bool PreviewWindow::Impl::ensure_back_buffer(HDC target, int width, int height) {
    if (memory_dc != nullptr && width == buffer_width && height == buffer_height) {
        return true;
    }

    release_back_buffer();

    HDC dc = CreateCompatibleDC(target);
    if (dc == nullptr) {
        return false;
    }

    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    if (bitmap == nullptr) {
        DeleteDC(dc);
        return false;
    }

    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
    if (old_bitmap == nullptr || old_bitmap == HGDI_ERROR) {
        DeleteObject(bitmap);
        DeleteDC(dc);
        return false;
    }

    memory_dc = dc;
    memory_bitmap = bitmap;
    previous_bitmap = old_bitmap;
    buffer_width = width;
    buffer_height = height;
    return true;
}


void PreviewWindow::Impl::release_back_buffer() {
    if (memory_dc != nullptr) {
        if (previous_bitmap != nullptr) {
            SelectObject(memory_dc, previous_bitmap);
        }
        DeleteDC(memory_dc);
        memory_dc = nullptr;
        previous_bitmap = nullptr;
    }

    if (memory_bitmap != nullptr) {
        DeleteObject(memory_bitmap);
        memory_bitmap = nullptr;
    }

    buffer_width = 0;
    buffer_height = 0;
}


PreviewWindow::PreviewWindow(const Renderer& renderer)
    : m_impl(std::make_unique<Impl>(renderer))
{}


PreviewWindow::~PreviewWindow() = default;


void PreviewWindow::run() {
    m_impl->run();
}
