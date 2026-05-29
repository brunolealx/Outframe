#include "platform/Win32Window.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <format>
#include <string_view>

namespace outframe {
namespace {

constexpr int kListTop = 224;
constexpr int kRowHeight = 34;

} // namespace

Win32Window::Win32Window(HINSTANCE instance, const wchar_t* class_name, const wchar_t* title)
    : instance_(instance), class_name_(class_name), title_(title) {}

bool Win32Window::Create(int width, int height) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpszClassName = class_name_;
    window_class.lpfnWndProc = Win32Window::WindowProc;
    window_class.style = CS_DBLCLKS;
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

void Win32Window::RefreshWindowList() {
    candidate_windows_ = EnumerateCandidateWindows(hwnd_);
    if (selected_index_ >= static_cast<int>(candidate_windows_.size())) {
        selected_index_ = -1;
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
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
        RefreshWindowList();
        return 0;
    }
    case WM_KEYDOWN:
        if (wparam == 'R') {
            RefreshWindowList();
            return 0;
        }
        if (wparam == VK_RETURN) {
            StartPreview();
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        SelectRowAt(GET_Y_LPARAM(lparam));
        return 0;
    case WM_LBUTTONDBLCLK:
        SelectRowAt(GET_Y_LPARAM(lparam));
        StartPreview();
        return 0;
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

    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void Win32Window::SelectRowAt(int y) {
    const int index = (y - kListTop) / kRowHeight;
    if (index >= 0 && index < static_cast<int>(candidate_windows_.size())) {
        selected_index_ = index;
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void Win32Window::StartPreview() {
    if (selected_index_ < 0 || selected_index_ >= static_cast<int>(candidate_windows_.size())) {
        MessageBoxW(hwnd_, L"Select a window first.", L"Outframe", MB_ICONINFORMATION);
        return;
    }

    if (presentation_window_ && presentation_window_->IsOpen()) {
        presentation_window_->Focus();
        return;
    }

    presentation_window_ = std::make_unique<PresentationWindow>(instance_, candidate_windows_[static_cast<size_t>(selected_index_)]);
    if (!presentation_window_->Create()) {
        MessageBoxW(hwnd_, L"Could not create the preview window.", L"Outframe", MB_ICONERROR);
        presentation_window_.reset();
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

    RECT title_rect{48, 36, client.right - 48, 88};
    SelectObject(dc, title_font);
    DrawTextW(dc, L"Outframe", -1, &title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SetTextColor(dc, RGB(180, 192, 202));
    SelectObject(dc, body_font);

    RECT body_rect{50, 94, client.right - 50, 160};
    constexpr std::wstring_view body =
        L"Click a window, press Enter to preview it, or double-click a row. Press R to refresh.";

    DrawTextW(dc, body.data(), static_cast<int>(body.size()), &body_rect, DT_LEFT | DT_WORDBREAK);

    RECT list_title_rect{50, 176, client.right - 50, 212};
    SetTextColor(dc, RGB(239, 244, 248));
    DrawTextW(dc, L"Visible windows", -1, &list_title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SetTextColor(dc, RGB(190, 202, 212));

    const int available_height = static_cast<int>(client.bottom) - kListTop - 48;
    const int max_rows = std::max(0, available_height / kRowHeight);
    const int row_count = std::min(static_cast<int>(candidate_windows_.size()), max_rows);

    for (int index = 0; index < row_count; ++index) {
        const WindowInfo& window = candidate_windows_[static_cast<size_t>(index)];
        const std::wstring line = std::format(L"{}  |  PID {}  |  {}", index + 1, window.process_id, window.title);
        RECT row_rect{50, kListTop + index * kRowHeight, client.right - 50, kListTop + (index + 1) * kRowHeight};
        if (index == selected_index_) {
            HBRUSH selected_brush = CreateSolidBrush(RGB(38, 78, 96));
            FillRect(dc, &row_rect, selected_brush);
            DeleteObject(selected_brush);
            SetTextColor(dc, RGB(248, 252, 255));
        } else {
            SetTextColor(dc, RGB(190, 202, 212));
        }
        DrawTextW(dc, line.c_str(), -1, &row_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    if (candidate_windows_.empty()) {
        RECT empty_rect{50, kListTop, client.right - 50, kListTop + kRowHeight};
        DrawTextW(dc, L"No candidate windows found.", -1, &empty_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }

    DeleteObject(title_font);
    DeleteObject(body_font);
    EndPaint(hwnd_, &paint);
}

} // namespace outframe
