/* Bounded optional control of an already loaded NR consumer. GPLv2. */
#ifndef NR_CONTROL_H
#define NR_CONTROL_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NrControlStatus {
    int loaded;
    int supported;
    int confirmed; /* -1 unknown, 0 original checkbox off, 1 checkbox on */
    int pending;
    int execution_verified; /* Never inferred from checkbox or carrier. */
    unsigned epoch;
    const char *reason; /* Stable static graphics.reason.* key. */
} NrControlStatus;
/* Request is CPU only. Poll and Shutdown belong to the renderer's safe-frame
 * thread, after its GPU fence and outside ReShade callbacks. No borrowed
 * backend objects are returned. epoch changes invalidate prior carrier work. */
void NrControl_Request(int enabled);
void NrControl_Poll(void);
NrControlStatus NrControl_GetStatus(void);
void NrControl_Shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
