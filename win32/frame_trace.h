/* Optional observations of the game's public frame contract. */
#ifndef WINDOOM_FRAME_TRACE_H
#define WINDOOM_FRAME_TRACE_H
void FrameTrace_Init(const char *path);
void FrameTrace_Record(int used_sr, int used_fsr2);
void FrameTrace_Shutdown(void);
#endif
