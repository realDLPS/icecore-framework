#pragma once

#pragma region Vector helpers
#include "../raylib-6.0/src/raylib.h"
#include "../raylib-6.0/src/raymath.h"

inline Vector2 Vec2(float x, float y)
{
    auto v = Vector2();
    v.x = x;
    v.y = y;

    return v;
}
inline Vector2 Vec2(float i)
{
    auto v = Vector2();
    v.x = i;
    v.y = i;

    return v;
}
inline Vector2 rotVec2(Vector2 v, float angle)
{
	// Constant is defined as 1/(180/pi)
	return Vector2Rotate(v, angle*0.01745329251994329577f);
}
#pragma endregion


#pragma region ICFW_WINDOW
#if defined(ICFW_WINDOW) // Multiplatform window

#include "../raylib-6.0/src/raylib.h"

#include <string>
#include <functional>

namespace engine {
    static int target_fps = 60;
    static bool exit_started = false;
}

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
namespace engine {
    static double last_frame_time = emscripten_get_now();
    static std::function<void (float deltaTime)> web_tick_func;

    static int target_width = 0;
    static int target_height = 0;
}


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
    while(!engine::exit_started)
    {
        tick(GetFrameTime());
    }
    #endif
    CloseWindow();
    return;
}

void MP_SetMaxFPS(int newMaxFPS)
{
    engine::target_fps = newMaxFPS;
    SetTargetFPS(newMaxFPS);
}

// Multiplatform exit function
void MP_Exit()
{
    engine::exit_started = true;
}
#endif
#pragma endregion

#pragma region ICFW_DRAWING
#if defined (ICFW_DRAWING)
// simplified sprite based drawing for raylib
// :)

#include "../raylib-6.0/src/raylib.h"

#include <vector>
#include <memory>
#include <map>

enum icfw_npatch_layout
{
    patch_3X3 = 0,
    patch_1X3 = 1,
    patch_3X1 = 2,
};
struct icfw_sprite
{
    Texture2D texture;

    int line_count= 1;
    int row_count = 1;
    int frame_count = 1;

    icfw_npatch_layout layout = patch_3X3;
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
};

struct icfw_drawable
{
    icfw_sprite sprite = icfw_sprite();
    virtual ~icfw_drawable() = default;
    Vector2 location = Vec2(0.0f);
    Vector2 scale = Vec2(1.0f);
    float rotation = 0.0f;
    Color tint = WHITE;
    int frame = 0;
};
// Animates drawable automatically when calling DrawDrawable (also when inside a draw queue when calling DrawQueue), or by manually calling Animate()
struct icfw_animated_drawable : icfw_drawable
{
    int frameRate = 0;
    float lastFrameUpdate = 0.0f;

    virtual void Animate()
    {
        float curTime = GetTime();
        float elapsedTime = curTime - lastFrameUpdate;
        frame = frame + (int)((float)frameRate * elapsedTime) % sprite.frame_count;
        lastFrameUpdate = curTime;
    }
};
// Animated drawable with multiple animations in the same sheet
struct icfw_multi_anim_drawable : icfw_animated_drawable
{
private:
    std::map<std::string, std::vector<int>> animations;
    std::string running_animation = "";
    int sub_frame = 0;
public:
    void AddAnimation(std::string name, std::vector<int> frames)
    {
        animations[name] = frames;
    }
    void SetAnimation(std::string name, int start_frame = 0)
    {
        sub_frame = start_frame;
        running_animation = name;
    }
    std::string GetAnimation()
    {
        return running_animation;
    }
    // Returns which frame of the current animation the drawable is on.
    int GetAnimationFrame()
    {
        return sub_frame;
    }
    void Animate()
    {
        if(running_animation == "")
        {
            icfw_animated_drawable::Animate();
        }
        else
        {
            float curTime = GetTime();
            float elapsedTime = curTime - lastFrameUpdate;
            sub_frame = sub_frame + (int)((float)frameRate * elapsedTime) % animations[running_animation].size(); // sub frame refers to the index in the current animation vector
            frame = animations[running_animation][sub_frame]; //Update the "real" frame
            lastFrameUpdate = curTime;
        }
    }
};

struct icfw_camera
{
    Vector2 location = Vec2(0.0f, 0.0f);
    float zoom = 1.0f;
    float rotation = 0.0f;
};

typedef std::vector<std::shared_ptr<icfw_drawable>> icfw_draw_queue;

namespace engine {
    static icfw_camera default_camera = icfw_camera();
    static icfw_draw_queue default_draw_queue;
}


float GetScreenSizeScaling()
{
    return (GetRenderWidth() / 1920.0f >= GetRenderHeight() / 1080.0f) ? GetRenderWidth() / 1920.0f : GetRenderHeight() / 1080.0f;
}

