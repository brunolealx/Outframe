#pragma once

#include <windows.h>

namespace fpsgrade {

class Win32Window {
public:
    Win32Window(HINSTANCE instance, const wchar_t* class_name, const wchar_t* title);

    bool Create(int width, int height);
    void Show(int show_command);

    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Paint();

    HINSTANCE instance_ = nullptr;
    const wchar_t* class_name_ = nullptr;
    const wchar_t* title_ = nullptr;
    HWND hwnd_ = nullptr;
};

} // namespace fpsgrade
