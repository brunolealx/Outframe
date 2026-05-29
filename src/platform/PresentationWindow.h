#pragma once

#include "platform/WindowInfo.h"

#include <windows.h>

namespace outframe {

class PresentationWindow {
public:
    PresentationWindow(HINSTANCE instance, WindowInfo target);
    ~PresentationWindow();

    bool Create();
    bool IsOpen() const;
    void Focus();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Paint();

    HINSTANCE instance_ = nullptr;
    WindowInfo target_;
    HWND hwnd_ = nullptr;
};

} // namespace outframe
