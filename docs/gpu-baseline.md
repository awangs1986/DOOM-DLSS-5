# 独显回放基线

基线入口为 `tools/capture-baseline.ps1`，使用既有完整 seam：启动游戏、播放固定 demo、导出最终 PNG。只在全新证据目录写入构建、配置及回放结果，拒绝复用已有目录，不安装 SDK、不修改驱动、不覆盖 IWAD、不结束其他进程。

## 远程入口与交互桌面

先读取用户指定 runner 配置；本会话最初指定的入口为 `/home/awang/.local/share/pi-coffee-workbench/sessions/runners/runners.json`，随后用户提供了项目会话专用 RTX runner。使用最后确认的配置中的 `usage.command` 数组调用 `list`、`exec`、`upload`、`download`，并在每次调用前核对 runner 身份和确认的端点。不要读取或复制 `credentialFile`，不要把密码写入命令。上传和下载使用 `C:/...` 形式的 Windows 绝对路径。

Windows SSH 默认运行在 Session 0。即使 `quser` 显示 console 用户活动，也不意味着 SSH 中直接启动的窗口可显示。本次实际直接启动失败为 `CreateSwapChainForHwnd failed (0x887a0022)`。通过临时无触发器 Scheduled Task 的 `Interactive` token 在登录用户桌面执行后，程序成功初始化并导出画面。需要实际登录的可用桌面；没有它时只做 probe/build，并保留视觉验收未完成的结论。

```powershell
& C:\isolated\source\tools\capture-baseline.ps1 -ProbeOnly
& C:\isolated\source\tools\capture-baseline.ps1 -BuildOnly
```

`-CMake` 可指定绝对路径；脚本也会通过 `vswhere` 查找 BuildTools 内的 CMake。当前生成器为 Visual Studio 2022 x64，Release。输出默认位于 `%TEMP%\windoom-baseline-<guid>`，由终端打印路径。`-SourceRevision` 用于上传源码快照；同时记录实际 instrumentation 源码摘要。它不会调用会修改仓库构建目录的 `build.cmd`。

## 固定输入

使用自己合法持有的 IWAD 或官方 [Freedoom 0.13.0](https://github.com/freedoom/freedoom/releases/tag/v0.13.0)，不提交 WAD。可从匹配 IWAD 中提取确定的短单人 demo：

```bash
python3 tools/prepare-baseline-demo.py freedoom2.wad baseline.lmp --ticks 210
```

该入口检查 IWAD 目录和版本 109 单人 demo，保留原始 13 字节头与前 210 个 tic 的输入，再写结束标记，拒绝覆盖输出。stdout 给出 IWAD、完整源 demo 和截取结果的 SHA-256。这个 6 秒输入用于最初基线；更长回放和针对性场景另行确定。

## 同一 demo 回放

需要已经按官方 DLL 部署入口准备好的 SR 可执行文件。脚本只读取该目录及其 DLL，不进行 SDK 部署。

本机活动桌面内可运行：

```powershell
& C:\isolated\source\tools\capture-baseline.ps1 `
  -Iwad C:\isolated\assets\freedoom2.wad `
  -Demo C:\isolated\assets\baseline.lmp `
  -SrExecutable C:\isolated\sr\windoom.exe `
  -InteractiveDesktopConfirmed
```

SSH 中改用桌面 worker：

```powershell
& C:\isolated\source\tools\start-baseline-interactive.ps1 `
  -Iwad C:\isolated\assets\freedoom2.wad `
  -Demo C:\isolated\assets\baseline.lmp `
  -SrExecutable C:\isolated\sr\windoom.exe
```

worker 使用当前 Windows 用户的交互 token，权限为 Limited，无密码和循环触发器，结束后移除仅本次创建的唯一任务。执行策略阻止脚本或用户未登录时失败并报告原因，不修改系统执行策略。输入参数保存为 JSON 数据，通过 PowerShell splat 传给 capture，文件路径不会成为动态代码。worker 的 success 只表示脚本执行完成，验收仍须检查每个 run 的结果。

capture 运行原版颜色、深度、法线、速度和当前 SR 颜色五个用例，读取相同 IWAD/demo 并记录摘要，分别使用隔离配置文件。每个用例保存 stdout、stderr、完整 PNG、命令、可执行文件摘要、退出码、实际 present mode 及关键帧摘要。`-KeyFrames` 指定导出序号，`-KeyGameTics` 指定游戏 tic，默认后者为 35、70、175。回放超时仅结束本脚本创建的进程；PowerShell 5.1 在启动后立即缓存进程句柄，避免退出码被错误读为 null。

## 时间线与测量

**按 `game_tic` 对齐比较画面。** 现有 wipe 会多次调用呈现，导致同一 tic 导出多个 PNG，数量受回放墙钟速度影响。本次 210 tic 输入原版导出 251 帧，SR 导出 212 帧；tic 175 分别是原版 f000216 与 SR f000177。不能把相同 PNG 序号当成相同 demo 时刻，也不能把序号除以 35 当成真实 demo 时间。`exportSequenceSeconds` 仅为给视频编码使用的序列索引；真实游戏 tic 来自 GPU CSV。

新增 `-gpu-timing <new.csv>` 是可选诊断入口。D3D12 timestamp query 分别测量上传、SR/其他超分执行、合成和导出 GPU copy，以及整个 command list 的 GPU 区间；使用队列 timestamp frequency 换算毫秒。读取发生在现有 queue fence 完成后，最后一帧在关机前收集。它不包含 CPU 软件渲染、PNG 编码或 `Present`，总 GPU 时间包含导出 copy，不能当成无导出的游戏帧时。

CSV 同行包含导出序号、真实 `gametic`、进程在实际渲染 adapter 的 local-memory 当前使用和 budget。查询不可用时空值表示 unavailable。采样初始化或输出失败只关闭诊断，普通渲染继续；CSV 拒绝覆盖已有文件。没有该参数时不创建查询、readback 或日志资源。

额外 `nvidia-smi` 每 500 ms 记录设备时间、显存和 GPU 利用率，可能包含其他进程，不作为 GPU 分阶段时间。混合显卡机器还需核对实际渲染 adapter；日志记录其名称、vendor/device，不能仅凭设备清单认定使用 RTX。

## 判读证据与本次基线

`manifest.json` 分开记录连接（需要 runner CLI transcript）、构建、进程创建、渲染初始化和视觉验收。PNG 存在或退出码零不能自动证明正确画面。必须检查关键帧、完整日志及 SR 实际路径；回退的画面不能作为 SR 基线。保存未经 timestamp instrumentation 的旧基线，再保存 instrumented 观察，明确分组。

2026-10-06 实际 RTX runner 为 RTX 5070 Ti Laptop GPU（12 GB，驱动 617.14），同时有 AMD Radeon 610M；Windows 11、Visual Studio 2022 BuildTools、Windows SDK 10.0.26100.0、CMake 可用，交互 worker 运行在 Session 2。原版四个视图与既有 v310.7.0 SR 均编译、初始化、结束并导出实际 PNG；SR 日志包含 `DLSS SR created (l)` 与 `present mode: dlss-upscale`。固定 320x200 创建仍低于日志建议的 427x267 optimal，留作后续 SR 输入工作。

实际 timestamp 首轮中位数：原版总 command list 约 0.317 ms，SR 约 0.699 ms，包含约 0.29 ms 的导出 copy。首次 local memory 约 33 MiB 与 147 MiB。这是本次短 demo、旧 SR DLL 和导出条件下的基线，不代表新 DLL 的提升或完整游戏性能。准确数值与最终重复运行以证据目录 CSV 为准。
