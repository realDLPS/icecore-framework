#include "raylib-6.0/src/raylib.h"

#include <string>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include "src/window/multiplatform_window.hpp"

void tick(float deltaTime)
{
    BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText((std::to_string(1.f / deltaTime).c_str()), 190, 200, 20, LIGHTGRAY);

    EndDrawing();
}

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    MP_SetMaxFPS(120);
    MP_InitWindow(tick, screenWidth, screenHeight, "raylib [core] example - basic window");

    CloseWindow();        // Close window and OpenGL context

    return 0;
}