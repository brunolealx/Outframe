#include "app/FpsgradeApp.h"

#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    fpsgrade::FpsgradeApp app(instance);
    return app.Run(show_command);
}
