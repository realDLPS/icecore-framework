#pragma once

#include "../raylib-6.0/src/raylib.h"

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

namespace engine {
void internal_tick(float delta_time)
{
    #if defined(ICFW_INPUT)
    #endif
}
}

#if defined(PLATFORM_WEB)
namespace engine {
void web_tick(void)
{
    double delta = emscripten_get_now() - last_frame_time;
    engine::internal_tick(float(delta) / 1000.f)

    int frame_width = 0;
    int frame_height = 0;
    emscripten_get_screen_size(&frame_width, &frame_height);
    SetWindowSize(std::min(target_width, frame_width), std::min(target_height, frame_height));

    web_tick_func(float(delta) / 1000.f);
    last_frame_time += delta;
}
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
        engine::internal_tick(GetFrameTime());
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





#pragma region ICFW_INPUT
#if defined(ICFW_INPUT)
#include <vector>

enum icfw_input_mode
{
    disabled = 1,
    game = 2,
#if defined(ICFW_UI)
    ui = 3,
    game_ui = 4,
    ui_game = 5,
#endif
};


enum icfw_input_value_type
{
    digital = 1,
    single_axis = 2,
};

enum icfw_mouse_inputs
{
    mouse_left = 1,
    mouse_right = 2,
    mouse_middle = 3,
    mouse_forward = 4,
    mouse_back = 5,
    mouse_side = 6,
    mouse_extra = 7,
    mouse_scroll = 8,
    mouse_x = 9,
    mouse_y = 10,
};
namespace engine{
    // Mappings of icfw mouse inputs to raylib mouse inputs
    int mouse_mapping[7] = {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_MIDDLE_BUTTON, MOUSE_BUTTON_FORWARD, MOUSE_BUTTON_BACK, MOUSE_BUTTON_SIDE, MOUSE_BUTTON_EXTRA};
}

enum icfw_gamepad_inputs
{
    gamepad_button_unknown = 1,
    gamepad_button_left_face_up = 2,
    gamepad_button_left_face_right = 3,
    gamepad_button_left_face_down = 4,
    gamepad_button_left_face_left = 5,
    gamepad_button_right_face_up = 6,
    gamepad_button_right_face_right = 7,
    gamepad_button_right_face_down = 8,
    gamepad_button_right_face_left = 9,
    gamepad_button_left_trigger_1 = 10,
    gamepad_button_left_trigger_2 = 11,
    gamepad_button_right_trigger_1 = 12,
    gamepad_button_right_trigger_2 = 13,
    gamepad_button_middle_left = 14,
    gamepad_button_middle = 15,
    gamepad_button_middle_right = 16,
    gamepad_button_left_thumb = 17,
    gamepad_button_right_thumb = 18,
    gamepad_axis_left_x = 19,
    gamepad_axis_left_y = 20,
    gamepad_axis_right_x = 21,
    gamepad_axis_right_y = 22,
    gamepad_axis_left_trigger = 23,
    gamepad_axis_right_trigger = 24,
};
namespace engine{
    // Mappings of icfw gamepad inputs to raylib gamepad inputs
    int gamepad_mapping[24] = {GAMEPAD_BUTTON_UNKNOWN, GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_RIGHT, GAMEPAD_BUTTON_LEFT_FACE_DOWN, GAMEPAD_BUTTON_LEFT_FACE_LEFT, GAMEPAD_BUTTON_RIGHT_FACE_UP, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_LEFT, GAMEPAD_BUTTON_LEFT_TRIGGER_1, GAMEPAD_BUTTON_LEFT_TRIGGER_2, GAMEPAD_BUTTON_RIGHT_TRIGGER_1, GAMEPAD_BUTTON_RIGHT_TRIGGER_2, GAMEPAD_BUTTON_MIDDLE_LEFT, GAMEPAD_BUTTON_MIDDLE, GAMEPAD_BUTTON_MIDDLE_RIGHT, GAMEPAD_BUTTON_LEFT_THUMB, GAMEPAD_BUTTON_RIGHT_THUMB, GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y, GAMEPAD_AXIS_RIGHT_X, GAMEPAD_AXIS_RIGHT_Y, GAMEPAD_AXIS_LEFT_TRIGGER, GAMEPAD_AXIS_RIGHT_TRIGGER};
}

struct icfw_input_trigger
{
    int index = 0;
    float multiplier = 1.0f;
    virtual float GetValue()
    {
        return 0.0f;
    }
};
struct icfw_keyboard_trigger : icfw_input_trigger
{
    const icfw_keyboard_trigger KEYBOARD_TRIGGER(KeyboardKey key, float multiplier = 1.0f)
    {
        auto a = icfw_keyboard_trigger();
        a.index = key;
        a.multiplier = multiplier;
        return a;
    }
    float GetValue()
    {
        return IsKeyDown(index) ? multiplier : 0.0f;
    }
};
struct icfw_mouse_trigger : icfw_input_trigger
{
    const icfw_mouse_trigger MOUSE_TRIGGER(icfw_mouse_inputs mouse_input, float multiplier = 1.0f)
    {
        auto a = icfw_mouse_trigger();
        a.index = mouse_input;
        a.multiplier = multiplier;
        return a;
    }
    float GetValue()
    {
        if(index < 8)
        {
            return IsMouseButtonDown(engine::mouse_mapping[index]) ? multiplier : 0.0f;
        }
        else if(index == 8)
        {
            return GetMouseWheelMove() * multiplier;
        }
        else if(index == 9)
        {
            return GetMouseDelta().x * multiplier; 
        }
        else if(index == 10)
        {
            return GetMouseDelta().y * multiplier; 
        }
        return 0.0f;
    }
};
struct icfw_gamepad_trigger : icfw_input_trigger
{
    const icfw_gamepad_trigger GAMEPAD_TRIGGER(icfw_gamepad_inputs gamepad_input, float multiplier = 1.0f)
    {
        auto a = icfw_gamepad_trigger();
        a.index = gamepad_input;
        a.multiplier = multiplier;
        return a;
    }
    float GetValue()
    {
        if(!IsGamepadAvailable(0))
        {
            return 0.0f;
        }
        if(index < 19)
        {
            return IsGamepadButtonDown(0, engine::gamepad_mapping[index]) ? multiplier : 0.0f;
        }
        else
        {
            return GetGamepadAxisMovement(0, engine::gamepad_mapping[index]) * multiplier;
        }
    }
};

struct icfw_input_action
{
    icfw_input_value_type value_type = digital;
    std::vector<icfw_input_trigger> triggers;
    bool consume_input = true;
};

struct icfw_input_mapping
{
    std::vector<icfw_input_action> actions;
};
#endif
#pragma endregion