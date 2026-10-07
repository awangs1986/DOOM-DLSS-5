# 已加载 NR consumer 的菜单控制接缝

模块为 `win32/nr_control.h/.cpp`，不安装、注入或加载第三方 consumer，也不改变原 Swapper 面板或项目默认 NR/carrier 路线。菜单与 renderer 的核心接入由 #16 负责；此模块只提供控制状态。

## C 接口与安全帧

- `NrControl_Request(0/1)`：仅保存本次请求并开启新的控制 epoch；没有 ImGui/D3D 调用。
- `NrControl_Poll()`：由同一 renderer 线程在 GPU fence 后、冻结本帧输入前调用。释放前次回调的临时 ReShade 引用，发现已加载 consumer、处理卸载/重载、注册回调、更新超时。没有调用原 consumer panel。
- `NrControl_GetStatus()`：返回值快照。`reason` 是静态 `graphics.reason.*` 字面量，没有 backend/runtime/heap 数据借用。
- `NrControl_Shutdown()`：在同一安全线程退出时注销自己注册的事件/addon/loader notification，并释放自己的临时引用。错误线程的 Poll/Shutdown 保守报告不可用，不擅自跨线程操作外部 UI。

`loaded` 表示最近安全帧发现已有同名 consumer；`supported` 必须由合法原 panel 回调实际看到可用 NR checkbox 后成立。`confirmed` 为 -1（未知）、0（原 checkbox 关闭）、1（开启）。新请求只在原 callback-local checkbox 参数中提交；下一次合法 overlay frame 回读后才确认，5 秒没有确认则失败并取消迟到写入，重试须有新请求或 backend 生命周期变化。原面板随后主动修改设置也会通过回读更新状态。

`execution_verified` 始终为 0：此私有设置适配不提供真实 NR feature/GPU 执行观察。不能把 checkbox、SR、DLAA 或 carrier 成功当成 NR 执行。`epoch` 在请求、实际确认、已控状态变化以及 backend/runtime 生命周期失效时变化；host 须在下一安全帧看到 epoch 变化后重置相关历史，不把旧 carrier 记录归入新控制状态。

缺 consumer 时为 `graphics.reason.backend_missing`；已加载 consumer 没有 ReShade 回调依赖时为 `graphics.reason.restart`；未知 binary/ABI、不可控或错误线程为 `graphics.reason.unsupported`；另外使用 `confirmation_pending`、`confirmation_failed`、`execution_unverified`。请求由 host 独立保留，缺依赖时不承诺 NR 已关闭。

## 固定契约与保护

只适配 Swapper 原 `Enable DLSS Neural Rendering` checkbox（原 field2/kind1/command103），不直接写 NR 全局设置，不保留 callback-local `bool*`。仅改变该项；其他 slider/button/combo 不响应隐藏 pass 的输入。disabled control 不提交请求。原 panel 的折叠、树节点及 BeginDisabled/EndDisabled 保持调用配对，恢复 dispatch 和 End 窗口使用 RAII。

固定 upstream：

- ReShade `18deaa52de0c425a78b329e9cb3c497281cd00ec`，API20。
- ImGui `3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c`，19250（1.92.5）。
- Swapper `9fb0b7c3563d350aa46eaa01d3e5ca4cbab4a7cb` 原 bridge/probe pin 数据。
- RenoDX6.5.3：878080 字节，SHA-256 `342341f669f1d64e0c70c8593a07a2fab5075e073dfae97c331c9a6776260a0a`。
- RenoDX4.7：1732608 字节，SHA-256 `d5adf82eb44b065f4c590ac91fe824bab07afea0eb9f994bde936710c8593952`。

文件 size/hash、x64 PE/映像范围、读写/执行页、consumer 初始化与注册代码 fingerprint、原 dispatch table identity 全部必须匹配；不猜未知版本地址。SDK headers 及 MIT/BSD 许可证在 `third_party/reshade/`，来源与 SHA-256 在 `pinned-sources.json`，适配 pin 数据保留 Swapper 归属及两个原许可证。

原 panel 只在 ReShade 的 `reshade_overlay` callback 中被调用。该 SDK event 明确保证位于 ImGui NewFrame/EndFrame 之间；固定 dispatch 的 `GetIO().Ctx` 非空和 `GetFrameCount()>0` 进一步检查此契约，并禁止同一 frame 重复隐藏调用。GetCurrentContext 没有出现在该 dispatch ABI，不能虚构 export/猜 context offset；GetIO/GetFrameCount 本身也依赖合法 event 契约，不是可在任意线程调用的 context 探针。

线程和重入保护针对本适配器；它不能为未知的其他 consumer callback 建立同步。临时替换的是原 consumer 的共享 dispatch slot，因此仍要求 DOOM 的一个 renderer/Present 线程与已匹配 runtime 契约成立。出现不同 runtime/线程时拒绝控制。尚未用真实已加载 consumer 验证外部 addon 注册时序、跨线程调用、卸载/重载和同地址句柄复用的实机行为，不据原生编译宣称这些正例已通过。

Loader notification 只使用 Interlocked 串号，在 loader lock 内不分配、不锁 mutex、不调用 consumer。串号使卸载/重载（包括同地址复用）立即失去旧确认；下一 Poll 重新识别。consumer 在原 callback 调用期间有临时模块引用，结束后释放；ReShade 回调持有的临时引用延后到下一安全 Poll 释放，避免在返回 ReShade 的调用栈上释放最后引用。没有永久 PIN；只平衡自己的 GetModuleHandleEx 引用，绝不释放用户提供的模块所有权。

缺 consumer 的常见路径不枚举进程、不哈希文件、不进行 PE/table survey。已注册路径只检查生命周期与超时；寻找尚未出现的 ReShade最多每250毫秒一次。没有测量游戏内 CPU 成本，不能声称无性能回归。

## 构建与证据

所有 Windows 模式加入 `win32/nr_control.cpp`，MSVC x64/C++17，`/EHsc`，链接 `advapi32`。不链接 ReShade importlib、不编译 ImGui 实现，也不部署 consumer DLL。文件只作为固定头文件 ABI 依赖。

独立 Windows 原生编译及 C 接口 headless contract 检查：C/C++混合目标 `/W4`，确认缺后端下请求开/关均保持 confirmed=-1/execution_verified=0、256 次 Poll 的 epoch 稳定、正常 Shutdown/重新初始化；没有 GUI、GPU、伪 consumer 或 ReShade 注入。详细实际源码 hashes、原始日志与结果在外部 `nr-control-native-*` 验收目录。最终完整项目构建、菜单与真实 GPU 场景由 #16 owner 继续验收；真实 NR 执行保持未验证。
