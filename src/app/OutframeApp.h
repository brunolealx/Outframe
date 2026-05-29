#pragma once

#include "platform/Win32Window.h"

#include <windows.h>

namespace outframe {

class OutframeApp {
public:
    explicit OutframeApp(HINSTANCE instance);

    int Run(int show_command);

private:
    HINSTANCE instance_ = nullptr;
    Win32Window main_window_;
};

} // namespace outframe
