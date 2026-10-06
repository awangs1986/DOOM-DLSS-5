# 场景与二维叠加分层验证

对应 [ticket #9](https://github.com/awangs1986/DOOM-DLSS-5/issues/9)。逻辑画面保持 320×200，最终 backbuffer 为 1280×800。

## 实现契约

- 在世界遮罩精灵绘制完成、玩家武器绘制前保存场景索引颜色。武器之后的列绘制及前台 patch、矩形复制记录明确二维覆盖，不通过颜色差异或零深度猜测覆盖。
- `GB_ColorRGBA()` 是不可变的本帧场景颜色；`GB_OverlayRGBA()` 是完整软件帧颜色；`GB_OverlayMask()` 决定最终合成的二维像素。
- `GB_SceneMask()` 只表示有效世界表面，排除天空和视窗外填充。武器后方保留真实世界颜色、深度、法线和运动，因此有效世界遮罩与二维覆盖遮罩允许重叠。
- 视窗外场景输入使用边缘颜色、8192 的无效远深度与零运动。Insert 保留原行为：隐藏状态栏及视窗外边框，武器、菜单和提示继续显示。
- 帮助、自动地图、标题、过场、结算和 wipe 不作为三维场景超分，使用完整软件帧并清除场景历史。
- 普通颜色和成功的超分路径均先完成武器/UI 合成再导出 PNG；PNG readback 来自随后呈现的同一 backbuffer。深度、法线、运动矢量调试图检查底层场景，不合成彩色 UI。

## 2026-10-06 实机验证

在已确认的 RTX runner 登录桌面 Session 2，使用 RTX 5070 Ti Laptop GPU、Visual Studio 2022 x64 Release、Windows SDK 10.0.26100.0 构建 original 和 SR，均成功。运行官方 `nvngx_dlss.dll` 文件/产品版本 `310,9,1,0`：

```text
SHA256 3975567B8943C53ACCE397F2B72380092F84F162D00B0D2C7D08A1025C563983
```

固定 Freedoom2 0.13.0 的 210-tic demo，同输入分别运行 color、SR、depth、normal、velocity、scene-mask、overlay-mask 七次回放。每次退出码 0，各导出 251 PNG。SR 日志包含 `present mode: dlss-upscale` 和 `DLSS SR created (l)`；没有把最近邻回退当作 SR 验证。

按各次 `gpu-timing.csv` 的真实 `game_tic` 对齐 35、70、175，使用 `overlay-mask` 白色像素比较普通颜色与 SR 导出的 RGB：

| game_tic | 二维覆盖像素数 | 覆盖内不同像素 | 覆盖外不同像素 |
| --- | ---: | ---: | ---: |
| 35 | 375904 | 0 | 246020 |
| 70 | 371792 | 0 | 176113 |
| 175 | 366432 | 0 | 418160 |

三个遮罩都是纯黑白。场景经过 SR 而二维覆盖逐像素一致；深度、法线及运动图可见武器下方真实世界，视窗外填充没有有效几何或运动。

另外 original 和 SR 各完成一次真实键盘交互：暂停、Insert、缩小视窗、Freedoom1 的 Read This HELP、自动地图上的主菜单、退出确认，均退出码 0。实际截图显示武器、PAUSE、边框、状态栏与提示正确；HELP 保留完整软件页面，自动地图和菜单清晰。截图名 `*-menu.png` 未保证捕捉到预期 Options 状态，不作为该状态证据；已确认的菜单证据是 `*-automap.png` 和 `*-quit.png`。

tic 175 的 GPU command-list 总时间为 nearest 0.328512 ms、SR 0.705984 ms，其中合成分别 0.014048 ms、0.026784 ms。这些单帧时间包含导出 GPU copy，不代表无导出的整机游戏帧时。

## 可移植测试与证据边界

`tests/layering_test.c` 链接实际 `V_DrawPatch`、`V_DrawPatchFlipped`、`V_CopyRect` 和 `V_DrawBlock`。ASan/UBSan 运行通过，覆盖相同颜色的显式覆盖、透明缺口、离屏 scratch 绘制、武器后方真实颜色/深度、无效填充、Insert、nearest 完整合成、调试图及非场景回退。旧引擎全局未参与 ASan instrumentation，以便链接裁剪无关的旧引擎代码；被测函数和 G-buffer 仍带 sanitizer。

原始证据保存在本次隔离运行目录的 `evidence-01`，包含构建日志、源码/二进制/输入摘要、worker、运行日志、CSV、完整 PNG 和交互截图。复核的七个编译源文件 SHA256 与本次提交代码一致。交互截图受 Windows DPI 和外部输入工具浮层影响，不用于逐像素比较；上表使用实际导出 backbuffer。

本次真实 GPU 验证覆盖 nearest 与 NGX SR。FSR2/Anime4K 共用显式覆盖合成路径，但本次没有单独构建或运行这两个后端。后续输入校准及光追工作仍需使用本接口和自身验收。
