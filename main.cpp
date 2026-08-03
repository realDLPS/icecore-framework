#include "raylib-6.0/src/raylib.h"

#include <string>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define ICFW_DRAWING
#define ICFW_WINDOW
#define ICFW_INPUT
#include "src/icecore_framework.hpp"

void tick(float deltaTime)
{
    BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText((std::to_string(1.f / deltaTime).c_str()), 190, 200, 20, LIGHTGRAY);

    EndDrawing();
}

bool CloseWindowEvent(icfw_digital_action_state state, bool updated)
{
    if(updated && state == press_started)
    {
        MP_Exit();

        return true;
    }
    return false;
}

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    AddMapping(INPUT_MAPPING({INPUT_ACTION(digital, {KEYBOARD_TRIGGER(KEY_ESCAPE)}, CloseWindowEvent)}), "default");
    LoadMapping("default");

    MP_SetMaxFPS(120);
    MP_InitWindow(tick, screenWidth, screenHeight, "raylib [core] example - basic window");

    CloseWindow();        // Close window and OpenGL context

    return 0;
}