# 静态实体地图点光源与硬阴影（#12）

## 使用

Windows x64、DXR 1.1 / Shader Model 6.5 硬件适配器，保留构建生成的 `shaders/dxr_lighting.cso`：

```powershell
.\windoom.exe -iwad .\freedoom2.wad -warp 1 -rt-light 288 -240 0 1 1 1 65536 512
# 同一固定灯光，关闭遮挡射线进行对比
.\windoom.exe -iwad .\freedoom2.wad -warp 1 -rt-light 288 -240 0 1 1 1 65536 512 -rt-shadows-off
```

`-rt-light` 八个值依次为 DOOM `x y height`、线性 RGB、强度、距离上限。GPU 坐标为 `(x,height,y)`。RGB 限于 `[0,1]`，强度 `[0,1e8]`，距离 `(0.0625,8192]`；所有值必须有限，位置绝对值不超过 `1e8`。最多一盏灯，每帧最多 64000 条遮挡射线。灯固定在地图坐标，不跟随相机。

`-rt-light-off`、`-nort`、配置无效、硬件能力不足、着色器或资源创建失败保留原场景。整个 D3D12 设备移除沿用现有致命错误处理，没有新增设备重建。`-rt-light-stats <新的.csv>` 记录预算和实际 GPU 结果；`-rt-light-pixels <新的.csv> -rt-light-pixel-tic 175` 记录指定 tic 的世界位置、材质、距离、visibility、原始与最终颜色。观察文件拒绝覆盖已有路径。按 `gpu-timing.csv` 的 `game_tic` 对齐 PNG。

## 场景、材质与合成

- 主显示使用真正的点光源 shader 改写场景颜色，再交给现有 SR/FSR2/Anime4K 或最近邻显示；硬阴影不是诊断替换画面。成功点光源在 SR 回退时仍可用最近邻显示。
- 世界材质样本在实际 `R_DrawColumn` / `R_DrawSpan` 内记录：同一个源索引、实际 COLORMAP 后的索引、wall/floor/ceiling 类别与材质 ID。没有从最终 gamma / COLORMAP RGB 反推材质。
- `-albedo` 使用真正的 PLAYPAL palette0 与源索引，未经 COLORMAP/gamma；`-material-mask` 显示有效接收者。sky、world sprite、masked 中墙、武器和 UI 不作为本票的接收者，特殊 fixed COLORMAP 禁用新照明。武器绘制后不再改变场景材质样本。
- 世界位置使用共享 `GB_SampleWorldPosition` 的实际软件采样与地图单位深度。墙法线统一为 front sector 的 inward 法线；地板 +Y、天花板 −Y。不做相机 faceforward。地图 GPU 绕序保留历史方向，射线不剔除背面。
- 阴影只追踪共享完整静态地图 TLAS 的实体三角形，force opaque + accept first hit。射线起点沿物理法线偏移 `0.03125`，TMin 为 `0.03125`，TMax 为灯距离减 `0.0625`；最多 64 次 Proceed，耗尽时保守遮挡。
- 武器、HUD、菜单、边框通过 #9 的明确 overlay coverage 在所有场景处理之后合成。HELP、automap、wipe 和非场景帧不执行点光源。没有将零深度或颜色差异当作二维覆盖判据。

新增照明在线性色彩中计算。当前选中的原始 PLAYPAL（gamma 前）决定 source albedo 和已经经过一次原始 COLORMAP 的 ambient：

```text
B = srgb_decode(active_PLAYPAL[ambient_index])
A = srgb_decode(active_PLAYPAL[source_index])
falloff = saturate(1-distance/radius)^2 / max(distance^2,256)
direct = A * linear_light_RGB * intensity * falloff * max(dot(N,L),0)
output = classic_gamma_LUT[round(255 * srgb_encode(saturate(B + visibility*direct)))]
```

遮挡只删除新 direct 分量，不删除原版 ambient；没有把最终场景再次当作未照明材质。pain/bonus/radiation 调色板按同一 active PLAYPAL 染色 direct，是明确的兼容近似；不会把这些调色板反推为完整物理材质。无 direct 贡献时保留原场景打包颜色字节，避免 gamma 往返漂移。

