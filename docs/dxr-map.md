# 真实静态地图 DXR 场景（#11）

> 本文保留该票首次交付的实现与验收快照；当前动态扇区、分组 AS 和版本更新规则见 [动态场景文档](dxr-dynamic-map.md)。

## 使用

Windows x64、支持 DXR 1.1 / Shader Model 6.5 的硬件适配器，以及构建生成的 `shaders/dxr_map.cso`：

```powershell
.\windoom.exe -iwad .\freedoom2.wad -warp 1 -rt-map-depth
.\windoom.exe -iwad .\freedoom2.wad -warp 1 -rt-map-normal
```

F5 开关地图诊断。`-nort`、软件适配器、缺失着色器、场景构建失败均保留普通呈现。此功能与 NGX 独立；同时请求三角形诊断时，地图诊断优先。整个 D3D12 设备被移除仍沿用原渲染器的致命错误处理，没有增加设备重建。

`-map-stats <新的.csv>` 输出逐帧命中覆盖、距离与法线比较，以及真实 GPU 屏幕外探针。`-map-hits <新的.csv> -map-hit-tic 175` 输出指定游戏 tic 的逐像素命中和 `(instance, geometry, primitive)` 元组。观察文件拒绝覆盖已有路径。使用 CSV 的 `game_tic` 对齐导出 PNG；wipe 会使一个 tic 对应多帧。

## 几何与边界

`MapSource_Capture` 将当前关卡数据复制为 `MapInput`，随即交给与引擎无关的 `MapMesh_Build`；输出不保留关卡、WAD 或 zone 内存指针。

- 实体墙从完整 linedef/sidedef 集合建立，包含双面墙两侧的上下高度差。不会依据当前视锥、可见 seg 或本帧 drawseg 收集几何。
- 地板和天花板先沿根到叶路径裁剪 BSP 半平面，再应用叶内真实 seg 半平面约束，得到闭合凸区域后才三角化。原始 seg 列表可能开放，不作为闭合三角扇使用。
- 边界裁剪采用地图包围盒加 64 单位的临时种子；最终区域仍接触种子边界时拒绝构建。真实空区域记录为 empty leaf；凹形和洞由多个闭合叶区域组成。
- 校验索引、子节点、循环、共享节点、不可达叶、扇区一致性、非有限数、闭合性、绕序、资源大小和深度限制。GPU float 转换后退化的三角形会导致事务性失败。
- sky 平面、两侧均 sky 的上墙、双面 masked 中墙不进入实体 AS。当前仅复制首次场景帧的高度；移动门、升降地板、masked alpha 与动态同步由后续票处理。

地图 `(x,y,height)` 映射为 GPU `(x,height,y)`。地板法线 +Y，天花板 −Y。墙元数据同时保存两个固定约定：`legacy_normal` 保留历史有向 seg 左侧法线，`inward_normal` 指向所属 front sector 的右侧。#12 将软件 G-buffer 和地图法线诊断统一为后者，供物理照明使用；均不按相机方向翻转。GPU 三角形绕序仍对应 legacy 法线；射线不剔除背面。以下 #11 的 2026-10-06 数据记录原提交的历史约定，#12 的复核见 [点光源文档](dxr-lighting.md)。

## 表面与材质接口

稳定 surface id 是复制元数据数组的槽位：

- 墙：`line * 6 + sideSlot * 3 + tier`，tier 为 mid / upper / lower。
- 平面：`lineCount * 6 + sector * 2 + ceiling`。

未启用的槽保留，当前 active 与 triangle_count 显式记录。一个扇区平面可能分散于多个叶；以 `triangle_surfaces` 查找，不假设连续三角形范围。

`MapSurface` 复制 linedef、实际 sidedef、front/back sector、flags、原始 texture/flat id、偏移、墙 texture_height 和 V anchor；`MapMeshVertex` 复制地图单位 UV。`base_material` 尚未应用动画帧翻译，后续材质导出负责实际采样资源、纹理尺寸与动画状态。

墙 U 为 sidedef textureoffset 加沿所属边方向距离，V 为 `v_anchor - height`。上墙、下墙、单面中墙分别遵守 DONTPEGTOP / DONTPEGBOTTOM 规则。平面 UV 为 `(mapX,-mapY)`，保持经典 flat 采样方向。

当前一个实例 / 一个 BLAS / 一个 geometry。共享 `scene-hit.hlsli` 及 CPU `DxrMap_GetSurface` 通过 range table 将 `(InstanceID, GeometryIndex, PrimitiveIndex)` 映射为全局三角形，再读稳定 surface id；primitive index 不被当作全局 surface id。

`DxrMap_GetScene` 提供当前 scene 的借用视图：CPU mesh、TLAS、GPU primitive metadata、geometry ranges、vertex/index resources 与 generation。借用者不释放资源，必须在同一渲染线程、已等待渲染队列后访问，并在 unload / shutdown 前结束使用；任何指针和 GPU 地址均不得跨 generation 保存。#12 增加 `DxrMap_RequestScene` 普通消费者请求；地图诊断或普通消费者请求均可触发构建。仅普通消费者请求时不创建诊断 pipeline/buffers，不依赖 `dxr_map.cso`。

## 帧契约与合成

主射线采用 #5 `GB_FrameInputs` 冻结 sampled 相机及 `GB_SampleRay` 的真实列 / 平面采样。方向不归一化，因此 `CommittedRayT` 与原始 `GB_Depth` 的软件视深单位一致，不替换供超分使用的 device-depth 资源。

诊断只替换有效静态世界覆盖。world sprite 保留软件像素；sky、武器、HUD、菜单、viewport 外区域均来自 `GB_ComposePresent`。有效世界 ray miss 显示洋红色，使几何缺口可见。逐帧 CSV 包括被武器覆盖下的世界比较，不通过 overlay 排除数据。

