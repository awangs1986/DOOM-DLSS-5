/* Runtime identity diagnostics. No NGX API or rendering ownership. */
#ifndef WINDOOM_NGX_RUNTIME_H
#define WINDOOM_NGX_RUNTIME_H
#include <stddef.h>
int NgxRuntime_Prepare(const wchar_t *directory);
void NgxRuntime_ReportLoaded(const char *stage);
int NgxRuntime_AddonLoaded(void);
void NgxRuntime_Reset(void);
#endif
