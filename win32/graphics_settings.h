/* Requested graphics policy and profile preferences. No GPU ownership. GPLv2. */
#ifndef WINDOOM_GRAPHICS_SETTINGS_H
#define WINDOOM_GRAPHICS_SETTINGS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { GRAPHICS_RT, GRAPHICS_SR, GRAPHICS_NR, GRAPHICS_COUNT };
enum { GRAPHICS_OFF, GRAPHICS_PENDING, GRAPHICS_ACTIVE, GRAPHICS_UNAVAILABLE,
       GRAPHICS_FALLBACK, GRAPHICS_UNVERIFIED, GRAPHICS_UNKNOWN, GRAPHICS_PAUSED };
typedef struct { int requested, state; const char *reason; unsigned epoch; } GraphicsStatus;
/* Profile is the resolved existing defaultfile/-config identity; caller owns mode
   defaults and explicit CLI overrides. No startup write, no exit-time write. */
void Graphics_Init(const char *profile, unsigned defaults, unsigned cli_mask, unsigned cli_values);
GraphicsStatus Graphics_Get(int feature);
const char *Graphics_StateKey(int state);
void Graphics_SetRequested(int feature, int enabled); /* menu edit: persist only that preference */
unsigned Graphics_TakePending(void); /* renderer safe frame only */
void Graphics_SetActual(int feature, int state, const char *reason);
const char *Graphics_PreferenceReason(void);
const char *Graphics_ProfilePath(void);
/* Bounded transactional public parser; presence is separate from default values. */
int Graphics_DefaultResource(const char *name, char *path, size_t capacity);
int Graphics_ReadDefaultLight(float values[8]);
int Graphics_Parse(const void *data, size_t length, unsigned *presence, unsigned *values);
#ifdef __cplusplus
}
#endif
#endif
