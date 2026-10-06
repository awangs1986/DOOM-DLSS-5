/* Public analytic contract checks; no renderer or GPU stand-in. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gbuffer.h"
static int failures;
static void check(const char *name, const GB_FrameInputs *current, const GB_FrameInputs *previous,
                  const float world[3], float x, float y)
{
    float motion[2] = {0};
    int ok = GB_ProjectMotion(current, previous, world, world, motion) &&
             fabsf(motion[0] - x) < 0.0001f && fabsf(motion[1] - y) < 0.0001f;
    printf("%s: %s (%.6f, %.6f) expected (%.6f, %.6f)\n", name, ok ? "PASS" : "FAIL",motion[0],motion[1],x,y);
    if (!ok) failures++;
}
int main(void)
{
    GB_FrameInputs previous, current;
    float world[3] = {64,16,0};
    memset(&previous,0,sizeof(previous));
    previous.base.forward_cos=1; previous.base.projection=160;
    previous.base.center_x=160; previous.base.center_y=100;
    previous.viewport_x=16; previous.viewport_y=8;
    current=previous;
    check("static world sample with nonzero viewport",&current,&previous,world,0,0);
    current.sampled.yaw=123456; current.jitter_x=0.375f;current.jitter_y=-0.25f;
    check("static jitter removed from motion",&current,&previous,world,0,0);
    current=previous;current.base.position[2]=4;
    check("camera left: current to previous negative x pixels",&current,&previous,world,-10,0);
    current=previous;current.base.position[1]=4;
    check("camera up: current to previous negative y pixels",&current,&previous,world,0,-10);
    current=previous;current.viewport_x=32;current.viewport_y=12;
    check("full buffer viewport offsets",&current,&previous,world,-16,-4);
    current=previous;current.base.forward_cos=(float)sqrt(0.5);current.base.forward_sin=(float)sqrt(0.5);
    check("45 degree left turn",&current,&previous,world,-160,(float)(40*sqrt(2.0)-40));
    current=previous;current.base.position[0]=8;world[2]=16;
    check("forward translation includes depth",&current,&previous,world,160*16.0f/56-40,160*16.0f/56-40);
    return failures ? 1 : 0;
}
