/* Ordinary scene lighting. Renderer fences before Prepare/config/shutdown.
   Game/menu callers only request intent and read portable status. GPLv2. */
#ifndef WINDOOM_DXR_LIGHTING_H
#define WINDOOM_DXR_LIGHTING_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int requested, active, available;
    const char *reason;
} DxrLightingStatus;
void DxrLighting_Init(void *device, unsigned output_width, unsigned output_height);
void DxrLighting_RequestEnabled(int enabled);
DxrLightingStatus DxrLighting_GetStatus(void);
void DxrLighting_Prepare(void);
/* Destination scene texture must be R8G8B8A8 and COPY_DEST. */
int DxrLighting_Evaluate(void *commands, void *scene_texture);
/* Same evaluated scene, nearest BGRA expansion; caller adds existing overlays. */
int DxrLighting_PresentNearest(void *commands, void *backbuffer);
void DxrLighting_Shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
