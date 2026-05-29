#include "app/OutframeApp.h"

#include <windows.h>

namespace outframe {

OutframeApp::OutframeApp(HINSTANCE instance)
    : instance_(instance),
      main_window_(instance, L"OutframeWindow", L"Outframe") {}

int OutframeApp::Run(int show_command) {
    if (!main_window_.Create(1120, 680)) {
        MessageBoxW(nullptr, L"Could not create Outframe window.", L"Outframe", MB_ICONERROR);
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

} // namespace outframe
