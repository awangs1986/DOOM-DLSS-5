/* Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <stdlib.h>
#include "render_adapter.h"
#include "m_argv.h"

HRESULT Render_CreateDevice(IDXGIFactory4 *factory, ID3D12Device **device)
{
    IDXGIAdapter1 *chosen = NULL;
    int selected = -1, best_score = -1, explicit_index = -1;
    UINT index;
    HRESULT hr;
    int argument = M_CheckParm("-adapter");
    if (argument) {
        char *end;
        long parsed;
        if (argument >= myargc - 1) {
            fprintf(stderr, "adapter: -adapter requires a DXGI index\n");
            return E_INVALIDARG;
        }
        parsed = strtol(myargv[argument + 1], &end, 10);
        if (!*myargv[argument + 1] || *end || parsed < 0 || parsed > 63) {
            fprintf(stderr, "adapter: invalid index %s\n", myargv[argument + 1]);
            return E_INVALIDARG;
        }
        explicit_index = (int)parsed;
    }
    for (index = 0; index < 64; index++) {
        IDXGIAdapter1 *adapter = NULL;
        DXGI_ADAPTER_DESC1 desc;
        char name[512] = "unknown";
        int score, supports_d3d12;
        hr = IDXGIFactory4_EnumAdapters1(factory, index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) break;
        if (FAILED(IDXGIAdapter1_GetDesc1(adapter, &desc))) {
            IDXGIAdapter1_Release(adapter);
            continue;
        }
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), NULL, NULL);
        supports_d3d12 = SUCCEEDED(D3D12CreateDevice((IUnknown *)adapter,
                     D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL));
        fprintf(stderr, "adapter: [%u] %s vendor=0x%04x device=0x%04x software=%d d3d12=%d\n",
                index, name, desc.VendorId, desc.DeviceId,
                !!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE), supports_d3d12);
        score = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? 0 :
                   (desc.VendorId == 0x10de ? 100 : 50);
        if ((explicit_index == (int)index) ||
            (explicit_index < 0 && supports_d3d12 && score > best_score)) {
            if (chosen) IDXGIAdapter1_Release(chosen);
            chosen = adapter;
            adapter = NULL;
            selected = (int)index;
            best_score = score;
        }
        if (adapter) IDXGIAdapter1_Release(adapter);
    }
    if (!chosen) {
        fprintf(stderr, "adapter: requested/default adapter unavailable; no silent override\n");
        return DXGI_ERROR_NOT_FOUND;
    }
    hr = D3D12CreateDevice((IUnknown *)chosen, D3D_FEATURE_LEVEL_11_0,
                           &IID_ID3D12Device, (void **)device);
    fprintf(stderr, "adapter: selected [%d] (%s), device result=0x%08lx\n", selected,
                   explicit_index >= 0 ? "explicit" : "NVIDIA preferred", (unsigned long)hr);
    IDXGIAdapter1_Release(chosen);
    return hr;
}