## 共享材质基础

`r_material.h` 公共导出纹理/flat 描述、动画翻译、COLORMAP 及复制出的 indexed color + 独立 binary alpha。八字节 WAD 名称另加终止符；source index0 可以不透明。纹理按照原 patch 顺序和 placement 合成，post 缺口不擦除此前 patch。校验整个 patch 后才修改输出；整张纹理失败时调用者丢弃临时数组。经典绝对 topdelta 行为，不扩展 Boom tall-patch 语义。所有 zone/WAD 指针在调用内消费，不保留。

`RtMaterials_Prepare` 是后续 alpha/reflection 共用的被动材质 catalog：墙与 flat 名字空间独立，descriptor 保存 base/resolved ID、尺寸、width mask、pixel offset/count；GPU pixel 低字节为索引、下一字节为 alpha。同一名字空间和 resolved ID 的像素块去重，动画翻译改变时重建。预算为 4096 descriptors、16M pixels；复制或预算失败关闭 catalog，已采样的实体点光源独立继续。

它目前跟踪关卡 generation 与已有 descriptor 的动画翻译。动态世界改变 active surface / base material 的同 generation 同步，由 #13/#14 增加 material revision / referenced-material invalidation；不声称本票已经覆盖这一行为。离屏 ambient 的 current sector lightlevel 也是后续 secondary-hit 着色的数据需求；本票 primary-hit ambient 始终使用真实软件 COLORMAP 样本，不用标量猜测替代。

## 2026-10-07 实机验收

RTX 5070 Ti Laptop、驱动 617.14、当前签入 Windows 交互 Session3；MSVC2022 x64 Release / Windows SDK 10.0.26100.0 / DXC。普通与官方 NGX SR 两种构建成功。普通 exe SHA256 `fa8436debaffd472409a9c620235b1a9f7f45d7ce968347acad579aa08398c27`；lighting shader `c3a0ab7d1d422918ef4e6cda75b9b354efd8b502d3cc020cf87e9a2b08cb273c`。

官方 Freedoom 0.13.0 MAP01 两份隔离 IWAD 只修改原 player1 THINGS 的位置/角度；所有地图几何、BSP 和材质字节不变。cameraA `(448,-208),225°`，cameraB `(480,-192),225°`；220 个普通静止 vanilla ticcmd。灯始终 `(288,-240,0)`，半径512，强度8192及65536分别验证。25 次普通回放全部 exit0，涵盖两相机灯关/无阴影/有阴影、albedo/material/overlay/depth/normal、SR、gamma4、红灯、radius64、NaN 配置、缺 lighting shader、缺 map diagnostic shader。

强度65536、tic175 的 GPU 结果：

| 相机 | eligible | rays | blocked | contributed | 固定灯 GPU 坐标 |
| --- | ---: | ---: | ---: | ---: | --- |
| A | 41472 | 32668 | 21089 | 11543 | (288,0,-240) |
| B | 41472 | 34182 | 19260 | 14882 | (288,0,-240) |

A 的实际地板样本 `(368.0533,-64,-303.8305)` 和 `(399.1668,-64,-304.3723)` visibility0，final 字节等于 original；`(413.5618,-64,-290.3369)` visibility1 增加 direct。已人工检查普通最终 PNG，墙角左侧/中心原 ambient 保留，右地板受光；关闭阴影时原阻挡区域也变亮。B 使用同一灯光并确认遮挡分区随视角投影改变。

