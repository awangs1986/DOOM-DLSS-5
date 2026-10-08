/* Copyright (C) 2026 Nikolai Zhivotenko. GPLv2; see LICENSE.TXT. */
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <exception>
#include <vector>
#include "dxr_diagnostic.h"
using Microsoft::WRL::ComPtr;
namespace {
struct Diagnostic {
    ComPtr<ID3D12Device5> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12Resource> vertices, instances, blas, tlas, scratch, output;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    unsigned width = 0, height = 0, row_pitch = 0;
    bool supported = false, enabled = false, disabled = false, ready = false;
    bool first_dispatch = true;
    D3D12_RESOURCE_STATES output_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
} state;

bool failed(HRESULT result, const char *operation)
{
    if (SUCCEEDED(result)) return false;
    std::fprintf(stderr, "DXR: %s failed (0x%08lx); diagnostic disabled, normal present retained\n",
                           operation, static_cast<unsigned long>(result));
    return true;
}
ComPtr<ID3D12Resource> buffer(UINT64 size, D3D12_HEAP_TYPE heap_type,
            D3D12_RESOURCE_STATES initial, D3D12_RESOURCE_FLAGS flags)
{
    ComPtr<ID3D12Resource> resource;
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = heap_type;
    D3D12_RESOURCE_DESC description = {};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = size;
    description.Height = description.DepthOrArraySize = description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = flags;
    if (failed(state.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
             &description, initial, nullptr, IID_PPV_ARGS(&resource)), "buffer allocation"))
        return {};
    return resource;
}
void uav_barrier(ID3D12GraphicsCommandList4 *commands, ID3D12Resource *resource)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    commands->ResourceBarrier(1, &barrier);
}
bool upload(ID3D12Resource *resource, const void *data, size_t size)
{
    void *mapped;
    D3D12_RANGE no_read = { 0, 0 };
    if (failed(resource->Map(0, &no_read, &mapped), "geometry upload map")) return false;
    std::memcpy(mapped, data, size);
    D3D12_RANGE written = { 0, size };
    resource->Unmap(0, &written);
    return true;
}
bool load_pipeline()
{
    wchar_t path[MAX_PATH];
    DWORD path_length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!path_length || path_length >= MAX_PATH) return false;
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash) return false;
    *(slash + 1) = 0;
    if (wcslen(path) + wcslen(L"shaders\\dxr_diagnostic.cso") >= MAX_PATH) return false;
    wcscat_s(path, L"shaders\\dxr_diagnostic.cso");
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "DXR: shader unavailable beside executable; rebuild with DXC; normal present retained\n");
        return false;
    }
    DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 16 * 1024 * 1024) {
        CloseHandle(file);
        std::fprintf(stderr, "DXR: invalid shader size; normal present retained\n");
        return false;
    }
    std::vector<unsigned char> bytes(size);
    DWORD read = 0;
    bool loaded = ReadFile(file, bytes.data(), size, &read, nullptr) && read == size;
    CloseHandle(file);
    if (!loaded) return false;
    D3D12_ROOT_PARAMETER parameters[3] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    parameters[1].Descriptor.ShaderRegister = 0;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[2].Constants.ShaderRegister = 0;
    parameters[2].Constants.Num32BitValues = 3;
    D3D12_ROOT_SIGNATURE_DESC description = {};
    description.NumParameters = 3;
    description.pParameters = parameters;
    ComPtr<ID3DBlob> blob, errors;
    if (failed(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1,
                      &blob, &errors), "root signature serialization")) return false;
    if (failed(state.device->CreateRootSignature(0, blob->GetBufferPointer(),
                blob->GetBufferSize(), IID_PPV_ARGS(&state.root)), "root signature")) return false;
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {};
    pipeline.pRootSignature = state.root.Get();
    pipeline.CS.pShaderBytecode = bytes.data();
    pipeline.CS.BytecodeLength = bytes.size();
    return !failed(state.device->CreateComputePipelineState(&pipeline,
                 IID_PPV_ARGS(&state.pipeline)), "inline ray pipeline creation");
}
bool build_scene()
{
    const float triangle[9] = { -0.75f, -0.65f, 0, 0, 0.75f, 0, 0.75f, -0.65f, 0 };
    state.vertices = buffer(sizeof(triangle), D3D12_HEAP_TYPE_UPLOAD,
                          D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
    if (!state.vertices || !upload(state.vertices.Get(), triangle, sizeof(triangle))) return false;
    D3D12_RAYTRACING_GEOMETRY_DESC geometry = {};
    geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometry.Triangles.VertexCount = 3;
    geometry.Triangles.VertexBuffer.StartAddress = state.vertices->GetGPUVirtualAddress();
    geometry.Triangles.VertexBuffer.StrideInBytes = 3 * sizeof(float);
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blas_inputs = {};
    blas_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blas_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    blas_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    blas_inputs.NumDescs = 1;
    blas_inputs.pGeometryDescs = &geometry;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlas_inputs = {};
    tlas_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlas_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlas_inputs.NumDescs = 1;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blas_size = {}, tlas_size = {};
    state.device->GetRaytracingAccelerationStructurePrebuildInfo(&blas_inputs, &blas_size);
    state.device->GetRaytracingAccelerationStructurePrebuildInfo(&tlas_inputs, &tlas_size);
    if (!blas_size.ResultDataMaxSizeInBytes || !tlas_size.ResultDataMaxSizeInBytes) {
        std::fprintf(stderr, "DXR: acceleration structure prebuild size unavailable\n");
        return false;
    }
    state.blas = buffer(blas_size.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
              D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    state.tlas = buffer(tlas_size.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT,
              D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    state.scratch = buffer(std::max(blas_size.ScratchDataSizeInBytes, tlas_size.ScratchDataSizeInBytes),
              D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    if (!state.blas || !state.tlas || !state.scratch) return false;
    D3D12_RAYTRACING_INSTANCE_DESC instance = {};
    instance.Transform[0][0] = instance.Transform[1][1] = instance.Transform[2][2] = 1;
    instance.InstanceMask = 0xff;
    instance.AccelerationStructure = state.blas->GetGPUVirtualAddress();
    state.instances = buffer(sizeof(instance), D3D12_HEAP_TYPE_UPLOAD,
                             D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
    if (!state.instances || !upload(state.instances.Get(), &instance, sizeof(instance))) return false;
    tlas_inputs.InstanceDescs = state.instances->GetGPUVirtualAddress();
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList4> commands;
    ComPtr<ID3D12Fence> fence;
    if (failed(state.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
          IID_PPV_ARGS(&allocator)), "build allocator") ||
        failed(state.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
          allocator.Get(), nullptr, IID_PPV_ARGS(&commands)), "command list 4") ||
        failed(state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "build fence")) return false;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
    build.Inputs = blas_inputs;
    build.DestAccelerationStructureData = state.blas->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData = state.scratch->GetGPUVirtualAddress();
    commands->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
    uav_barrier(commands.Get(), state.blas.Get());
    uav_barrier(commands.Get(), state.scratch.Get());
    build.Inputs = tlas_inputs;
    build.DestAccelerationStructureData = state.tlas->GetGPUVirtualAddress();
    commands->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
    uav_barrier(commands.Get(), state.tlas.Get());
    if (failed(commands->Close(), "build command close")) return false;
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) return false;
    ID3D12CommandList *lists[] = { commands.Get() };
    state.queue->ExecuteCommandLists(1, lists);
    HRESULT hr = state.queue->Signal(fence.Get(), 1);
    if (SUCCEEDED(hr) && fence->GetCompletedValue() < 1) {
        hr = fence->SetEventOnCompletion(1, event);
        if (SUCCEEDED(hr) && WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0) hr = E_FAIL;
    }
    CloseHandle(event);
    if (failed(hr, "acceleration structure fence") ||
        failed(state.device->GetDeviceRemovedReason(), "acceleration structure execution")) return false;
    std::fprintf(stderr, "DXR: one triangle BLAS and one instance TLAS built and fence completed\n");
    return true;
}
} // namespace
extern "C" void DxrDiag_Init(void *device, void *queue, unsigned width,
                              unsigned height, int requested, int disabled)
{
    state = Diagnostic();
    state.width = width;
    state.height = height;
    state.row_pitch = (width * 4 + 255) & ~255u;
    state.disabled = disabled != 0;
    state.queue = static_cast<ID3D12CommandQueue *>(queue);
    auto base = static_cast<ID3D12Device *>(device);
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options = {};
    D3D12_FEATURE_DATA_SHADER_MODEL shader = { D3D_SHADER_MODEL_6_5 };
    HRESULT ray_hr = base->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options));
    HRESULT shader_hr = base->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shader, sizeof(shader));
    HRESULT interface_hr = base->QueryInterface(IID_PPV_ARGS(&state.device));
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 adapter_description = {};
    bool hardware = SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->EnumAdapterByLuid(base->GetAdapterLuid(), IID_PPV_ARGS(&adapter))) &&
        SUCCEEDED(adapter->GetDesc1(&adapter_description)) &&
        !(adapter_description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
    if (!hardware) std::fprintf(stderr, "DXR: software/unknown adapter is not hardware ray tracing; normal present retained\n");
    state.supported = hardware && SUCCEEDED(ray_hr) && SUCCEEDED(shader_hr) && SUCCEEDED(interface_hr) &&
                      options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1 && shader.HighestShaderModel >= D3D_SHADER_MODEL_6_5;
    std::fprintf(stderr, "DXR: tier=%u SM=0x%x device5=%d hardware=%d support=%d (options=0x%08lx shader=0x%08lx); independent of NGX\n",
        static_cast<unsigned>(options.RaytracingTier), static_cast<unsigned>(shader.HighestShaderModel),
        SUCCEEDED(interface_hr), hardware, state.supported, static_cast<unsigned long>(ray_hr), static_cast<unsigned long>(shader_hr));
    state.enabled = requested && state.supported && !state.disabled;
    if (disabled) std::fprintf(stderr, "DXR: disabled by -nort; normal present retained\n");
    else if (requested && !state.supported) std::fprintf(stderr, "DXR: diagnostic requires DXR 1.1 / SM 6.5 / device5; normal present retained\n");
}
extern "C" void DxrDiag_Toggle(void)
{
    if (!state.supported || state.disabled) {
        std::fprintf(stderr, "DXR: diagnostic toggle unavailable; normal present retained\n");
        return;
    }
    state.enabled = !state.enabled;
    std::fprintf(stderr, "DXR: diagnostic %s\n", state.enabled ? "enabled" : "disabled");
}
extern "C" void DxrDiag_Prepare(void)
{
    if (!state.enabled || state.ready) return;
    bool initialized = false;
    try {
        state.output = buffer(static_cast<UINT64>(state.row_pitch) * state.height,
                      D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        initialized = state.output && load_pipeline() && build_scene();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "DXR: diagnostic initialization exception: %s\n", error.what());
    }
    if (!initialized) {
        state.enabled = false;
        std::fprintf(stderr, "DXR: diagnostic initialization failed; normal present retained\n");
        return;
    }
    state.ready = true;
    std::fprintf(stderr, "DXR: diagnostic ready; green=hardware triangle hit, navy=miss; F5 toggles\n");
}
extern "C" int DxrDiag_Render(void *commands, void *backbuffer)
{
    if (!state.enabled || !state.ready) return 0;
    ComPtr<ID3D12GraphicsCommandList4> list;
    auto base = static_cast<ID3D12GraphicsCommandList *>(commands);
    if (failed(base->QueryInterface(IID_PPV_ARGS(&list)), "frame command list 4") ||
        failed(state.device->GetDeviceRemovedReason(), "frame device status")) {
        state.enabled = false;
        return 0;
    }
    if (state.output_state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = state.output.Get();
        barrier.Transition.StateBefore = state.output_state;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1, &barrier);
    }
    list->SetPipelineState(state.pipeline.Get());
    list->SetComputeRootSignature(state.root.Get());
    list->SetComputeRootShaderResourceView(0, state.tlas->GetGPUVirtualAddress());
    list->SetComputeRootUnorderedAccessView(1, state.output->GetGPUVirtualAddress());
    unsigned dimensions[] = { state.width, state.height, state.row_pitch / 4 };
    list->SetComputeRoot32BitConstants(2, 3, dimensions, 0);
    list->Dispatch((state.width + 7) / 8, (state.height + 7) / 8, 1);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = state.output.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &barrier);
    state.output_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
    D3D12_TEXTURE_COPY_LOCATION source = {}, destination = {};
    source.pResource = state.output.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    source.PlacedFootprint.Footprint.Width = state.width;
    source.PlacedFootprint.Footprint.Height = state.height;
    source.PlacedFootprint.Footprint.Depth = 1;
    source.PlacedFootprint.Footprint.RowPitch = state.row_pitch;
    destination.pResource = static_cast<ID3D12Resource *>(backbuffer);
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    if (state.first_dispatch) {
        std::fprintf(stderr, "DXR: inline RayQuery dispatch recorded (%ux%u); validate fence/timestamp and exported hit/miss pixels\n", state.width, state.height);
        state.first_dispatch = false;
    }
    return 1;
}
extern "C" void DxrDiag_Shutdown(void)
{
    state = Diagnostic();
}
