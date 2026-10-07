# #16 图形菜单验收记录

## 版本与证据范围

测试核心源码：`9c762acf1a237d9e91ff089e29da1f87dd04626a`。fresh v5 包含全部 316 个 tracked blob；远端逐文件 SHA-256 与该提交及打包 manifest 一致。VS2022 x64 Release 原版和官方 NGX SR 均编译、链接成功。原版 exe SHA-256 为 `839962c5635f4ea091d321e0dcaf1b0d338c02ab352f8609d35176c2faf63ea8`；SR exe 为 `f64d75af88a854bf876a04c77e68d02a222d5ce430255a3d49c66af654f1c37e`。

各构建日志有 503 条既有编译警告，主要来自遗留 DOOM C 代码、宏和 `ngx_dlss.h` 编码；未声称整个项目无警告。`m_menu.c` 无警告，`d_main.c` 仍有旧指针转换警告。实际 NR 控制后端的注册／卸载／checkbox／GPU 执行正例不在本次证明范围。

Linux 原始材料保存在 `/home/awang/tmp/doom-implementation-g6ir9es3/`，独立审查应读取原始数据，而非仅凭报告结论：

| 材料 | 内容 |
| --- | --- |
| `menu-source-identity-v5.json` / `menu-source-hashes-v5.json` | 完整提交、包身份及 316 输入哈希 |
| `menu-native-v45/` | 原生 v4/v5 原始日志、结果及远端实际输入哈希 |
| `menu-layout-v5-01/` / `menu-layout-v5-01-analysis.json` | Options→Graphics→关闭、保存失败提示、物理窗口／HUD |
| `menu-interactive-v5-02/` / `menu-interactive-v5-02-analysis.json` | 最终真实键盘矩阵、CSV、截图、偏好、焦点／输入／启动身份、GPU 统计 |
| `menu-failures-v5-01/` / `menu-failures-v5-01-analysis.json` | 8 条 matched-tic 故障／颜色路线 |

NGX 编译 SDK 为 `v310.9.1` / `374959484e79a640feaba44c93ac8cfb0a03f5b5`，SR DLL 文件版本为 `310.9.1.0`，SHA-256 为 `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983`。SDK 身份、DLL 身份和实际 feature 执行分别记录；此信息不证明 NR。

## 真实菜单、独立请求和重启

`interactive-v5-02` 13 个进程均通过主菜单 Quit、paired Y 正常退出 0。使用 x64 40-byte `INPUT` 的 278 次原子 `SendInput` down/up pair，全部返回 2；原生窗口收到 556 个 WM 事件，无 VK229，HKL 保持 `0000000008040804`。每次输入前后核验 owned PID／start／HWND／foreground。每次 launch 新查唯一 active `awang` session；初次焦点先分类，只允许游戏自身、owned console 或 Explorer desktop；其他用户应用时中止。必要时仅一次有 `finally` detach 的 `AttachThreadInput` 事务。

最终 worker 每次非终端按键及截图等待至少 3 个新的导出帧，不读取仍被排他打开的观察 CSV。截图携带 engine frame，退出后按该 frame 对齐观察状态，所有期望请求值一致。

- 上／下选择、左／右／Enter 切换、Backspace 返回 Options、Escape 关闭及重新进入已实际使用。
- 同进程执行 `00→10→11→01→00` 及反向 RT／SR 路线，每种组合分别 NR off／on。四种 RT／SR 的实际 off／active 和官方 SR evaluation 与请求一致；46 个单项请求转换在首应用帧带 history reset bit 512。
- 所有 NR on 都保留独立请求并显示 consumer 未加载／不可用；没有把 SR execution 当作 NR activation。`consumer_loaded=0`、`nr_execution_verified=0`。
- 同 profile 保存 `rt=1,sr=0,nr=1` 后重启恢复；CLI `010` 覆盖不写偏好。再用 CLI 仅覆盖 SR，同时菜单只改 RT，保存结果 `rt=0,sr=0,nr=1`，没有将临时 SR CLI 值写入。独立 profile 使用自己的模式默认值。
- 原版未编译 NGX 时，SR 请求保留并明确 unavailable／not_compiled；RT 仍 active。DLSS5 模式默认请求 NR，但缺消费者仍显示不可用，SR 成功不改变这一事实。
- 启动 wipe 等非 scene 帧可出现 paused／non_scene，和进入稳定游戏后的实际状态分开记录。