- tic175 的 381056 个最终 overlay 像素在普通有阴影、无阴影、SR 中与灯关完全一致。`-nort`、NaN 配置、缺 lighting shader 的全帧与灯关完全一致。
- 缺 `dxr_map.cso` 时普通强阴影全帧与正常强阴影完全一致；普通消费者不依赖地图诊断 pipeline。
- radius64 的 35333 个距离不小于64的 eligible 样本 final==original。gamma4 的 21089 个 blocked 样本保留原 gamma ambient。
- 独立读取真实 WAD PLAYPAL 和原 gammatable，对五组各41472个 GPU samples 核对色彩公式：ambient 原值错误0；强白、gamma4、红、radius64 的最终值与 CPU 公式完全一致，弱白最大1LSB（CSV 浮点精度/DXC运算差异），无贡献样本全部字节保持。
- inward 法线地图诊断复核：41472 hits / eligible，41362 normals 在每通道±2内，41413 depths 在 `max(0.5,1%)` 内。软件 scanline 与几何边界仍有取整差异，未声称所有边缘像素等同或无自阴影瑕疵。
- copied atlas 成功：75 wall/flat descriptors，658944 indexed-alpha pixels，2638176 GPU upload bytes。没有启用 alpha/reflection。
- SR 实际 Evaluate 219次成功、0失败。官方 DLL `310.9.1.0` SHA256 `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983`。这是 SR 证据，不是项目 NR 激活/执行证据。

| tic175 路径 | command list GPU ms | rt_ms | sr_ms | DXGI process local memory bytes |
| --- | ---: | ---: | ---: | ---: |
| 灯关 | 0.368064 | 0.000064 | 0.000064 | 34619392 |
| 无阴影，强度8192 | 0.590560 | 0.258880 | 0.000160 | 95985664 |
| 有阴影，强度8192 | 0.572800 | 0.250880 | 0.000160 | 95985664 |
| 相机B有阴影，强度8192 | 0.630720 | 0.316416 | 0.000128 | 95985664 |
| SR+有阴影，强度8192 | 0.958848 | 0.253504 | 0.385856 | 176439296 |

`rt_ms` 包含照明 dispatch、场景 copy、观察 GPU readback copy，不能解释为纯 ray trace；总 GPU 时间包含 PNG export copy。显存是 DXGI 当前进程 local usage，不是系统总用量。这些固定单帧值不构成无导出游戏性能或长期泄漏压力测试。

共用 include 仅更新时间、不修改源码内容后，map 与 lighting 两个 shader 均重新生成且哈希保持。`build.cmd --original` 实际构建与 staging 成功，staged lighting shader 与已测试 shader 哈希一致。192 个下载的代码/构建文件摘要与本次源代码复核一致；完整日志、CSV、worker、PNG 和 hash 保存在隔离 `lighting-12-01/interactive-01..03` 及协调目录的 `lighting-evidence*`。

## 可移植验收

`tests/material_copy_test.c` 通过实际公共 post copier 验证不透明零索引、缺口、placement/clipping、无效目录、截断post、尺寸/容量。`tests/material_sample_test.c` 链接实际 column/span 验证 raw 与 mapped 索引、原软件像素、palette/gamma 独立、武器覆盖不修改材质、fixed COLORMAP 禁用接收者。两者 ASan/UBSan 通过；旧引擎 globals 的 sanitizer instrumentation 按已有 GC fixture 方法关闭，被测函数仍带 sanitizer。原 `layering_test`、frame motion/device-depth 公共契约、map mesh builder 验证也通过。

```sh
gcc -std=c99 -g -fsanitize=address,undefined -Ilinuxdoom-1.10 tests/material_copy_test.c linuxdoom-1.10/r_material_copy.c -o /tmp/material-copy-test
/tmp/material-copy-test
gcc -std=c99 -g -fsanitize=address,undefined --param=asan-globals=0 -ffunction-sections -fdata-sections -D_WIN32 -DNORMALUNIX -DLINUX -include strings.h -Ilinuxdoom-1.10 -Iwin32 tests/material_sample_test.c linuxdoom-1.10/r_draw.c linuxdoom-1.10/m_fixed.c win32/gbuffer.c win32/frame_inputs.c -Wl,--gc-sections -lm -o /tmp/material-sample-test
/tmp/material-sample-test
```

本票只交付静态实体硬阴影及共享复制材质基础。移动扇区、alpha 中墙/sky 的次级射线策略、reflection、软阴影、去噪和菜单分别属于后续票。FSR2/Anime4K 沿用共享场景输入/overlay 合成，但本轮未单独构建运行。
