/* Optional hash-pinned original-checkbox NR adapter. GPLv2.
 * UI delegation adapted from MIT DLSS5-Swapper@9fb0b7c (overlay license
 * in third_party/reshade). Never installs, loads, pins, or injects a consumer.
 * ReShade external registration outside AddonInit is runtime-unverified. */
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <wincrypt.h>
#include <mutex>
#include <cstring>
#include <cstdint>
#include "nr_control.h"
#include "../third_party/reshade/imgui.h"
#include "../third_party/reshade/reshade_events.hpp"
#include "../third_party/reshade/reshade_overlay.hpp"
#include "../third_party/reshade/renodx-build-pins.hpp"
static_assert(sizeof(void *) == 8 && IMGUI_VERSION_NUM == 19250,
              "NR adapter requires the pinned x64 ImGui 1.92.5 ABI");
namespace {
constexpr const char *missing="graphics.reason.backend_missing";
constexpr const char *restart="graphics.reason.restart";
constexpr const char *unsupported="graphics.reason.unsupported";
constexpr const char *pending="graphics.reason.confirmation_pending";
constexpr const char *failed="graphics.reason.confirmation_failed";
constexpr const char *unverified="graphics.reason.execution_unverified";
constexpr ULONGLONG confirmation_timeout_ms=5000;
using Runtime=reshade::api::effect_runtime;
using RegisterAddon=bool(*)(void *,uint32_t);
using UnregisterAddon=void(*)(void *);
using Event=void(*)(void *,reshade::addon_event,void *);
using GetTable=const imgui_function_table *(*)(uint32_t);
struct Exports {RegisterAddon add=nullptr;UnregisterAddon remove=nullptr;Event event=nullptr,unevent=nullptr;GetTable table=nullptr;};
/* Documented ntdll notifications: callback executes under the loader lock.
 * It only uses Interlocked operations; no allocation, mutex or API discovery. */
struct DllData {ULONG flags;const UNICODE_STRING *full_name,*base_name;void *base;ULONG size;};
union DllNotification {DllData loaded,unloaded;};
using Notification=void(CALLBACK *)(ULONG,const DllNotification *,void *);
using RegisterNotification=NTSTATUS(NTAPI *)(ULONG,Notification,void *,void **);
using UnregisterNotification=NTSTATUS(NTAPI *)(void *);
void *volatile observed_reshade=nullptr,*volatile observed_consumer=nullptr;
volatile LONG reshade_serial=0,consumer_serial=0,runtime_serial=0;
void *volatile observed_runtime=nullptr;
void CALLBACK module_changed(ULONG reason,const DllNotification *data,void *) {
    if(!data||(reason!=1&&reason!=2))return;
    void *base=reason==1?data->loaded.base:data->unloaded.base;
    if(base==InterlockedCompareExchangePointer(&observed_reshade,nullptr,nullptr))InterlockedIncrement(&reshade_serial);
    if(base==InterlockedCompareExchangePointer(&observed_consumer,nullptr,nullptr))InterlockedIncrement(&consumer_serial);
}
struct Loan {
    HMODULE module=nullptr;
    Loan()=default;
    explicit Loan(HMODULE address){if(address)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(address),&module);}
    ~Loan(){if(module)FreeLibrary(module);} /* balances only our temporary ref */
    Loan(const Loan &)=delete;Loan &operator=(const Loan &)=delete;
};
struct State {
    std::mutex mutex;
    NrControlStatus status={0,0,-1,0,0,1,missing};
    DWORD owner=0;
    bool has_request=false,wanted=false,registered=false,registration_failed=false,attempt=false,awaiting=false;
    unsigned command_epoch=0,applied_epoch=0,callback_number=0,applied_callback=0;
    ULONGLONG deadline=0,next_discovery=0;
    HMODULE reshade=nullptr,consumer=nullptr;
    LONG re_serial=0,co_serial=0,rt_serial=0;
    HMODULE callback_reshade_hold=nullptr; /* released next Poll on owner thread */
    const nr_pins::build_pin *pin=nullptr;
    int imgui_frame=0;ImGuiContext *imgui_context=nullptr;
    Runtime *runtime=nullptr; /* identity only; never dereferenced outside callback */
    void *notification_cookie=nullptr;
    UnregisterNotification unnotify=nullptr;
    bool notification_attempted=false;
} state;
void bump(){if(++state.status.epoch==0)++state.status.epoch;}
void invalidate(const char *reason){const bool changed=state.status.confirmed!=-1||state.status.supported||state.status.pending||state.status.reason!=reason;state.status.confirmed=-1;state.status.supported=0;state.status.execution_verified=0;state.status.pending=0;state.status.reason=reason;state.awaiting=false;if(changed)bump();}
void arm(){state.attempt=state.has_request;state.awaiting=false;state.status.pending=state.attempt?1:0;state.deadline=GetTickCount64()+confirmation_timeout_ms;if(state.attempt)state.status.reason=pending;}
LONG serial(volatile LONG *p){return InterlockedCompareExchange(p,0,0);}
bool memory(HMODULE module,const void *address,size_t size,bool writable=false,bool executable=false) {
    auto at=reinterpret_cast<uintptr_t>(address);if(!at||size>SIZE_MAX-at)return false;const auto end=at+size;
    while(at<end){MEMORY_BASIC_INFORMATION info={};if(!VirtualQuery(reinterpret_cast<void *>(at),&info,sizeof(info))||info.State!=MEM_COMMIT||info.Type!=MEM_IMAGE||info.AllocationBase!=module||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        const DWORD p=info.Protect&0xff;const bool read=p==PAGE_READONLY||p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;
        const bool write=p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;
        const bool execute=p==PAGE_EXECUTE||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;
        if(!read||(writable&&!write)||(executable&&!execute))return false;
        const auto next=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(next<=at)return false;at=next;
    }return true;
}
bool image(HMODULE module,DWORD *size=nullptr) {
    MODULEINFO info={};if(!module||!K32GetModuleInformation(GetCurrentProcess(),module,&info,sizeof(info))||info.lpBaseOfDll!=module||!memory(module,module,sizeof(IMAGE_DOS_HEADER)))return false;
    const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(module);if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||static_cast<unsigned>(dos->e_lfanew)>info.SizeOfImage||info.SizeOfImage-static_cast<unsigned>(dos->e_lfanew)<sizeof(IMAGE_NT_HEADERS64))return false;
    const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(reinterpret_cast<const unsigned char *>(module)+dos->e_lfanew);
    if(!memory(module,nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->OptionalHeader.SizeOfImage!=info.SizeOfImage)return false;
    if(size)*size=info.SizeOfImage;return true;
}
template<class T> T exported(HMODULE module,const char *name){auto p=GetProcAddress(module,name);return p&&memory(module,reinterpret_cast<void *>(p),1,false,true)?reinterpret_cast<T>(p):nullptr;}
Exports exports(HMODULE m){return {exported<RegisterAddon>(m,"ReShadeRegisterAddon"),exported<UnregisterAddon>(m,"ReShadeUnregisterAddon"),exported<Event>(m,"ReShadeRegisterEventForAddon"),exported<Event>(m,"ReShadeUnregisterEventForAddon"),exported<GetTable>(m,"ReShadeGetImGuiFunctionTable")};}
bool complete(const Exports &e){return e.add&&e.remove&&e.event&&e.unevent&&e.table;}
HMODULE discover_reshade() {
    HMODULE modules[1024];DWORD used=0;HMODULE found=nullptr;
    if(!K32EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&used)||used>sizeof(modules))return nullptr;
    for(unsigned i=0;i<used/sizeof(HMODULE);++i){Loan loan(modules[i]);if(!loan.module||!image(loan.module))continue;const auto e=exports(loan.module);if(!complete(e))continue;if(found)return nullptr;found=loan.module;}
    return found;
}
bool file_hash(HMODULE module,const nr_pins::build_pin &pin) {
    wchar_t filename[32768];DWORD n=GetModuleFileNameW(module,filename,32768);if(!n||n>=32768)return false;
    HANDLE file=CreateFileW(filename,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size={};HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=GetFileSizeEx(file,&size)&&size.QuadPart==pin.size&&CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
    unsigned char buffer[65536];DWORD total=0;
    while(okay&&total<pin.size){DWORD got=0;const DWORD want=(pin.size-total)<sizeof(buffer)?pin.size-total:static_cast<DWORD>(sizeof(buffer));okay=ReadFile(file,buffer,want,&got,nullptr)&&got==want&&CryptHashData(hash,buffer,got,0);total+=got;}
    unsigned char digest[32];DWORD length=sizeof(digest);okay=okay&&total==pin.size&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)&&length==32&&!std::memcmp(digest,pin.sha256,32);
    if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return okay;
}
const nr_pins::build_pin *identify(HMODULE module){for(const auto &p:nr_pins::known_builds)if(file_hash(module,p))return &p;return nullptr;}
bool table_valid(HMODULE module,const imgui_function_table *table) {
    if(!table||!memory(module,table,sizeof(*table)))return false;
    static_assert(sizeof(*table)%sizeof(void *)==0,"pinned dispatch consists of pointers");
    for(size_t i=0;i<sizeof(*table);i+=sizeof(void *)){void *p=nullptr;std::memcpy(&p,reinterpret_cast<const unsigned char *>(table)+i,sizeof(p));if(!p||!memory(module,p,1,false,true))return false;}
    return true;
}
bool consumer_valid(HMODULE module,const nr_pins::build_pin &pin,const imgui_function_table *table) {
    DWORD size=0;if(!image(module,&size))return false;const auto *base=reinterpret_cast<const unsigned char *>(module);
    if(pin.slot>size||size-pin.slot<sizeof(void *)||pin.init_at>size||size-pin.init_at<pin.init_size||pin.call_at>size||size-pin.call_at<pin.call_size||pin.overlay>=size)return false;
    const auto slot=reinterpret_cast<const imgui_function_table *const *>(base+pin.slot);
    return memory(module,slot,sizeof(*slot),true)&&memory(module,base+pin.init_at,pin.init_size,false,true)&&memory(module,base+pin.call_at,pin.call_size,false,true)&&memory(module,base+pin.overlay,1,false,true)&&!std::memcmp(base+pin.init_at,pin.init,pin.init_size)&&!std::memcmp(base+pin.call_at,pin.call,pin.call_size)&&*slot==table;
}
struct Invocation {
    const imgui_function_table *original;
    bool seen=false,denied=false;
    unsigned disabled=0,depth=0;
    bool stack[32]={};
};
thread_local Invocation *current=nullptr;
bool checkbox(const char *label,bool *value) {
    if(!current||!label||!value||std::strcmp(label,"Enable DLSS Neural Rendering"))return false;
    if(current->seen){current->denied=true;return false;}current->seen=true;
    if(current->disabled||current->denied)return false;
    const int actual=*value?1:0;
    if(state.awaiting&&state.applied_epoch==state.command_epoch&&state.callback_number!=state.applied_callback){
        state.awaiting=false;state.attempt=false;state.status.pending=0;state.status.confirmed=actual;
        state.status.reason=actual==static_cast<int>(state.wanted)?unverified:failed;bump();return false;
    }
    if(state.attempt){state.applied_epoch=state.command_epoch;state.applied_callback=state.callback_number;state.awaiting=true;state.attempt=false;state.status.confirmed=-1;state.status.pending=1;state.status.reason=pending;
        if(actual!=static_cast<int>(state.wanted)){*value=state.wanted;return true;}return false;
    }
    if(!state.awaiting&&state.status.confirmed!=actual){state.status.confirmed=actual;state.status.reason=unverified;bump();}
    return false;
}
bool slider(const char *,float *,float,float,const char *,ImGuiSliderFlags){return false;}
bool button(const char *,const ImVec2 &){return false;}
bool small_button(const char *){return false;}
bool invisible(const char *,const ImVec2 &,ImGuiButtonFlags){return false;}
bool combo(const char *,int *,const char *const [],int,int){return false;}
bool combo2(const char *,int *,const char *,int){return false;}
void begin_disabled(bool value){if(!current)return;if(current->depth<32)current->stack[current->depth++]=value;else current->denied=true;if(value)++current->disabled;current->original->BeginDisabled(value);}
void end_disabled(){if(!current)return;if(current->depth){if(current->stack[--current->depth])--current->disabled;}else current->denied=true;current->original->EndDisabled();}
bool collapsing(const char *label,ImGuiTreeNodeFlags flags){return current&&current->original->CollapsingHeader(label,flags|ImGuiTreeNodeFlags_DefaultOpen);}
bool collapsing2(const char *label,bool *visible,ImGuiTreeNodeFlags flags){return current&&current->original->CollapsingHeader2(label,visible,flags|ImGuiTreeNodeFlags_DefaultOpen);}
bool tree(const char *label,ImGuiTreeNodeFlags flags){return current&&current->original->TreeNodeEx(label,flags|ImGuiTreeNodeFlags_DefaultOpen);}
struct Window {const imgui_function_table *table;~Window(){table->End();}};
struct Dispatch {
    void *volatile *slot;const imgui_function_table *original;imgui_function_table *temporary;
    ~Dispatch(){InterlockedCompareExchangePointer(slot,const_cast<imgui_function_table *>(original),temporary);current=nullptr;}
};
/* Temporary ReShade reference survives callback return to the next safe Poll
 * on this exact thread. Dropping its last ref on the ReShade caller stack
 * would be unsafe. Consumer loan ends after its original panel has returned.
 * No callback or panel address is invoked by Poll/Request. */
