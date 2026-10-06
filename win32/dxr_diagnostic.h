/* DXR capability tool, independent of NGX. Caller fences shared queue before
   Prepare/Shutdown and owns the backbuffer COPY_DEST transition. */
#ifndef WINDOOM_DXR_DIAGNOSTIC_H
#define WINDOOM_DXR_DIAGNOSTIC_H
#ifdef __cplusplus
extern "C" {
#endif
void DxrDiag_Init(void *device, void *queue, unsigned width, unsigned height,
                  int requested, int disabled);
void DxrDiag_Toggle(void);
void DxrDiag_Prepare(void);
int DxrDiag_Render(void *commands, void *backbuffer);
void DxrDiag_Shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
