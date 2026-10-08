/* Public analytic contract checks; no renderer or GPU stand-in. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gbuffer.h"
static int failures;
static void depth_check(const char *name, float view_depth, int valid, float expected)
{
    float actual = GB_DeviceDepthFromViewDepth(view_depth, valid);
    int ok = isfinite(actual) && fabsf(actual - expected) < 0.000001f;
    printf("%s: %s (%.9f) expected (%.9f)\n", name, ok ? "PASS" : "FAIL", actual, expected);
    if (!ok) failures++;
}
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
    depth_check("device depth near endpoint",1,1,0);
    depth_check("device depth far endpoint",8192,1,1);
    depth_check("device depth reciprocal perspective mapping",2,1,4096.0f/8191);
    depth_check("device depth is not normalized linear view distance",64,1,8064.0f/8191);
    depth_check("positive depth before near clamps near",0.5f,1,0);
    depth_check("depth beyond far clamps far",16384,1,1);
    depth_check("zero invalid depth is far",0,1,1);
    depth_check("negative invalid depth is far",-1,1,1);
    depth_check("non-scene depth is far",64,0,1);
    depth_check("NaN depth is far",NAN,1,1);
    depth_check("infinite depth is far",INFINITY,1,1);
    {
        float values[] = {1,2,8,64,128,512,2048,4096,8192};
        float previous_depth = -1;
        int i, ok = 1;
        for (i = 0; i < (int)(sizeof(values)/sizeof(values[0])); i++) {
            float d = GB_DeviceDepthFromViewDepth(values[i],1);
            double inverse = GB_TEMPORAL_NEAR * GB_TEMPORAL_FAR /
                (GB_TEMPORAL_FAR - d * (GB_TEMPORAL_FAR - GB_TEMPORAL_NEAR));
            if (d <= previous_depth || fabs(inverse-values[i])/values[i] > 0.0003) ok = 0;
            previous_depth = d;
        }
        printf("device depth monotonic and perspective inverse: %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }
    return failures ? 1 : 0;
}