AS 使用独立构建 command list，在渲染队列上显式完成 BLAS / TLAS build 与 fence 后发布借用视图。每帧 readback 在原渲染器 queue wait 后读取。`P_SetupLevel` 在 `Z_FreeTags` 前卸载；新关卡只标记 generation，第一帧恢复后的世界再复制，确保 savegame unarchive 已完成。诊断 pipeline/buffer 可跨地图复用，AS/mesh/metadata 属于单个 generation。

## 2026-10-06 实际验收

RTX 5070 Ti Laptop、驱动 617.14、Windows 交互 Session 2、MSVC Release / DXC。构建基线 `2de904a`。

- 实际 exe SHA256：`06e663856544136960f18596db7d04a374496eb7cb3893bf7442a55bc3715ac9`。
- 地图 shader SHA256：`7a156bc30282d1261fb82c87e11d15e6eccaa61fb31e7d8080e214439a615c49`。
- 官方 Freedoom 0.13.0 `freedoom2.wad` SHA256：`a8772e088847032510d97ba2312406a6998f21cbab44d4ff10696faa9c0ecd4b`。

核心验收使用此未修改 IWAD 和独立 vanilla 1.09 MAP01 demo，300 个普通静止 / 前进 / 转向 ticcmd，不使用三角形或矩形替代地图。

| 实际地图 | 叶 / 空叶 | 三角形 | active surfaces | 闭合面积 |
| --- | --- | --- | --- | --- |
| MAP01 | 698 / 2 | 4600 | 1588 | 3987681.216 |
| MAP02 | 538 / 0 | 3848 | 1389 | 3451556.000 |

通过公共 builder 对官方 MAP01 / MAP02 全量检查，均为零退化三角形、零绕序与 metadata 法线错误。

MAP01 10 个模式均 exit 0：普通 color、软件 depth / normal、overlay / scene mask、GPU map depth / normal、`-nort`、缺失 shader、WARP。按 game_tic 对齐五个关键帧：

| tic | 静态 eligible / GPU hits | 距离达标 | 法线达标 | 屏幕外 forward dot |
| --- | --- | --- | --- | --- |
| 45 | 41472 / 41472 | 41287 | 41370 | −48.000 |
| 95 | 41464 / 41464 | 41347 | 41263 | −182.649 |
| 175 | 41403 / 41403 | 41183 | 40973 | −64.869 |
| 220 | 41448 / 41448 | 41280 | 41206 | −385.252 |
| 280 | 41448 / 41448 | 41280 | 41206 | −385.252 |

上述统计不排除边界像素。距离容差 `max(0.5 map unit, 1% reference depth)`，法线容差编码后每通道 ±2。tic175 平均绝对距离误差 0.2854915，最大 233.3537；446 个像素至少一项不达标，其中 432 个处于软件法线边界两像素内，另外 14 个处于同法线不同高度天花板的深度跳变边界。若把参考深度邻域跳变 `> max(8,10%)` 一并作为边界指标，446 / 446 都在两像素内。该指标仅解释偏差，不修改上表计数。

具体差异：低分辨率 `(209,69)` 软件深度 228.4338、GPU 461.7875，下一行 `(209,70)` 软件 461.8599、GPU 461.7875；`(202,33)` 软件 248.0838、GPU 339.3309，下一行两者均 346.1874。两种画面已人工查看，区域与视角一致，边缘取整存在一条 scanline 的差异；没有声称像素完全等同。

独立 GPU 探针使用实际相机反向水平射线。tic175 命中 surface774，即 linedef129 的 mid wall，真实 endpoints `(320,-256)→(256,-256)`，GPU hit `(288.881,-23.002,-256)`，距离 64.86932，`dot(hit-origin,forward)=-64.86932`。它位于相机背后，证明完整地图参与 AS。

五个关键帧中 map depth / normal 的 379264–381056 个 overlay mask 白像素均与同 tic 普通 color 完全相同；`-nort`、缺失 shader、WARP 的完整 1280×800 画面与普通 color 完全相同。

生命周期使用另一份隔离 IWAD，只改变 MAP01 的原 player-start THINGS 为 `(216,-664),90°`，原官方地图所有几何、BSP、材质和资源字节均不变，以便通过原 linedef774 special11 出口。实际操作 quicksave → 连续两次 quickload → Use 原出口 → intermission → MAP02 → New Game MAP01 → WM_CLOSE，642 帧 exit 0。generation1..5 依次构建 / fence / 卸载，恢复后首帧相机高度 −55（MAP02 为 41），包含 pause、menu、缩小 viewport、F5 关闭再启用。MAP01 的 DXGI process local-memory 稳态中位数在三次恢复及返回后均 94924800 字节，未见本序列逐代增长。该短序列不作为长时间泄漏压力测试。

MAP02 的截图也显示少量几何 / sky 接缝洋红像素；当前 GPU 主射线边界与软件 scanline 可见性并非逐像素相同。动态扇区尚不实时更新。

## 可复现 CPU 验证

```sh
g++ -std=c++17 -Wall -Wextra -Iwin32 tools/check-map-mesh.cpp win32/map_mesh.cpp -o /tmp/check-map-mesh
/tmp/check-map-mesh
```

公共接口 fixtures 验证矩形面积 / 绕序 / owned rebuild、开放 seg 的凹 L、带洞环、相邻 portal、高度差、UV pegging、sky / normal、零长边、坏索引 / leaf range / child / cycle / 非有限数与 GPU float collapse。Windows 实际证据和完整生成脚本保存在本次外部验证目录 `map-evidence`，不提交官方 IWAD 或大量逐帧 PNG。
