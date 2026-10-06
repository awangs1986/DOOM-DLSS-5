#ifndef WINDOOM_RENDER_ADAPTER_H
#define WINDOOM_RENDER_ADAPTER_H
#include <d3d12.h>
#include <dxgi1_4.h>
HRESULT Render_CreateDevice(IDXGIFactory4 *factory, ID3D12Device **device);
#endif