void overlay(Runtime *runtime) {
    std::unique_lock<std::mutex> lock(state.mutex,std::try_to_lock);if(!lock.owns_lock()||current)return;
    if(GetCurrentThreadId()!=state.owner){invalidate(unsupported);state.attempt=false;state.registration_failed=true;return;}
    if(!runtime||!state.registered||state.registration_failed||!state.pin||state.re_serial!=serial(&reshade_serial)||state.co_serial!=serial(&consumer_serial))return;
    if(state.runtime&&state.runtime!=runtime){invalidate(unsupported);state.attempt=false;state.registration_failed=true;return;}
    Loan consumer(state.consumer);Loan re(state.reshade);if(consumer.module!=state.consumer||re.module!=state.reshade||state.re_serial!=serial(&reshade_serial)||state.co_serial!=serial(&consumer_serial))return;
    if(!state.callback_reshade_hold){state.callback_reshade_hold=re.module;re.module=nullptr;}
    const auto e=exports(state.reshade);if(!complete(e))return;
    const auto *original=e.table(IMGUI_VERSION_NUM);
    if(!table_valid(state.reshade,original)||!consumer_valid(state.consumer,*state.pin,original)){invalidate(unsupported);state.attempt=false;return;}
    if(std::strcmp(original->GetVersion(),"1.92.5")){invalidate(unsupported);state.attempt=false;return;}
    // API20 reshade_overlay is documented between NewFrame and EndFrame.
    // GetCurrentContext is NOT in this pinned dispatch ABI. These supported
    // getters corroborate that contract; they are not safe out-of-frame probes.
    ImGuiContext *context=original->GetIO().Ctx;const int frame=context?original->GetFrameCount():0;
    if(!context||frame<=0){invalidate(unsupported);state.attempt=false;state.registration_failed=true;return;}
    if(state.imgui_context&&state.imgui_context!=context){invalidate(restart);bump();arm();state.imgui_frame=0;}
    state.imgui_context=context;if(state.imgui_frame==frame)return;state.imgui_frame=frame;
    state.runtime=runtime;InterlockedExchangePointer(&observed_runtime,runtime);++state.callback_number;
    Invocation invocation={original};imgui_function_table table=*original;
    table.Checkbox=checkbox;table.SliderFloat=slider;table.Button=button;table.SmallButton=small_button;table.InvisibleButton=invisible;table.Combo=combo;table.Combo2=combo2;
    table.BeginDisabled=begin_disabled;table.EndDisabled=end_disabled;table.CollapsingHeader=collapsing;table.CollapsingHeader2=collapsing2;table.TreeNodeEx=tree;
    original->SetNextWindowPos(ImVec2(-30000,-30000),0,ImVec2(0,0));original->SetNextWindowSize(ImVec2(600,1000),0);
    original->Begin("##DoomNRControl",nullptr,ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    Window window={original}; /* Begin/End remain valid even when clipped. */
    auto *base=reinterpret_cast<unsigned char *>(state.consumer);auto slot=reinterpret_cast<void *volatile *>(base+state.pin->slot);
    if(InterlockedCompareExchangePointer(slot,&table,const_cast<imgui_function_table *>(original))!=original){invalidate(unsupported);state.attempt=false;return;}
    {
        Dispatch restore={slot,original,&table};current=&invocation;
        try{reinterpret_cast<void(*)(Runtime *)>(base+state.pin->overlay)(runtime);}catch(...){invocation.denied=true;}
    }
    if(!invocation.seen||invocation.denied||invocation.disabled||invocation.depth){invalidate(unsupported);state.attempt=false;return;}
    state.status.supported=1;state.status.execution_verified=0;
}
void destroyed(Runtime *runtime){if(runtime!=InterlockedCompareExchangePointer(&observed_runtime,nullptr,nullptr))return;InterlockedIncrement(&runtime_serial);std::unique_lock<std::mutex> lock(state.mutex,std::try_to_lock);if(lock.owns_lock()&&runtime==state.runtime){state.rt_serial=serial(&runtime_serial);state.runtime=nullptr;state.imgui_context=nullptr;state.imgui_frame=0;InterlockedExchangePointer(&observed_runtime,nullptr);invalidate(restart);bump();arm();}}
void unregister_current() {
    if(state.registered&&state.reshade&&state.re_serial==serial(&reshade_serial)){
        Loan loan(state.reshade);if(loan.module==state.reshade&&image(loan.module)){const auto e=exports(loan.module);if(complete(e)){void *self=GetModuleHandleW(nullptr);e.unevent(self,reshade::addon_event::reshade_overlay,reinterpret_cast<void *>(&overlay));e.unevent(self,reshade::addon_event::destroy_effect_runtime,reinterpret_cast<void *>(&destroyed));e.remove(self);}}
    }
    state.registered=false;state.runtime=nullptr;state.imgui_context=nullptr;state.imgui_frame=0;InterlockedExchangePointer(&observed_runtime,nullptr);
}
void initialize_notifications(){if(state.notification_attempted)return;state.notification_attempted=true;HMODULE ntdll=GetModuleHandleW(L"ntdll.dll");auto add=reinterpret_cast<RegisterNotification>(GetProcAddress(ntdll,"LdrRegisterDllNotification"));state.unnotify=reinterpret_cast<UnregisterNotification>(GetProcAddress(ntdll,"LdrUnregisterDllNotification"));if(!add||!state.unnotify||add(0,module_changed,nullptr,&state.notification_cookie)<0){state.notification_cookie=nullptr;state.unnotify=nullptr;}}
}
extern "C" void NrControl_Request(int enabled){std::lock_guard<std::mutex> lock(state.mutex);state.has_request=true;state.wanted=enabled!=0;state.registration_failed=false;if(++state.command_epoch==0)++state.command_epoch;state.status.confirmed=-1;state.status.execution_verified=0;bump();arm();}
extern "C" void NrControl_Poll(void) {
    std::lock_guard<std::mutex> lock(state.mutex);const DWORD thread=GetCurrentThreadId();if(!state.owner)state.owner=thread;if(state.owner!=thread){invalidate(unsupported);return;}
    if(state.callback_reshade_hold){HMODULE held=state.callback_reshade_hold;state.callback_reshade_hold=nullptr;FreeLibrary(held);}
    if(state.rt_serial!=serial(&runtime_serial)){state.rt_serial=serial(&runtime_serial);state.runtime=nullptr;state.imgui_context=nullptr;state.imgui_frame=0;InterlockedExchangePointer(&observed_runtime,nullptr);invalidate(restart);bump();arm();}
    initialize_notifications();Loan consumer(GetModuleHandleW(L"renodx-dlss5.addon64"));
    const LONG cs=serial(&consumer_serial),rs=serial(&reshade_serial);
    // Normal missing-backend route has no process enumeration, hashing,
    // PE/table survey or retry attempt. Only prior registrations need teardown.
    if(!consumer.module){const bool changed=state.consumer!=nullptr;unregister_current();state.consumer=nullptr;state.pin=nullptr;state.co_serial=cs;state.re_serial=rs;InterlockedExchangePointer(&observed_consumer,nullptr);state.status.loaded=0;invalidate(missing);if(changed)bump();state.attempt=state.has_request;return;}
    if(state.consumer!=consumer.module||state.co_serial!=cs){state.consumer=consumer.module;state.co_serial=cs;state.registration_failed=false;InterlockedExchangePointer(&observed_consumer,state.consumer);state.pin=nullptr;invalidate(missing);bump();arm();if(image(consumer.module))state.pin=identify(consumer.module);}
    state.status.loaded=1;
    if(!state.pin||!state.notification_cookie){unregister_current();invalidate(unsupported);state.attempt=false;return;}
    if(state.re_serial!=rs){unregister_current();state.reshade=nullptr;state.re_serial=rs;state.registration_failed=false;state.next_discovery=0;InterlockedExchangePointer(&observed_reshade,nullptr);invalidate(restart);bump();arm();}
    const auto expire=[](){if(state.status.pending&&GetTickCount64()>=state.deadline){state.awaiting=state.attempt=false;state.status.pending=0;state.status.confirmed=-1;state.status.reason=failed;bump();}};
    // Loader notifications and callback-local guards protect the live route;
    // don't re-enumerate the process or recheck hundreds of ABI pointers here.
    if(state.registered){if(state.registration_failed){unregister_current();state.status.reason=unsupported;state.status.pending=0;return;}expire();return;}
    if(state.registration_failed){state.status.reason=unsupported;state.status.pending=0;return;}
    HMODULE re=state.reshade;
    if(!re){const ULONGLONG now=GetTickCount64();if(now<state.next_discovery){state.status.reason=restart;state.status.pending=0;return;}state.next_discovery=now+250;re=discover_reshade();}
    Loan reshade(re);
    if(!reshade.module){state.status.reason=restart;state.status.pending=0;return;}
    if(state.reshade!=reshade.module){state.reshade=reshade.module;InterlockedExchangePointer(&observed_reshade,state.reshade);state.re_serial=serial(&reshade_serial);invalidate(restart);bump();arm();}
    const auto e=exports(reshade.module);const auto *table=complete(e)?e.table(IMGUI_VERSION_NUM):nullptr;
    if(!table_valid(reshade.module,table)||!consumer_valid(consumer.module,*state.pin,table)){invalidate(unsupported);state.attempt=false;state.registration_failed=true;return;}
    void *self=GetModuleHandleW(nullptr);if(!e.add(self,20)){state.registration_failed=true;invalidate(unsupported);state.attempt=false;return;}
    // All ABI/consumer validation precedes registration. There is no partial
    // registered addon on rejection, and a failed request waits for a retry.
    e.event(self,reshade::addon_event::reshade_overlay,reinterpret_cast<void *>(&overlay));e.event(self,reshade::addon_event::destroy_effect_runtime,reinterpret_cast<void *>(&destroyed));state.registered=true;state.status.reason=pending;arm();expire();
}
extern "C" NrControlStatus NrControl_GetStatus(void){std::lock_guard<std::mutex> lock(state.mutex);NrControlStatus out=state.status;if(state.re_serial!=serial(&reshade_serial)||state.co_serial!=serial(&consumer_serial)||state.rt_serial!=serial(&runtime_serial)){out.confirmed=-1;out.supported=out.execution_verified=0;out.reason=restart;}return out;}
extern "C" void NrControl_Shutdown(void){std::lock_guard<std::mutex> lock(state.mutex);if(state.owner&&state.owner!=GetCurrentThreadId()){invalidate(restart);state.attempt=false;return;}unregister_current();if(state.callback_reshade_hold){HMODULE held=state.callback_reshade_hold;state.callback_reshade_hold=nullptr;FreeLibrary(held);}if(state.notification_cookie&&state.unnotify)state.unnotify(state.notification_cookie);state.notification_cookie=nullptr;state.unnotify=nullptr;state.notification_attempted=false;InterlockedExchangePointer(&observed_reshade,nullptr);InterlockedExchangePointer(&observed_consumer,nullptr);state.consumer=state.reshade=nullptr;state.pin=nullptr;state.owner=0;state.registration_failed=false;state.has_request=state.attempt=state.awaiting=false;state.status.loaded=0;invalidate(missing);}