Vector2 WorldToViewSpace(Vector2 worldPosition, icfw_camera camera = engine::default_camera)
{
    // Convert to camera position, basically relative to the camera
    Vector2 WorkingPosition = worldPosition * Vec2(1, -1) - camera.location * Vec2(1, -1);

    // Zooming
    WorkingPosition = WorkingPosition * camera.zoom;

    // Screen sizing
    // Things scale properly if the window is resized
    WorkingPosition = WorkingPosition * GetScreenSizeScaling();

    // Rotating the position (and going from degrees to radians)
    WorkingPosition = Vector2Rotate(WorkingPosition, camera.rotation * 0.0174533f);

    return WorkingPosition + Vec2(float(GetScreenWidth()) / 2.0f, float(GetScreenHeight()) / 2.0f);
}

void DrawSprite(icfw_sprite sprite, Vector2 location = Vec2(0.0f), float rotation = 0.0f, Vector2 scale = Vec2(1.0f), Color tint = WHITE, int frame = 0, icfw_camera camera = engine::default_camera)
{
    //Rectangle dest = { location.x, location.y, (float)sprite.texture.width*scale.x, (float)sprite.texture.height*scale.y };

    float frame_width = sprite.texture.width / sprite.row_count;
    float frame_height = sprite.texture.height / sprite.line_count;

    Rectangle source = Rectangle();
    source.width = frame_width;
    source.height = frame_height;

    // Scaling to account for multiple frames in a single texture
    float xScale = source.width / sprite.texture.width;
	float yScale = source.height / sprite.texture.height;

    // This could be redone to support differently sorted sprite sheets
    // Currently frames are expected to be sorted in the way shown below
    // 0 1 2
    // 3 4 5
    // 6 7 8
    int x = frame % sprite.row_count;
	int y = frame / sprite.row_count;
	source.x = x * frame_width;
	source.y = y * frame_height;

    NPatchInfo PatchInfo = NPatchInfo();
    PatchInfo.bottom = sprite.bottom;
    PatchInfo.layout = sprite.layout;
    PatchInfo.left = sprite.left;
    PatchInfo.right = sprite.right;
    PatchInfo.source = source;
    PatchInfo.top = sprite.top;

    auto view_space_location = WorldToViewSpace(location, camera);

    Rectangle dest = { view_space_location.x, view_space_location.y, (float)sprite.texture.width*scale.x*xScale*GetScreenSizeScaling()*camera.zoom, (float)sprite.texture.height*scale.y*yScale*GetScreenSizeScaling()*camera.zoom };

    DrawTextureNPatch(
        sprite.texture,
        PatchInfo,
        dest,
        Vec2(dest.width / 2, dest.height / 2),
        rotation,
        tint
    );
}

void DrawDrawable(std::shared_ptr<icfw_drawable> drawable, icfw_camera camera = engine::default_camera)
{
    auto p = drawable.get();
    // Automatically animate drawable.
    if(auto* c = dynamic_cast<icfw_animated_drawable*>(p))
    {
        c->Animate();
    }
    DrawSprite(p->sprite, p->location, p->rotation, p->scale, p->tint, p->frame, camera);
}

void SetDefaultCameraLocation(Vector2 location) { engine::default_camera.location = location; }
Vector2 GetDefaultCameraLocation() { return engine::default_camera.location; }
void SetDefaultCameraZoom(float zoom) { engine::default_camera.zoom = zoom; }
float GetDefaultCameraZoom() { return engine::default_camera.zoom; }
void SetDefaultCameraRotation(float rotation) { engine::default_camera.rotation = rotation; }
float GetDefaultCameraRotation() { return engine::default_camera.rotation; }


// Add sprite to a draw queue
void AddToDrawQueue(icfw_sprite sprite, Vector2 location = Vec2(0.0f), Vector2 scale=Vec2(1.0f), float rotation = 0.0f, Color tint = WHITE, int frame = 0, icfw_draw_queue &target_queue = engine::default_draw_queue)
{
    std::shared_ptr<icfw_drawable> drawable(new icfw_drawable);

    drawable.get()->sprite = sprite;
    drawable.get()->location = location;
    drawable.get()->scale = scale;
    drawable.get()->rotation = rotation;
    drawable.get()->tint = tint;
    drawable.get()->frame = frame;

    target_queue.push_back(drawable);
}
// Add drawable to a draw queue
void AddToDrawQueue(std::shared_ptr<icfw_drawable> drawable, icfw_draw_queue &target_queue = engine::default_draw_queue)
{
    target_queue.push_back(drawable);
}

// Draw all drawables in a draw queue using a camera to a render texture
void DrawQueue(icfw_draw_queue &draw_queue, icfw_camera &camera, RenderTexture2D &render_target)
{
    BeginTextureMode(render_target);

    for (int i = 0; i < (int)draw_queue.size(); i++)
    {
        DrawDrawable(draw_queue[i], camera);
    }

    EndTextureMode();
}
#endif
#pragma endregion