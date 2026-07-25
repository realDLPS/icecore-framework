#include "../../raylib-6.0/src/raylib.h"

#include <string>

bool WEB = false;

#if defined(PLATFORM_WEB)
WEB = true;
#endif


void MP_InitWindow(int width, int height, std::string title)
{
    if(WEB)
    {

    }
    else
    {
        InitWindow(width, height, title.c_str());
    }
    return;
}