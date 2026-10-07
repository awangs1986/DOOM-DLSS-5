/* Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT. */
#include <stdio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "gbuffer.h"
#include "frame_trace.h"
static FILE *trace;
void FrameTrace_Shutdown(void) { if (trace) fclose(trace); trace = NULL; }
void FrameTrace_Init(const char *path)
{
    int fd;
    FrameTrace_Shutdown();
    fd = _open(path, _O_CREAT | _O_EXCL | _O_WRONLY | _O_TEXT, _S_IREAD | _S_IWRITE);
    if (fd >= 0) { trace = _fdopen(fd, "w"); if (!trace) _close(fd); }
    if (!trace) { fprintf(stderr, "frame-inputs: cannot create new trace; rendering continues\n"); return; }
    fprintf(trace, "frame,game_tic,scene,history,reset,fixed,delta_ms,used_sr,vx,vy,vw,vh,base_x,base_height,base_y,base_yaw,sampled_yaw,cx,cy,projection,jitter_x,jitter_y,sample_x,sample_y,valid,kind,depth,world_x,world_height,world_y,mv_x,mv_y,normal_x,normal_y,normal_z,velocity_max,velocity_nonfinite,used_fsr2,map_episode,map_number,device_depth,device_min,device_max,device_invalid,invalid_far_mismatch\n");
    fprintf(stderr, "frame-inputs: current-to-previous jitter-free 320x200 pixels; Y-up world; trace=%s\n", path);
}
void FrameTrace_Record(int used_sr, int used_fsr2)
{
    const GB_FrameInputs *f = GB_GetFrameInputs();
    const float *velocity = GB_VelocityRG();
    const unsigned char *normal = GB_NormalRGBA();
    const float *device_depth = GB_TemporalDepth();
    const unsigned char *scene_mask = GB_SceneMask();
    int i, x = f->viewport_x + f->viewport_width / 2, y = f->viewport_y + f->viewport_height / 2;
    int valid, nonfinite = 0;
    float world[3] = {0}, maximum = 0;
    float device_min = 1.0f, device_max = 0.0f;
    int device_invalid = 0, invalid_far_mismatch = 0;
    if (!trace) return;
    for (i = 0; i < GB_WIDTH * GB_HEIGHT * 2; i++) {
        if (!isfinite(velocity[i])) nonfinite++;
        else if (fabsf(velocity[i]) > maximum) maximum = fabsf(velocity[i]);
    }
    valid = GB_SampleWorldPosition(x, y, world);
    for (i = 0; i < GB_WIDTH * GB_HEIGHT; i++) {
        float d = device_depth[i];
        if (!isfinite(d) || d < 0.0f || d > 1.0f) device_invalid++;
        else { if (d < device_min) device_min = d; if (d > device_max) device_max = d; }
        if ((!f->scene_valid || !scene_mask[i]) && d != 1.0f) invalid_far_mismatch++;
    }
    i = ((unsigned)x < GB_WIDTH && (unsigned)y < GB_HEIGHT) ? y * GB_WIDTH + x : 0;
    fprintf(trace, "%u,%d,%d,%d,%u,%d,%.6f,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%u,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%u,%u,%u,%.6f,%d,%d,%d,%d,%.9f,%.9f,%.9f,%d,%d\n",
            f->frame_id, f->game_tic, f->scene_valid, f->history_valid, f->reset_reasons, f->fixed_timeline, f->frame_delta_ms, used_sr,
            f->viewport_x,f->viewport_y,f->viewport_width,f->viewport_height,f->base.position[0],f->base.position[1],f->base.position[2],f->base.yaw,f->sampled.yaw,
            f->base.center_x,f->base.center_y,f->base.projection,f->jitter_x,f->jitter_y,x,y,valid,GB_SurfaceKind()[i],GB_Depth()[i],world[0],world[1],world[2],velocity[i*2],velocity[i*2+1],normal[i*4],normal[i*4+1],normal[i*4+2],maximum,nonfinite,used_fsr2,f->map_episode,f->map_number,device_depth[i],device_min,device_max,device_invalid,invalid_far_mismatch);
    fflush(trace);
}
