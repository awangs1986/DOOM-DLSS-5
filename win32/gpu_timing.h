/* Opt-in GPU measurements. Caller owns command-list sequencing and fencing. */
#ifndef WINDOOM_GPU_TIMING_H
#define WINDOOM_GPU_TIMING_H
#include <d3d12.h>
void GpuTiming_Init(ID3D12Device *device, ID3D12CommandQueue *queue,
                    const char *csv_path, int export_timeline);
void GpuTiming_Begin(ID3D12GraphicsCommandList *commands, int game_tic);
void GpuTiming_Mark(ID3D12GraphicsCommandList *commands, unsigned stage);
/* Called after optional point lighting, before any upscaler. */
void GpuTiming_LightingEnd(ID3D12GraphicsCommandList *commands);
void GpuTiming_End(ID3D12GraphicsCommandList *commands);
/* Call only after the queue fence covering End has completed. */
void GpuTiming_Collect(void);
void GpuTiming_Shutdown(void);
#endif
