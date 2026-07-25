// simplified sprite based drawing for raylib
// :)

#include "../../raylib-6.0/src/raylib.h"
#include "../math/vector.hpp"

#include <vector>


struct icfw_sprite
{
    Texture2D texture;

    int line_count= 1;
    int row_count = 1;
    int frame_count = 1; 
};

enum icfw_npatch_layout
{
    patch_3X3 = 0,
    patch_1X3 = 1,
    patch_3X1 = 2,
};
// Sprite with additional NPatch support
struct icfw_sprite_patch : icfw_sprite
{
    icfw_npatch_layout layout = patch_3X3;
    int left;
    int top;
    int right;
    int bottom;
};
struct icfw_camera
{
    Vector2 location = Vec2(0.0f, 0.0f);
    float zoom = 1.0f;
    float rotation = 0.0f;
};

static icfw_camera default_camera = icfw_camera();

float GetScreenSizeScaling()
{
    return (GetRenderWidth() / 1920.0f >= GetRenderHeight() / 1080.0f) ? GetRenderWidth() / 1920.0f : GetRenderHeight() / 1080.0f;
}

Vector2 WorldToViewSpace(Vector2 worldPosition)
{
    // Convert to camera position, basically relative to the camera
    Vector2 WorkingPosition = worldPosition * Vec2(1, -1) - default_camera.location * Vec2(1, -1);

    // Zooming
    WorkingPosition = WorkingPosition * default_camera.zoom;

    // Screen sizing
    // Things scale properly if the window is resized
    WorkingPosition = WorkingPosition * GetScreenSizeScaling();

    // Rotating the position (and going from degrees to radians)
    WorkingPosition = Vector2Rotate(WorkingPosition, default_camera.rotation * 0.0174533f);

    return WorkingPosition + Vec2(float(GetScreenWidth()) / 2.0f, float(GetScreenHeight()) / 2.0f);
}

void DrawSprite(icfw_sprite_patch sprite, Vector2 location, float rotation, Vector2 scale, Color tint, int frame)
{
    Rectangle dest = { location.x, location.y, (float)sprite.texture.width*scale.x, (float)sprite.texture.height*scale.y };

    float frame_width = sprite.texture.width / sprite.row_count;
    float frame_height = sprite.texture.height / sprite.line_count;

    Rectangle source = Rectangle();
    source.width = frame_width;
    source.height = frame_height;

    // Scaling to account for multiple frames in a single texture
    float xScale = source.width / sprite.texture.width;
	float yScale = source.height / sprite.texture.height;

    // This can be redone to support differently sorted sprite sheets
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

    auto view_space_location = WorldToViewSpace(location);

    Rectangle dest = { view_space_location.x, view_space_location.y, (float)sprite.texture.width*scale.x, (float)sprite.texture.height*scale.y };

    DrawTextureNPatch(
        sprite.texture,
        PatchInfo,
        dest,
        Vec2(dest.width / 2, dest.height / 2),
        rotation,
        tint
    );
}




void SetDefaultCameraLocation(Vector2 location) { default_camera.location = location; }
Vector2 GetDefaultCameraLocation() { return default_camera.location; }
void SetDefaultCameraZoom(float zoom) { default_camera.zoom = zoom; }
float GetDefaultCameraZoom() { return default_camera.zoom; }
void SetDefaultCameraRotation(float rotation) { default_camera.rotation = rotation; }
float GetDefaultCameraRotation() { return default_camera.rotation; }

typedef std::vector<icfw_sprite> icfw_draw_queue;
static icfw_draw_queue default_draw_queue;
// Add sprite to a draw queue
void AddToDrawQueue(icfw_sprite sprite, icfw_draw_queue &target_queue)
{
    target_queue.push_back(sprite);
}
// Add sprite to the default draw queue
void AddToDrawQueue(icfw_sprite sprite)
{
    AddToDrawQueue(sprite, default_draw_queue);
}

// Draw all sprites in a draw queue using a camera to a render texture
void DrawQueue(icfw_draw_queue &draw_queue, icfw_camera &camera, RenderTexture2D &render_target)
{
    BeginTextureMode(render_target);

    for (int i = 0; i < draw_queue.size(); i++)
    {
        DrawTexturePro
    }

    EndTextureMode();
}