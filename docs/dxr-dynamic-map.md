# 动态扇区与共享 DXR 场景（#13）

## 当前更新规则

普通 RT 点光源、地图诊断和后续反射消费者共用 `DxrMap`。`DxrMap_RequestSceneFor` 聚合独立请求；关闭点光源不会清除反射请求。未请求场景期间保留世界版本，重新请求时先比较当前世界，再发布当前场景。

`I_FinishUpdate` 在 `GB_EndFrame` / `GB_ConsumeReset` **之前**调用 `DxrMap_DetectChanges`。它只读取当前最终世界值，不根据 thinker 调用次数、速度或预测位置判定变化；回滚的运动也按最终值比较。`GB_HasScenePixels` 表示当前显示已捕获世界，不能在这一步用尚未冻结的 `frame.scene_valid`。

- 高度、sky 分类、MID 由无到有/由有到无（即使解析纹理高度相同）及影响几何的纹理高度变化：提取当前地图，递增 `geometry_revision`，当前帧请求 `GB_RESET_GEOMETRY=1024`。
- base material 或 active surface 的引用集合可能变化：递增 `material_revision`，供共享复制材质目录刷新引用。几何变化也保守递增此版本。
- 当前 sector lightlevel：只复制到表面的 `lightlevel`，递增 `shading_revision`；无地图几何提取、BLAS refit/rebuild 或 TLAS work。
- sidedef texture/row offset：更新复制的 surface offset、V anchor 及三角形展开后的 UV 上传；无几何提取或 AS work。
- shading 变化采用保守的同帧 `GB_RESET_SHADING=2048`。滚动纹理所在地图可能持续触发此重置，这是明确的历史失效策略，不提供动态材质运动向量。

GPU 修改继续在原渲染器 queue wait 之后执行。新版本发布前完成独立 AS 构建命令和共享队列 fence；旧资源在已有消费者完成后释放。场景和 surface 借用指针只供当前帧使用，下一次 `Prepare` 或卸载均可能使其失效；消费者应按 generation / revision 获取当前数据。

## 分组与命中身份

平面归属自己的 sector；同一 linedef 的两侧、三个 tier 均归属 front/back 中较小的有效 sector。每次实际几何变化重新调用已有 BSP/邻接 builder，包含移动扇区平面及所有 incident linedef 的两侧完整 tier、UV anchor 和表面 metadata。

每组按 opaque / masked 分开 descriptor。descriptor 索引是其实际位置：仅有 masked 时它为 geometry 0，同时有两类时 opaque 为 0、masked 为 1。InstanceID 是稳定 owner sector id。`MapGeometryRange` 将 `(InstanceID, GeometryIndex, PrimitiveIndex)` 转换为全局三角形，再读取稳定 surface slot。

shader 顶点/索引按上述 tuple 顺序重新打包。每个 BLAS 使用自己独立的三角形展开顶点上传，因此其他组的全局偏移变化不会破坏 DXR update 的布局约束。`ScenePrimitive` 保持 32 字节；UV/材质/光照由共享目录及独立表面表消费。

- 同一组的 descriptor 分类、surface 序列及三角形数稳定，位置改变：初建使用 `ALLOW_UPDATE`，当前更新使用同一 BLAS 的 `PERFORM_UPDATE` 和实际 `UpdateScratchDataSizeInBytes`。
- 三角形出现/消失或 opaque/masked 分类改变：只重建该组，替换该组 BLAS。
- 每次 BLAS 更新/重建后刷新 TLAS；纯 UV、光照和不影响位置/拓扑的材质变化不更新 AS。
- 关卡卸载先 fence，再释放 mesh、分组 AS 和引用；`P_SetupLevel` 仍在 `Z_FreeTags` 之前卸载。读档后的新 generation 只在完整 restored world 的场景帧建立快照，不保留 engine/WAD 指针。

## 观察接口

```powershell
.\windoom.exe -iwad .\freedoom2.wad -warp 1 `
  -rt-light 656 -160 -24 1 1 1 1048576 512 `
  -map-updates .\new-map-updates.csv -map-watch-sector 27
```

观察文件必须是新路径。CSV 包含 frame、game tic、generation、三个 revision、同帧 reset/history、三角形数、refit/rebuild 分组数量、CPU capture 时间、BLAS/TLAS 的真实 D3D12 timestamp 时间、当前 scene-owned GPU buffer bytes，以及观察扇区的最终 floor/ceiling。时间不包含 Present、PNG 编码或整个进程的其他资源；`resident_gpu_bytes` 不是总显存。

## 实际 RTX 结果

