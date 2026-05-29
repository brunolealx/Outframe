#pragma once

#include "platform/Win32Window.h"

#include <windows.h>

namespace fpsgrade {

class FpsgradeApp {
public:
    explicit FpsgradeApp(HINSTANCE instance);

    int Run(int show_command);

private:
    HINSTANCE instance_ = nullptr;
    Win32Window main_window_;
};

} // namespace fpsgrade
