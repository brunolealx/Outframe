#include "app/OutframeApp.h"

#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    outframe::OutframeApp app(instance);
    return app.Run(show_command);
}