Windows 11、RTX 5070 Ti Laptop、DXR 1.1 / SM 6.5，当前签入 console Session 3，Limited Interactive task。正式矩阵 `interactive-03` 执行应用代码 `b67dba8264a1fca802673f0c20b7cdfa9ec55d5f`：13 次普通 demo 回放全部 exit 0。输入为原 Freedoom IWAD，仅调整已有 player start 六字节坐标/角度，地图几何、触发、速度和其他资源保持原字节。所有移动由普通 Use / turn ticcmd 触发，无私有 thinker 或高度修改。

| 原事件 | 实际高度 | 动态帧 / refit 分组累计 | 拓扑 rebuild | CPU capture 中位数 | BLAS / TLAS 中位数 |
| --- | --- | --- | --- | --- | --- |
| MAP01 door27 / line165 special117 | ceiling −64→28→−64 | 24 / 46 | tic39、212，各1组 | 4.950 ms | 0.184 / 0.087 ms |
| MAP01 platform114 / line1223 special123 | floor72→−64→72 | 34 / 30 | tic39、55、162、178，各1组 | 4.865 ms | 0.185 / 0.086 ms |
| MAP06 floor87 / line622 special71 | floor0→−120 | 30 / 60 | 无；此原始事件布局稳定 | 4.062 ms | 0.191 / 0.072 ms |

以上每个动态帧在 map observer 与独立 frame-input trace 中均具有 geometry reset 1024、history 0；静止几何、lightlevel 和滚动 UV 帧的 geometry extraction / BLAS / TLAS 时间均为 0。MAP06 RT+SR 的 30 个变化帧均实际呈现 SR 并使用同帧 geometry reset；总计 186 帧实际 SR 呈现。这不是 NR 执行证据。

固定灯强度 1048576、半径 512。相同普通镜头中，实际 GPU receiver readback 与引擎导出普通 PNG 对应像素一致：

| 例 | 静态 receiver world X,height,Y | 关闭/高位 blocked RGB | 打开/低位 clear RGB |
| --- | --- | --- | --- |
| door | (834.572,−64,−147.331) | (36,36,36) | (88,88,88) |
| platform | (1100.669,−64,461.428) | (28,28,28) | (50,50,50) |
| floor | (718.354,−128,1960.437) | (80,60,36) | (127,97,61) |

这些 receiver 未饱和。门/平台返回原高度后遮挡恢复。观察时转向静态 receiver，运动表面在镜头后仍改变阴影；MAP06 在 tic48 尚无贡献、tic55 有6086个有贡献 sample、tic60/70 有21278个。每例 tic30/70 的阴影开/关普通 PNG 对比有差异，viewport 外边框/HUD 的360448个像素逐字节相同。

初建 MAP01 为207个非空组，scene buffer bytes1758840；门打开最高1760152，关闭回到1758840。MAP06 全程1035488。首次场景建组比每帧动态更新昂贵；本实现仍在几何变化时提取完整地图，尚未缓存 BSP 平面多边形来减少约4–5ms CPU 工作。

### 生命周期结果与截图边界

`lifecycle-04` 的 save-load **已完成子流程**：普通 Use、F6 选槽/输入名字保存，F9+Y 两次加载并分别等待 generation2/3 发布，Pause 保持高度28且无 AS work，Use 反向关闭（−4）再打开（4），菜单 New Game 建 generation4、恢复关闭高度−64，普通 F10+Y退出0。未改动的 DSG 经只读解析，sector27保存 floor−64/ceiling−24，其他210个扇区高度与原始 IWAD 全部一致；F6菜单观察高度−32与真正序列化相差一个正常 tic。同一 worker 后续的 exit leg 因自动输入时序/列索引未成功，不计入通过结果；换关依据为独立 `lifecycle-05`。

`lifecycle-05`：出生点 (216,−664),yaw90，普通 Use 原 MAP01 exit line774 special11，正常 intermission 后进入 MAP02 generation2，再由菜单 New Game 回 MAP01 generation3，普通退出0。`lifecycle-01` 独立 diagnostic F5 关闭场景请求，Use 改变门，再 F5 启用，首次恢复捕获当前 ceiling28；正常退出0。这验证 scene owner 的关闭/重新请求边界，最终玩家图形菜单开关仍由 #16/#17 验证。

生命周期 CSV、generation、地图编号、reset、保存文件及正常退出记录为此处依据。外部 `CopyFromScreen` 截图可能裁切游戏窗口或捕获其他前台窗口（尤其 `map02.png`），不能当作完整游戏 client、PNG parity 或玩家可见画面的证据。普通可见阴影依据正式矩阵的引擎 PNG 与 GPU readback。自动输入只向该测试进程拥有的窗口发送公开键盘事件。

