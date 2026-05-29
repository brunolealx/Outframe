#include "app/FpsgradeApp.h"

#include <windows.h>

namespace fpsgrade {

FpsgradeApp::FpsgradeApp(HINSTANCE instance)
    : instance_(instance),
      main_window_(instance, L"FPSgradeEngineWindow", L"FPSgrade Engine") {}

int FpsgradeApp::Run(int show_command) {
    if (!main_window_.Create(1120, 680)) {
        MessageBoxW(nullptr, L"Could not create FPSgrade Engine window.", L"FPSgrade Engine", MB_ICONERROR);
        return 1;
    }

    main_window_.Show(show_command);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

} // namespace fpsgrade
