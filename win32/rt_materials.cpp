/* Passive bounded atlas shared by later alpha/reflection. GPLv2. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <set>
#include <map>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include "rt_materials.h"
#include "r_material.h"
using Microsoft::WRL::ComPtr;
namespace {
constexpr size_t descriptor_budget=4096,pixel_budget=16*1024*1024;
struct State {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Resource> descriptions,atlas;
    std::vector<RtMaterialDescriptor> descriptors;
    std::vector<uint32_t> pixels;
    RtMaterialView view={};
    uint64_t failed_generation=0;
} state;
ComPtr<ID3D12Resource> copied_buffer(const void *data,size_t size) {
    ComPtr<ID3D12Resource> resource;
    D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=size;desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;
    desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if(FAILED(state.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource))))return {};
    void *mapped=nullptr;D3D12_RANGE no_read={0,0};
    if(FAILED(resource->Map(0,&no_read,&mapped)))return {};
    std::memcpy(mapped,data,size);D3D12_RANGE written={0,size};resource->Unmap(0,&written);return resource;
}
bool resolution(unsigned base,unsigned flat,unsigned *resolved) {
    return flat?R_ResolveFlat(base,resolved)!=0:R_ResolveTexture(base,resolved)!=0;
}
}
extern "C" void RtMaterials_Init(void *device) {state.device=static_cast<ID3D12Device*>(device);}
extern "C" void RtMaterials_Prepare(const DxrMapSceneView *scene) {
    if(!scene){state.view={};return;}
    if(!state.device||state.failed_generation==scene->generation)return;
    try {
        bool rebuild=state.view.generation!=scene->generation||!state.view.descriptor_count;
        if(!rebuild)for(const auto &d:state.descriptors){unsigned resolved;if(!resolution(d.base_id,d.flat_namespace,&resolved)||resolved!=d.resolved_id){rebuild=true;break;}}
        if(!rebuild)return;
        state.view={};
        std::set<std::pair<unsigned,unsigned>> referenced;
        for(size_t i=0;i<scene->mesh->surface_count;i++) {
            const auto &s=scene->mesh->surfaces[i];if(!s.active||s.base_material<0)continue;
            referenced.emplace(s.kind==MAP_FLOOR||s.kind==MAP_CEILING?1u:0u,(unsigned)s.base_material);
            if(referenced.size()>descriptor_budget)throw std::runtime_error("material descriptor budget exceeded");
        }
        std::vector<RtMaterialDescriptor> descriptors;
        std::vector<uint32_t> pixels;
        std::map<std::pair<unsigned,unsigned>,RtMaterialDescriptor> copied;
        for(const auto &key:referenced) {
            unsigned resolved;R_MaterialDescription description={};
            if(!resolution(key.second,key.first,&resolved)||
               !(key.first?R_DescribeFlat(resolved,&description):R_DescribeTexture(resolved,&description)))throw std::runtime_error("material description unavailable");
            const auto resource_key=std::make_pair(key.first,resolved);
            auto existing=copied.find(resource_key);
            if(existing!=copied.end()) {
                auto descriptor=existing->second;descriptor.base_id=key.second;
                descriptors.push_back(descriptor);continue;
            }
            size_t n=(size_t)description.width*description.height;
            if(n>pixel_budget-pixels.size())throw std::runtime_error("material pixel budget exceeded");
            std::vector<unsigned char> indices(n),alpha(n,255);
            if(!(key.first?R_CopyFlatIndexed(resolved,indices.data(),n):R_CopyTextureIndexedAlpha(resolved,indices.data(),alpha.data(),n)))throw std::runtime_error("WAD material post/bounds rejected");
            RtMaterialDescriptor descriptor={key.second,resolved,key.first,description.width,description.height,description.width_mask,(unsigned)pixels.size(),(unsigned)n};
            descriptors.push_back(descriptor);
            copied.emplace(resource_key,descriptor);
            for(size_t i=0;i<n;i++)pixels.push_back(indices[i]|((uint32_t)alpha[i]<<8));
        }
        if(descriptors.empty()||pixels.empty())throw std::runtime_error("no copied materials");
        auto descriptions=copied_buffer(descriptors.data(),descriptors.size()*sizeof(RtMaterialDescriptor));
        auto atlas=copied_buffer(pixels.data(),pixels.size()*sizeof(uint32_t));
        if(!descriptions||!atlas)throw std::runtime_error("material GPU allocation failed");
        state.descriptions=descriptions;state.atlas=atlas;
        state.descriptors=std::move(descriptors);state.pixels=std::move(pixels);
        state.view={state.descriptors.data(),(unsigned)state.descriptors.size(),state.pixels.data(),(unsigned)state.pixels.size(),state.descriptions.Get(),state.atlas.Get(),scene->generation};
        std::fprintf(stderr,"RT materials: generation=%llu copied %u wall/flat descriptors, %u indexed-alpha pixels, %llu GPU upload bytes; passive foundation, no alpha/reflection enabled\n",(unsigned long long)scene->generation,state.view.descriptor_count,state.view.pixel_count,(unsigned long long)(state.pixels.size()*4+state.descriptors.size()*sizeof(RtMaterialDescriptor)));
    }catch(const std::exception &e){state.view={};state.failed_generation=scene->generation;std::fprintf(stderr,"RT materials: optional shared catalog unavailable: %s; sampled opaque point lighting remains independent\n",e.what());}
}
extern "C" const RtMaterialView *RtMaterials_GetView(void) {return state.view.descriptor_count?&state.view:nullptr;}
extern "C" void RtMaterials_Shutdown(void) {state=State();}
