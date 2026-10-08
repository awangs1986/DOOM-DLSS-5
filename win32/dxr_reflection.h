/* One deterministic map bounce. Renderer fences before Prepare/shutdown. GPLv2. */
#ifndef WINDOOM_DXR_REFLECTION_H
#define WINDOOM_DXR_REFLECTION_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int requested,active,available; const char *reason; } DxrReflectionStatus;
void DxrReflection_Init(void *device,unsigned output_width,unsigned output_height);
void DxrReflection_RequestEnabled(int enabled);
DxrReflectionStatus DxrReflection_GetStatus(void);
void DxrReflection_Prepare(void);
int DxrReflection_Evaluate(void *commands,void *scene_texture);
int DxrReflection_PresentNearest(void *commands,void *backbuffer);
/* Conservative global reset while any reflective receiver is visible. */
int DxrReflection_NeedsHistoryReset(void);
/* Records the renderer's actual current temporal argument and successful path.
 * Frozen GB frame inputs describe capture time, before reflection evaluation. */
void DxrReflection_RecordTemporalEvaluation(int reset,int ngx_evaluated,int fsr_evaluated);
void DxrReflection_Shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
