#include "App.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    return App{}.Run(instance, showCommand);
}
