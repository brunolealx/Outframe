#include "platform/PresentationWindow.h"

#include <algorithm>
#include <format>

namespace outframe {
namespace {

constexpr wchar_t kPresentationClassName[] = L"OutframePresentationWindow";
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT kFrameIntervalMs = 16;

RECT FitRect(RECT source, RECT destination) {
    const int source_width = std::max(1, static_cast<int>(source.right - source.left));
    const int source_height = std::max(1, static_cast<int>(source.bottom - source.top));
    const int destination_width = std::max(1, static_cast<int>(destination.right - destination.left));
    const int destination_height = std::max(1, static_cast<int>(destination.bottom - destination.top));

    const double source_aspect = static_cast<double>(source_width) / static_cast<double>(source_height);
    const double destination_aspect = static_cast<double>(destination_width) / static_cast<double>(destination_height);

    RECT fitted = destination;
    if (destination_aspect > source_aspect) {
        const int width = static_cast<int>(destination_height * source_aspect);
        fitted.left = destination.left + (destination_width - width) / 2;
        fitted.right = fitted.left + width;
    } else {
        const int height = static_cast<int>(destination_width / source_aspect);
        fitted.top = destination.top + (destination_height - height) / 2;
        fitted.bottom = fitted.top + height;
    }

    return fitted;
}

} // namespace

PresentationWindow::PresentationWindow(HINSTANCE instance, WindowInfo target)
    : instance_(instance), target_(std::move(target)) {}

PresentationWindow::~PresentationWindow() {
    if (IsOpen()) {
        DestroyWindow(hwnd_);
    }
}

bool PresentationWindow::Create() {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpszClassName = kPresentationClassName;
    window_class.lpfnWndProc = PresentationWindow::WindowProc;
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (!RegisterClassExW(&window_class)) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kPresentationClassName,
        L"Outframe Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        720,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

bool PresentationWindow::IsOpen() const {
    return hwnd_ && IsWindow(hwnd_);
}

void PresentationWindow::Focus() {
    if (IsOpen()) {
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
    }
}

LRESULT CALLBACK PresentationWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    PresentationWindow* window = nullptr;

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<PresentationWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<PresentationWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(message, wparam, lparam);
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT PresentationWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd_, kFrameTimer, kFrameIntervalMs, nullptr);
        return 0;
    case WM_TIMER:
        if (wparam == kFrameTimer) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd_);
            return 0;
        }
        break;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kFrameTimer);
        hwnd_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd_, message, wparam, lparam);
    }

    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void PresentationWindow::Paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(hwnd_, &paint);

    RECT client{};
    GetClientRect(hwnd_, &client);

    HBRUSH background = CreateSolidBrush(RGB(8, 10, 14));
    FillRect(dc, &client, background);
    DeleteObject(background);

    if (!IsWindow(target_.hwnd) || IsIconic(target_.hwnd)) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(230, 235, 240));
        DrawTextW(dc, L"Target window is unavailable or minimized.", -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd_, &paint);
        return;
    }

    RECT source_bounds{};
    if (!GetWindowRect(target_.hwnd, &source_bounds)) {
        EndPaint(hwnd_, &paint);
        return;
    }

    const RECT target_rect = FitRect(source_bounds, client);
    HDC source_dc = GetWindowDC(target_.hwnd);
    if (source_dc) {
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchBlt(
            dc,
            target_rect.left,
            target_rect.top,
            target_rect.right - target_rect.left,
            target_rect.bottom - target_rect.top,
            source_dc,
            0,
            0,
            source_bounds.right - source_bounds.left,
            source_bounds.bottom - source_bounds.top,
            SRCCOPY);
        ReleaseDC(target_.hwnd, source_dc);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(240, 244, 248));
    const std::wstring overlay = std::format(L"Outframe preview: {}  |  Esc closes", target_.title);
    RECT overlay_rect{18, 14, client.right - 18, 46};
    DrawTextW(dc, overlay.c_str(), -1, &overlay_rect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    EndPaint(hwnd_, &paint);
}

} // namespace outframe
