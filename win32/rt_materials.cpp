/* Passive bounded atlas shared by later alpha/reflection. GPLv2. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include "rt_materials.h"
#include "r_material.h"
#ifdef WINDOOM_NGX_DIAGNOSTICS
extern "C" {
#include "m_argv.h"
}
#endif
using Microsoft::WRL::ComPtr;
static_assert(sizeof(RtMaterialDescriptor)==32&&sizeof(RtMaterialSurface)==16,"shader material layout");
namespace {
constexpr size_t descriptor_budget=4096,pixel_budget=16*1024*1024;
struct State {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Resource> descriptions,atlas,surface_buffer;
    std::vector<RtMaterialDescriptor> descriptors;
    std::vector<uint32_t> pixels;
    std::vector<RtMaterialSurface> surfaces;
    RtMaterialView view={};
    uint64_t signature=0,surface_signature=0,failed_signature=0;
#ifdef WINDOOM_NGX_DIAGNOSTICS
    bool synthetic_failure_consumed=false;
#endif
} state;
ComPtr<ID3D12Resource> copied_buffer(const void *data,size_t size) {
#ifdef WINDOOM_NGX_DIAGNOSTICS
    if(!state.synthetic_failure_consumed&&M_CheckParm("-rt-material-fail-once")) {
        state.synthetic_failure_consumed=true;
        std::fprintf(stderr,"RT materials diagnostic: injected_failure=create-once (synthetic API result; not hardware failure)\n");
        return {};
    }
#endif
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
std::vector<RtMaterialSurface> copy_surfaces(const DxrMapSceneView *scene,const std::vector<RtMaterialDescriptor> &descriptors) {
    std::map<std::pair<unsigned,unsigned>,unsigned> lookup;
    for(unsigned i=0;i<descriptors.size();i++)lookup.emplace(std::make_pair(descriptors[i].flat_namespace,descriptors[i].base_id),i);
    if(scene->mesh->surface_count>1000000)throw std::runtime_error("surface metadata budget exceeded");
    std::vector<RtMaterialSurface> surfaces(scene->mesh->surface_count);
    for(size_t i=0;i<surfaces.size();i++) {
        const auto &s=scene->mesh->surfaces[i];auto &out=surfaces[i];out.descriptor_index=UINT32_MAX;
        out.masked=s.masked;out.kind=s.kind;out.lightlevel=(unsigned)std::clamp(s.lightlevel,0,255);
        if(!s.active||s.base_material<0)continue;
        auto found=lookup.find({s.kind==MAP_FLOOR||s.kind==MAP_CEILING?1u:0u,(unsigned)s.base_material});
        if(found!=lookup.end())out.descriptor_index=found->second;
    }
    return surfaces;
}
}
extern "C" void RtMaterials_Init(void *device) {state.device=static_cast<ID3D12Device*>(device);}
extern "C" void RtMaterials_RetryUnavailable(void) {state.failed_signature=0;}
extern "C" void RtMaterials_Prepare(const DxrMapSceneView *scene) {
    if(!scene){state.view={};return;}
    if(!state.device)return;
    uint64_t signature=1469598103934665603ull,surface_signature=1469598103934665603ull,failure_signature=0;
    try {
        auto add=[&](uint64_t value){signature^=value;signature*=1099511628211ull;};
        auto add_surface=[&](uint64_t value){surface_signature^=value;surface_signature*=1099511628211ull;};
        add(scene->generation);add_surface(scene->generation);add_surface(scene->material_revision);add_surface(scene->shading_revision);
        std::set<std::pair<unsigned,unsigned>> referenced;
        for(size_t i=0;i<scene->mesh->surface_count;i++) {
            const auto &s=scene->mesh->surfaces[i];add_surface(s.active);if(!s.active)continue;
            add_surface(i);add_surface(s.kind);add_surface((uint32_t)s.base_material);add_surface(s.masked);add_surface((uint32_t)s.lightlevel);
            if(s.base_material>=0)referenced.emplace(s.kind==MAP_FLOOR||s.kind==MAP_CEILING?1u:0u,(unsigned)s.base_material);
            if(referenced.size()>descriptor_budget)throw std::runtime_error("material descriptor budget exceeded");
        }
        for(const auto &key:referenced){unsigned resolved=UINT32_MAX;resolution(key.second,key.first,&resolved);add(key.first);add(key.second);add(resolved);}
        failure_signature=signature^surface_signature;if(state.failed_signature&&failure_signature==state.failed_signature)return;
        bool rebuild=state.signature!=signature||state.view.generation!=scene->generation||!state.view.descriptor_count;
        if(!rebuild) {
            if(state.surface_signature!=surface_signature) {
                auto surfaces=copy_surfaces(scene,state.descriptors);
                void *mapped=nullptr;D3D12_RANGE no_read={0,0};
                if(FAILED(state.surface_buffer->Map(0,&no_read,&mapped)))throw std::runtime_error("surface metadata map failed");
                std::memcpy(mapped,surfaces.data(),surfaces.size()*sizeof(RtMaterialSurface));
                D3D12_RANGE written={0,surfaces.size()*sizeof(RtMaterialSurface)};state.surface_buffer->Unmap(0,&written);
                state.surfaces=std::move(surfaces);state.surface_signature=surface_signature;
                state.view.surfaces=state.surfaces.data();state.view.material_revision=scene->material_revision;state.view.shading_revision=scene->shading_revision;
            }
            return;
        }
        state.view={};
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
        auto surfaces=copy_surfaces(scene,descriptors);
        auto descriptions=copied_buffer(descriptors.data(),descriptors.size()*sizeof(RtMaterialDescriptor));
        auto atlas=copied_buffer(pixels.data(),pixels.size()*sizeof(uint32_t));
        auto surface_buffer=copied_buffer(surfaces.data(),surfaces.size()*sizeof(RtMaterialSurface));
        if(!descriptions||!atlas||!surface_buffer)throw std::runtime_error("material GPU allocation failed");
        state.descriptions=descriptions;state.atlas=atlas;state.surface_buffer=surface_buffer;
        state.descriptors=std::move(descriptors);state.pixels=std::move(pixels);
        state.surfaces=std::move(surfaces);state.signature=signature;state.surface_signature=surface_signature;
        state.view={state.descriptors.data(),(unsigned)state.descriptors.size(),state.pixels.data(),(unsigned)state.pixels.size(),state.descriptions.Get(),state.atlas.Get(),scene->generation,state.surfaces.data(),(unsigned)state.surfaces.size(),state.surface_buffer.Get(),scene->material_revision,scene->shading_revision};
        std::fprintf(stderr,"RT materials: generation=%llu copied %u wall/flat descriptors, %u indexed-alpha pixels, %llu GPU upload bytes; shared alpha/secondary-hit catalog revisions=(%llu,%llu)\n",(unsigned long long)scene->generation,state.view.descriptor_count,state.view.pixel_count,(unsigned long long)(state.pixels.size()*4+state.descriptors.size()*sizeof(RtMaterialDescriptor)+state.surfaces.size()*sizeof(RtMaterialSurface)),(unsigned long long)scene->material_revision,(unsigned long long)scene->shading_revision);
    }catch(const std::exception &e){state.view={};state.failed_signature=failure_signature?failure_signature:signature;std::fprintf(stderr,"RT materials: optional shared catalog unavailable: %s; opaque lighting independent, alpha errors conservatively block\n",e.what());}
}
extern "C" const RtMaterialView *RtMaterials_GetView(void) {return state.view.descriptor_count?&state.view:nullptr;}
extern "C" void RtMaterials_Shutdown(void) {state=State();}
