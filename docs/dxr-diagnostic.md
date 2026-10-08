# DXR 硬件诊断

这个模式验证 Windows D3D12、适配器、DXC、最小 BLAS/TLAS 和 inline RayQuery 的完整路径，不表示真实 DOOM 地图已有光追阴影或反射。DXR 与 NGX 独立判断：没有 DLSS SDK 也可以运行硬件诊断。

## 构建和启动

需要 CMake 3.20+、Visual Studio C/C++ 工具链及支持 DXR 的 Windows SDK。构建默认寻找 PATH 或 Windows Kits x64 目录中的 `dxc.exe`；也可配置 `-DDXC_EXECUTABLE=<absolute path>`。DXC 使用 `cs_6_5` 编译诊断 shader，直接输出到可执行文件旁的 `shaders/dxr_diagnostic.cso`。仅修改 shader 时也会刷新此运行资源，`build.cmd` 会把它复制到所选 staged 目录。

```powershell
.\build.cmd
.\play-windoom.cmd -rt-diagnostic
```

日志枚举 DXGI 索引、适配器名称、vendor/device、软件标记及 D3D12 支持情况。默认选择可用的 NVIDIA D3D12 设备，缺少 NVIDIA 时选择其他可用硬件；显式 `-adapter <index>` 优先，且不会悄悄改用别的设备。索引以本机枚举日志为准，不能把某台机器的索引复制到另一台机器。无效索引会明确拒绝启动。

```powershell
.\play-windoom.cmd -adapter 1 -rt-diagnostic
.\play-windoom.cmd -rt-diagnostic -nort
```

开启诊断后，整幅窗口由硬件射线测试图替换：绿色三角形代表命中，深蓝色代表未命中。射线针对一个真实构建的三角形 BLAS 和一个 TLAS instance，不是软件画出的图形。诊断是独立工具视图，覆盖 HUD 和菜单。`F5` 在诊断与正常游戏视图间切换；该快捷键只在以 `-rt-diagnostic` 启动时生效。关闭诊断后仍可用 `F1`–`F4` 查看原有 G-buffer。

## 能力与失败

必须同时满足非软件适配器、DXR tier 1.1+、Shader Model 6.5+、Device5 和 CommandList4。日志保留 D3D12 原始 tier 值；Windows SDK 中 10=1.0、11=1.1、12=1.2。不能由 NVIDIA 品牌或 NGX 成功推断 DXR 支持，也不能单凭 DXR API tier 推断实际硬件：本次 Windows 11 WARP 软件适配器也报告 1.1，因此显式拒绝软件适配器用于硬件诊断。

能力不足、`-nort`、没有 DXC 产物、损坏 shader、资源或 pipeline 初始化失败都会记录原因并保留正常游戏呈现。缺少 DXC 时普通构建仍可完成；`-DWINDOOM_DXR=OFF` 可跳过 shader 构建。shader 从 exe 相邻路径加载，改变工作目录不会影响查找。已有 CSV 等用户文件不会被诊断模块替换。

若执行所需接口或设备状态失败，诊断会关闭并返回正常呈现调用。实际整个 D3D12 设备被移除时，原有 renderer 的设备移除处理仍报告 fatal；这不等同于可用设备上的单个 RT 功能失败，不宣称已有完整 GPU 设备重建。

## 同步和证据

最小场景构建使用独立 allocator/list 和 fence，但共享 renderer 的实际 queue。BLAS、scratch 和 TLAS 写入后有 UAV barrier；构建 fence 完成后才用于 RayQuery。每帧在已有等待完成后准备资源，输出 buffer 从 UAV 转到 COPY_SOURCE，再以 BGRA footprint 复制到已经处于 COPY_DEST 的 backbuffer，之后沿用 renderer 的导出和 Present。关闭视图保留资源方便切回；退出在 queue idle 后释放。

使用 `-export <dir> -gpu-timing <new.csv>` 保存实际最终像素和 GPU timestamps。诊断 GPU 区间计入 `compose_ms`，包含 GPU 输出复制，不是纯 RayQuery 耗时。第一条 dispatch 日志只说明已录制，GPU 执行证据需要构建 fence 完成、正常退出、对应 timestamp 行以及绿色命中/深蓝未命中的实际 PNG。

2026-10-06 在 RTX 5070 Ti Laptop GPU（617.14）及显式 AMD Radeon 610M 上都实际导出命中/未命中图；NGX 未编译，证明两个能力独立。缺失/损坏 shader 和 `-nort` 用例保持正常游戏并退出 0；F5 关闭、深度视图、重新开启和退出未出现设备错误。软件适配器 API 能力虽可用，仍因不是硬件而关闭诊断。
