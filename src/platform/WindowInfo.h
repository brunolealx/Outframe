#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace outframe {

struct WindowInfo {
    HWND hwnd = nullptr;
    DWORD process_id = 0;
    std::wstring title;
};

std::vector<WindowInfo> EnumerateCandidateWindows(HWND owner);

} // namespace outframe