## 回退和重试

| 路线 | 实测结果 |
| --- | --- |
| 持续注入 SR init/create/evaluate/evaluate-late 故障 | SR off/on 重新检查后仍诚实 fallback，RT 保持 active；不是故障已解除的正例 |
| 测试目录缺 SR DLL，菜单 off，恢复该目录 owned DLL，on | 重新 init/create 后官方 SR evaluation 成功，RT 与 SR active |
| 测试目录缺反射 compose shader，菜单 RT off，恢复该 owned shader，on | 缺失时 RT fallback、SR active；恢复后 RT 与 SR active |

独立静态反射 fixture 使用相同输入，在 game tic 70 和 175 分别比较原生 RT、原生无 RT、SR 加 RT、四种 SR 故障及缺反射 compose shader，共 8 个进程，均退出 0。四种 SR 故障的 PNG **完整 SHA-256 与原生 RT 完全一致**，且不同于原生无 RT；这证明保留了 RT 合成颜色、weapon、HUD 和导出。SR 加 RT 的 observer 同帧为 reflection evaluation 1、官方 SR evaluation 1。缺 RT shader 时 reflection evaluation 0，官方 SR 继续 evaluation 1。故障注入是诊断构建中的 synthetic 分支，不标记为真实驱动故障。

既有 #13／#15 动态门、平台、透明、天空与反射验证材料由对应票记录；本轮没有重新声称已覆盖所有动态地图和 NR 正例组合。

## 完整窗口、HUD 与保存错误

两条独立原版路线均实际 Options→Graphics→Escape、显示 RT active 和通过菜单正常退出。另一路先建立 4097-byte owned 图形偏好文件，菜单编辑产生保存失败提示，原文件仍为原来的 4097 bytes；本次请求仍生效。

所有 11 个截图的物理 client 均为 1280×800，window awareness 2、DPI 192，完整窗口位于 2560×1504 workarea 内。菜单／保存错误文字没有再污染 HUD；关闭菜单后健康、弹药及状态栏标签完整。与 penultimate engine PNG 的 HUD 对比允许异步 face 动画差异，face 之外仅有 112 个固定 bottom-corner 像素差异（Windows 圆角，y786..799）；未宣称整个异步截图逐像素相等。多显示器 DPI 转换和较小工作区缩放路径尚未实测。

## GPU 阶段与显存

原始 `gpu.csv` 记录 upload、SR、compose、export-copy、RT、reflection trace/filter、local memory 与 budget。最终菜单矩阵含 8942 行 graphics observer，报告按每组实际请求聚合样本数、最小／中位／最大；缺 NR 后端的样本不能解释为 NR 性能测量。

例如 NR off 的稳定请求组 GPU 时间中位数为：RT/SR `00` 0.305 ms、`10` 1.522 ms、`01` 0.883 ms、`11` 1.917 ms。对应 local memory 范围依组为约 33–136、128–136、147–191、189–191 MiB；本次 local budget 11176 MiB。包含菜单、PNG 导出和读回，样本也受初始化／缓存及运行时调度影响，不推导用户帧率或 NR 增益。

## 未通过或未完成的范围

- 早期输入失败及 VK229 红／绿对照保留；失败截图不算菜单通过。
- v4 包实际对应 `999d4cd9...`，构建结果曾误标 `999d4cd0`，仅保留为初步材料；最终采用完整正确 revision 的 fresh v5。
- `interactive-v5-01` 曾在 SR 初始化期间取得旧截图，不能按截图文件名断言已应用；最终矩阵采用同步新帧的 `interactive-v5-02`。
- 本轮没有加载／注入／安装 NR consumer，没有 positive 注册、checkbox 回读、卸载／重载或 genuine NR GPU execution，也没有可选 carrier 分配故障的实机正例。`execution_verified` 始终为 0。
- 用户要求当前可以关机，全部测试退出且原始材料已下载后停止远端工作。5 个一次性 owned scheduled-task 定义的最终删除及全局无进程快照尚未复核；它们没有自动 trigger，保留定义不代表测试仍运行。后续打包和最终集成 GPU 验收待用户再次开机。
