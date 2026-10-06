/* Projection helpers for the public frame-input contract. GPLv2. */
#include <math.h>
#include "gbuffer.h"

static int gb_project(const GB_FrameInputs *frame, const float world[3], float pixel[2])
{
    const GB_CameraSample *camera = &frame->base;
    float dx = world[0] - camera->position[0], dy = world[2] - camera->position[2];
    float z = dx * camera->forward_cos + dy * camera->forward_sin;
    float left = -dx * camera->forward_sin + dy * camera->forward_cos;
    if (z < 1.0f) return 0;
    pixel[0] = frame->viewport_x + camera->center_x - left * camera->projection / z;
    pixel[1] = frame->viewport_y + camera->center_y - (world[1] - camera->position[1]) * camera->projection / z;
    return isfinite(pixel[0]) && isfinite(pixel[1]);
}

int GB_ProjectMotion(const GB_FrameInputs *current, const GB_FrameInputs *previous,
                     const float current_world[3], const float previous_world[3], float motion[2])
{
    float cur[2], prev[2];
    if (!current || !previous || !current_world || !previous_world || !motion || !gb_project(current, current_world, cur) ||
        !gb_project(previous, previous_world, prev)) return 0;
    motion[0] = prev[0] - cur[0]; motion[1] = prev[1] - cur[1];
    return 1;
}