保留了早期脚本失败及不同版本的 pilot，未将它们改写成通过：首次 metadata 文件名修正、ZIP时间导致 MSBuild 跳过补丁、DOOM wipe 期间输入时序，以及状态 CSV 列索引问题。正式矩阵重新 touch、编译并链接后验证了新 exe 摘要；生命周期换关改为真实 exit，未修改原商业版 idclev 的既有问题。

## 证据与限制

隔离远端目录 `C:\Users\awang\doom-spec-validation\dynamic-13-01`；正式矩阵 `selected-03.zip`、保留04/05前缀的 `lifecycle-final-preserved.zip`。本地协调目录中的 `dynamic-evidence-03`、`dynamic-lifecycle-final-evidence`、`dynamic-analysis.json`、`dynamic-save-analysis.json`、`dynamic-compiled-source-hashes.json` 保存明细。编译/构建输入206项由 root 独立核对，触碰补丁后的 MSVC 日志确实包含 map_source.c / dxr_map.cpp 编译及 original / SR 两次链接。

- 普通 exe SHA256：`8b828aab860174ba2168380c58af0e08b72566f9d07db85dbba9f988e868d2ae`。
- SR exe SHA256：`2beada48933979f025678969095bd5254e50b90d080db3095c57ad09ab92961f`。
- 普通 shader 保持 #12 lighting 摘要 `c3a0ab7d1d422918ef4e6cda75b9b354efd8b502d3cc020cf87e9a2b08cb273c`。

可移植 builder 及 final-world survey / lightlevel / idempotent UV / material / sky revision 检查通过 ASan/UBSan。门开启时主射线边界仍约1%的软件 scanline / GPU 可见性差异，平台距离匹配41472/41472，地板约99.96%；未宣称 scanline 与 ray-query 逐像素完全一致。alpha 裁剪/sky 次级射线与反射分别由 #14/#15 的共享 helper 承接；当前 #12 force-opaque 阴影路径并不代表这些功能已完成。资源增长结论仅覆盖已记录的有限回放与生命周期循环，非无限次运行证明。

### MID presence 与借用生命周期补充

正式 GPU 矩阵的应用代码仍严格为上述 `b67dba8`。后续补丁 `d55562eeb468eda4aca35939902ad555f831e777` 将 sidedef 的 MID 有效性加入 geometry hash：纹理0与纹理1即使解析高度相同，0→1也改变 masked topology，必须在同一帧递增 geometry/material revision 并请求 geometry reset。原版本已因 material revision 重新 capture / 更新 AS，但这个等高切换没有 geometry revision 分类，补充修正补齐了 reset 判定。公开 `tools/check-map-source.c` 使用两个等高纹理覆盖这个边界，同时确认 mapping revision 不变。

借用的 `DxrMapSceneView` / `MapSurface` 生命周期注释已明确为下一次 `Prepare` 或卸载；`GetSurface` 通过当前 canonical tuple range 查找，不能跨 `Prepare` 保留指针。`MapMesh` 是 owned current-map snapshot。

补充源的公开 CPU 检查以 GCC ASan/UBSan 重新编译，六项断言全部通过，记录在协调目录 `dynamic-followup-presence01-cpu.json`。独立 clean native 编译在远端新目录 `dynamic-13-01\followup-presence01` 完成：普通版和 SR 版均成功。两份 MSVC 日志均包含 `map_source.c` / `dxr_map.cpp` 编译及链接；上传的271项 tracked 非 third_party 文件与远端源 SHA256 全部相同，无缺失或不匹配。证据下载为 `dynamic-followup-presence01-native.zip`，比对记录为 `dynamic-followup-presence01-native-analysis.json`。SDK以已有 NGX 离线安装重新校验；未覆盖原矩阵源文件或二进制。

- 补充普通 exe SHA256：`13e1f6f5740ecc1bc8923ead232600ec0d40c984be0c3cc40796d11ad5df227f`。
- 补充 SR exe SHA256：`e519b742296dbde4b8b069b189a8c1fac6575397e06109eae836818d2c4ec2bd`。
- lighting shader SHA256仍为 `c3a0ab7d1d422918ef4e6cda75b9b354efd8b502d3cc020cf87e9a2b08cb273c`。

这些是 CPU 与原生编译证据，不与 `interactive-03` 的 GPU 证据混用。此补充版本未单独启动游戏，#14/#17 的整合 GPU 验收必须包含 `d55562e`。
