#pragma once

#include "platform/PresentationWindow.h"
#include "platform/WindowInfo.h"

#include <windows.h>

#include <memory>
#include <vector>

namespace outframe {

class Win32Window {
public:
    Win32Window(HINSTANCE instance, const wchar_t* class_name, const wchar_t* title);

    bool Create(int width, int height);
    void Show(int show_command);
    void RefreshWindowList();

    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Paint();
    void SelectRowAt(int y);
    void StartPreview();

    HINSTANCE instance_ = nullptr;
    const wchar_t* class_name_ = nullptr;
    const wchar_t* title_ = nullptr;
    HWND hwnd_ = nullptr;
    std::vector<WindowInfo> candidate_windows_;
    std::unique_ptr<PresentationWindow> presentation_window_;
    int selected_index_ = -1;
};

} // namespace outframe
