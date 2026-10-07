/* Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include "gpu_timing.h"

#define TIMESTAMP_COUNT 8
static ID3D12QueryHeap *queries;
static ID3D12Resource *readback;
static IDXGIAdapter3 *adapter;
static FILE *csv;
static UINT64 frequency, frame;
static int pending, timeline, measured_game_tic;

void GpuTiming_Shutdown(void)
{
    if (csv) fclose(csv);
    if (adapter) IDXGIAdapter3_Release(adapter);
    if (readback) ID3D12Resource_Release(readback);
    if (queries) ID3D12QueryHeap_Release(queries);
    csv = NULL;
    adapter = NULL;
    readback = NULL;
    queries = NULL;
    frequency = frame = 0;
    pending = 0;
}

void GpuTiming_Init(ID3D12Device *device, ID3D12CommandQueue *queue,
                    const char *csv_path, int export_timeline)
{
    D3D12_QUERY_HEAP_DESC query_desc;
    D3D12_HEAP_PROPERTIES heap;
    D3D12_RESOURCE_DESC buffer;
    IDXGIFactory4 *factory = NULL;
    HANDLE file;
    int fd;
    if (!csv_path) return;
    GpuTiming_Shutdown();
    if (FAILED(ID3D12CommandQueue_GetTimestampFrequency(queue, &frequency)) || !frequency)
        goto unavailable;
    memset(&query_desc, 0, sizeof(query_desc));
    query_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    query_desc.Count = TIMESTAMP_COUNT;
    if (FAILED(ID3D12Device_CreateQueryHeap(device, &query_desc,
                   &IID_ID3D12QueryHeap, (void **)&queries))) goto unavailable;
    memset(&heap, 0, sizeof(heap));
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    memset(&buffer, 0, sizeof(buffer));
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = TIMESTAMP_COUNT * sizeof(UINT64);
    buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(ID3D12Device_CreateCommittedResource(device, &heap,
          D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
          &IID_ID3D12Resource, (void **)&readback))) goto unavailable;
    if (SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory4, (void **)&factory))) {
        LUID luid;
        ID3D12Device_GetAdapterLuid(device, &luid);
        IDXGIFactory4_EnumAdapterByLuid(factory, luid, &IID_IDXGIAdapter3, (void **)&adapter);
        IDXGIFactory4_Release(factory);
        if (adapter) {
            DXGI_ADAPTER_DESC1 description;
            char name[512];
            if (SUCCEEDED(IDXGIAdapter3_GetDesc1(adapter, &description)) &&
                WideCharToMultiByte(CP_UTF8, 0, description.Description, -1,
                                    name, sizeof(name), NULL, NULL))
                fprintf(stderr, "gpu-timing: render adapter %s (vendor 0x%04x, device 0x%04x)\n",
                        name, description.VendorId, description.DeviceId);
        }
    }
    /* Do not replace an existing report, even if a user accidentally reuses it. */
    file = CreateFileA(csv_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto unavailable;
    fd = _open_osfhandle((intptr_t)file, _O_TEXT);
    if (fd < 0) { CloseHandle(file); goto unavailable; }
    csv = _fdopen(fd, "w");
    if (!csv) { _close(fd); goto unavailable; }
    timeline = export_timeline;
    fprintf(csv, "frame,game_tic,export_sequence_seconds,gpu_ms,upload_ms,sr_ms,compose_ms,export_copy_ms,local_memory_bytes,local_budget_bytes,rt_ms,reflection_trace_ms,reflection_filter_ms\n");
    fflush(csv);
    fprintf(stderr, "gpu-timing: D3D12 timestamps (%llu ticks/s); CSV -> %s\n",
              (unsigned long long)frequency, csv_path);
    return;
unavailable:
    fprintf(stderr, "gpu-timing: unavailable (query/readback/output creation failed); rendering continues\n");
    GpuTiming_Shutdown();
}

