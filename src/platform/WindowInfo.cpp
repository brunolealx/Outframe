#include "platform/WindowInfo.h"

#include <algorithm>

namespace outframe {
namespace {

struct EnumerationContext {
    HWND owner = nullptr;
    std::vector<WindowInfo> windows;
};

bool IsCandidateWindow(HWND hwnd, HWND owner) {
    if (hwnd == owner || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return false;
    }

    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return false;
    }

    const int title_length = GetWindowTextLengthW(hwnd);
    if (title_length <= 0) {
        return false;
    }

    RECT bounds{};
    if (!GetWindowRect(hwnd, &bounds)) {
        return false;
    }

    return (bounds.right - bounds.left) > 80 && (bounds.bottom - bounds.top) > 80;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lparam) {
    auto* context = reinterpret_cast<EnumerationContext*>(lparam);
    if (!IsCandidateWindow(hwnd, context->owner)) {
        return TRUE;
    }

    const int title_length = GetWindowTextLengthW(hwnd);
    std::wstring title(static_cast<size_t>(title_length + 1), L'\0');
    const int copied = GetWindowTextW(hwnd, title.data(), static_cast<int>(title.size()));
    title.resize(static_cast<size_t>(copied));

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);

    context->windows.push_back(WindowInfo{
        .hwnd = hwnd,
        .process_id = process_id,
        .title = std::move(title),
    });

    return TRUE;
}

} // namespace

std::vector<WindowInfo> EnumerateCandidateWindows(HWND owner) {
    EnumerationContext context{.owner = owner};
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&context));

    std::sort(context.windows.begin(), context.windows.end(), [](const WindowInfo& left, const WindowInfo& right) {
        return left.title < right.title;
    });

    return context.windows;
}

} // namespace outframe
