#include "../../raylib-6.0/src/raylib.h"

#include <string>
#include <functional>

static int target_fps = 60;

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
static double last_frame_time = emscripten_get_now();
static std::function<void (float deltaTime)> web_tick_func;

static int target_width = 0;
static int target_height = 0;

#endif

#if defined(PLATFORM_WEB)
void web_tick(void)
{
    int frame_width = 0;
    int frame_height = 0;
    emscripten_get_screen_size(&frame_width, &frame_height);
    SetWindowSize(std::min(target_width, frame_width), std::min(target_height, frame_height));

    double delta = emscripten_get_now() - last_frame_time;
    web_tick_func(float(delta) / 1000.f);
    last_frame_time += delta;
}
#endif

void MP_InitWindow(std::function<void (float deltaTime)> tick, int width=640, int height=320, std::string title="Hello world!")
{
    InitWindow(width, height, title.c_str());

    #if defined(PLATFORM_WEB)
    web_tick_func = tick;
    target_width = width;
    target_height = height;
    emscripten_set_main_loop(web_tick, 0, 1);
    #else
    while(true)
    {
        tick(GetFrameTime());
    }
    #endif
    return;
}

void MP_SetMaxFPS(int newMaxFPS)
{
    target_fps = newMaxFPS;
    SetTargetFPS(newMaxFPS);
}