void GpuTiming_Begin(ID3D12GraphicsCommandList *commands, int game_tic)
{
    if (!csv) return;
    frame++;
    measured_game_tic = game_tic;
    ID3D12GraphicsCommandList_EndQuery(commands, queries, D3D12_QUERY_TYPE_TIMESTAMP, 0);
}
void GpuTiming_Mark(ID3D12GraphicsCommandList *commands, unsigned stage)
{
    if (csv && stage > 0 && stage <= 3)
        ID3D12GraphicsCommandList_EndQuery(commands, queries, D3D12_QUERY_TYPE_TIMESTAMP, stage == 1 ? 1 : stage + 3);
}
void GpuTiming_LightingEnd(ID3D12GraphicsCommandList *commands)
{
    if (csv) ID3D12GraphicsCommandList_EndQuery(commands, queries, D3D12_QUERY_TYPE_TIMESTAMP, 2);
}
void GpuTiming_ReflectionTraceEnd(ID3D12GraphicsCommandList *commands)
{
    if (csv) ID3D12GraphicsCommandList_EndQuery(commands, queries, D3D12_QUERY_TYPE_TIMESTAMP, 3);
}
void GpuTiming_ReflectionEnd(ID3D12GraphicsCommandList *commands)
{
    if (csv) ID3D12GraphicsCommandList_EndQuery(commands, queries, D3D12_QUERY_TYPE_TIMESTAMP, 4);
}
void GpuTiming_End(ID3D12GraphicsCommandList *commands)
{
    if (!csv) return;
    ID3D12GraphicsCommandList_EndQuery(commands, queries, D3D12_QUERY_TYPE_TIMESTAMP, TIMESTAMP_COUNT - 1);
    ID3D12GraphicsCommandList_ResolveQueryData(commands, queries,
                  D3D12_QUERY_TYPE_TIMESTAMP, 0, TIMESTAMP_COUNT, readback, 0);
    pending = 1;
}
void GpuTiming_Collect(void)
{
    UINT64 *ticks;
    D3D12_RANGE read_range = { 0, TIMESTAMP_COUNT * sizeof(UINT64) };
    D3D12_RANGE written = { 0, 0 };
    DXGI_QUERY_VIDEO_MEMORY_INFO memory;
    double ms;
    int i, memory_available = 0;
    if (!csv || !pending) return;
    ms = 1000.0 / (double)frequency;
    pending = 0;
    if (FAILED(ID3D12Resource_Map(readback, 0, &read_range, (void **)&ticks))) {
        fprintf(stderr, "gpu-timing: readback failed; disabling measurements\n");
        GpuTiming_Shutdown();
        return;
    }
    for (i = 1; i < TIMESTAMP_COUNT; i++) {
        if (ticks[i] < ticks[i-1]) {
            ID3D12Resource_Unmap(readback, 0, &written);
            fprintf(stderr, "gpu-timing: invalid timestamps; disabling measurements\n");
            GpuTiming_Shutdown();
            return;
        }
    }
    if (adapter)
        memory_available = SUCCEEDED(IDXGIAdapter3_QueryVideoMemoryInfo(adapter, 0,
                               DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory));
    fprintf(csv, "%llu,%d,", (unsigned long long)frame, measured_game_tic);
    if (timeline) fprintf(csv, "%.9f", (double)(frame - 1) / 35.0);
    fprintf(csv, ",%.6f,%.6f,%.6f,%.6f,%.6f,", (ticks[7] - ticks[0]) * ms,
               (ticks[1] - ticks[0]) * ms, (ticks[5] - ticks[4]) * ms,
               (ticks[6] - ticks[5]) * ms, (ticks[7] - ticks[6]) * ms);
    if (memory_available) fprintf(csv, "%llu,%llu", (unsigned long long)memory.CurrentUsage,
                                                (unsigned long long)memory.Budget);
    else fprintf(csv, ",");
    fprintf(csv, ",%.6f,%.6f,%.6f", (ticks[2] - ticks[1]) * ms, (ticks[3] - ticks[2]) * ms, (ticks[4] - ticks[3]) * ms);
    fputc('\n', csv);
    fflush(csv);
    ID3D12Resource_Unmap(readback, 0, &written);
}
