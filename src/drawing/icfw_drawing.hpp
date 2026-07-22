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

void DrawSprite(icfw_sprite_patch sprite, Vector2 location, float rotation, Vector2 scale, Color tint, int frame)
{
    Rectangle dest = { location.x, location.y, (float)sprite.texture.width*scale.x, (float)sprite.texture.height*scale.y };

    float frame_width = sprite.texture.width / sprite.row_count;
    float frame_height = sprite.texture.height / sprite.line_count;

    Rectangle source = Rectangle();
    source.width = frame_width;
    source.height = frame_height;


}

struct icfw_camera
{
    Vector2 location = Vec2(0.0f, 0.0f);
    float zoom = 1.0f;
    float rotation = 0.0f;
};

static icfw_camera default_camera = icfw_camera();
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