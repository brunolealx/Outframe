#include "platform/Win32Window.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <string_view>

namespace fpsgrade {

Win32Window::Win32Window(HINSTANCE instance, const wchar_t* class_name, const wchar_t* title)
    : instance_(instance), class_name_(class_name), title_(title) {}

bool Win32Window::Create(int width, int height) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpszClassName = class_name_;
    window_class.lpfnWndProc = Win32Window::WindowProc;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&window_class)) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    RECT bounds{0, 0, width, height};
    AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0);

    hwnd_ = CreateWindowExW(
        0,
        class_name_,
        title_,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        instance_,
        this);

    return hwnd_ != nullptr;
}

void Win32Window::Show(int show_command) {
    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
}

LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    Win32Window* window = nullptr;

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<Win32Window*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(message, wparam, lparam);
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT Win32Window::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        const BOOL immersive_dark_mode = TRUE;
        DwmSetWindowAttribute(hwnd_, 20, &immersive_dark_mode, sizeof(immersive_dark_mode));
        return 0;
    }
    case WM_PAINT:
        Paint();
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd_, nullptr, TRUE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd_, message, wparam, lparam);
    }
}

void Win32Window::Paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(hwnd_, &paint);

    RECT client{};
    GetClientRect(hwnd_, &client);

    HBRUSH background = CreateSolidBrush(RGB(16, 18, 22));
    FillRect(dc, &client, background);
    DeleteObject(background);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(239, 244, 248));

    HFONT title_font = CreateFontW(
        34, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT body_font = CreateFontW(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    RECT title_rect{48, 44, client.right - 48, 96};
    SelectObject(dc, title_font);
    DrawTextW(dc, L"FPSgrade Engine", -1, &title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SetTextColor(dc, RGB(180, 192, 202));
    SelectObject(dc, body_font);

    RECT body_rect{50, 112, client.right - 50, 240};
    constexpr std::wstring_view body =
        L"Clean-room baseline ready. Next steps: window picker, Windows Graphics Capture, "
        L"Direct3D presentation, and original game-focused scaling presets.";

    DrawTextW(dc, body.data(), static_cast<int>(body.size()), &body_rect, DT_LEFT | DT_WORDBREAK);

    DeleteObject(title_font);
    DeleteObject(body_font);
    EndPaint(hwnd_, &paint);
}

} // namespace fpsgrade
