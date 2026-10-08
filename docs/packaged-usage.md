# 打包后的图形与语言使用

本文说明实际设置和资源契约；最终集成的构建、六模式画面及中文扩展结果分别以
[最终验收记录](packaging-acceptance.md)为准，不凭资源存在声明功能执行成功。

## 构建和部署

Windows x64、D3D12、Visual Studio C++ 与 CMake。硬件 RT 需要 DXR 1.1、
Shader Model 6.5 和 DXC；官方 DLSS 需要受支持的 NVIDIA RTX/驱动。实际验收
工具链和驱动版本由运行记录固定，不把本次一台设备当所有硬件的最低要求。

在仓库根运行 `build.cmd --all`，输出原七种 `build-win/Release/windoom-*` 目录，
名称、flag、默认效果见根 README 原表。NGX 与 FSR2 SDK 使用现有固定下载入口；
官方 NGX SDK 为 v310.9.1、commit
`374959484e79a640feaba44c93ac8cfb0a03f5b5`；SR DLL 文件版本为310.9.1.0。
SDK 发布号、DLL 文件版本、预设和功能分别记录，不能由版本名推断 NR 执行。

每个目录包含程序、语言、`zh-cn.cjk` 字体及 `FONT-OFL-1.1.txt` 许可、默认光源/材质及已构建的五个 DXR shader；NGX 目录
按实际 SR/RR 功能部署官方 DLL 和 ngx.mode；Anime4K 包含其独立 shader。
未安装 DXC 时普通显示仍可用，但 RT 缺 shader 必须明确回退，不算完整 RT 包。
default effect 文件只在不存在时复制，重新 staging 不覆盖玩家已编辑的配置。
语言和效果资源按 exe 所在目录查找，可从其他工作目录直接启动。
IWAD 不由程序提供；传入合法 IWAD 的绝对路径，例如：

```powershell
C:\games\windoom-ngx-dlss4.5\windoom.exe -iwad C:\games\wads\freedoom2.wad -config C:\games\profiles\player.cfg -lang es-ascii
```

## 菜单

Escape → Options → Graphics；上下选择，左右或 Enter 切换请求，Backspace 回到
Options，Escape 关闭。RT 总开关控制硬阴影及已配置表面的单次反射，SR 为官方
DLSS Super Resolution。诊断 F5 和 `-rt-diagnostic` 不是游戏光照开关。
默认灯位在 `rt-light.cfg`，材质在 `rt-materials.cfg`；示例不把所有墙都变为镜面，
RT 默认关闭。修改灯的位置、强度或纹理材质后重启加载，详见图形菜单与反射文档。

请求 ON/OFF 和实际状态分开：关闭、等待、启用、不可用、回退、未验证、未知、
非场景暂停。SR 失败保留成功 RT 颜色；RT 失败保留普通颜色和其他可用显示。
显式关再开可以重新检查已恢复的依赖，不在每帧隐藏失败重试。菜单/武器/HUD
在场景效果之后叠加，菜单切换与地图/相机变化按历史 reset 契约处理。

图形优先级为模式默认 → 已保存字段 → 明确 CLI。支持 `-rt/-nort`、
`-sr/-nosr`（原 `-dlss/-nodlss` 保留）；仅本次 CLI 覆盖不自动写偏好。
菜单主动修改只保存那一项，偏好文件为解析后的现有 `-config` 路径追加
`.graphics.cfg`，不改变原按键/声音设置文件。保存失败仍使用本次请求，底部显示
未保存原因，原备份保留。原子效果关闭 flag 和显式灯/材质 CLI 的优先级见
[图形菜单](graphics-menu.md)。

## 语言

`-lang es-ascii` 是原字体可显示的西班牙语示例；`-lang en` 是内建英文；
`-lang zh-cn` 使用随包部署的 12px 中文位图，覆盖当前中文包的 289 个非 ASCII 码点。
中文字体缺失、损坏或不覆盖译文时整包回退英文并记录原因；不依赖系统安装字体。
语言选择优先级为明确 CLI → exe 旁 language.cfg → 英文。成功显式选择保存，
坏包回退英文且不改原选择；语言只启动时加载，重新选择需重启。语言偏好位置
与图形 profile 不同，不能把它们混为一个配置文件。
缺键逐字段英文，坏 schema/重复键/非法 UTF-8/超限整体英文；字体、长文字及
已迁移键见 [语言包](language-packs.md)。中文包与实际字体扩展的部署及许可见
[中文字体](chinese-font.md)，实际画面以最终集成验收记录为准。

## DLSS5／NR 验证范围

DLSS5／DLSSNR 是本项目神经渲染增强，与 SR 和 DLAA 是不同功能。原
windoom-ngx-dlss5 及 Swapper 路线完整保留，不安装/注入第三方组件。2026-10-07
用户明确豁免 NR 验证：本轮不启动该模式、不切 NR、不验 consumer/checkbox/NR
GPU 执行。七模式静态构建/部署清单不能充当 DLSS5 运行正例；NR 未验证不记 PASS。

## 观察与恢复

`-graphics-stats <new.csv>`、`-gpu-timing <new.csv>` 与固定 demo PNG 可分别观察
请求/实际状态/reset/执行与各 GPU 阶段/本地显存。同 demo 比较按 game_tic 对齐；
导出帧序号在 wipe 中可重复游戏 tic。GPU timestamp 包含本次观察/导出成本，
不保证交互 FPS。实际窗口截图的焦点、DPI、客户区和完整 HUD 独立检查，不将
失焦截图或不同尺寸图像称为逐像素一致。

升级运行库前关闭使用该目录的游戏，使用既有 fetch-ngx 事务备份及恢复入口，
让 SDK 和 DLL 作为一致版本恢复；不能只替换一个文件或强行覆盖已加载 DLL。
备份/恢复步骤见 [NGX](../win32/README-NGX.md) 与固定下载工具说明。
图形偏好 `.bak.<pid>.<sequence>` 备份不自动删除；恢复时先备份当前文件，关闭
游戏后再复制已核验的同 profile 备份。用户自定义效果和语言包也应另存副本。
