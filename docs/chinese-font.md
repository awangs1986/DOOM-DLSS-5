# 简体中文位图字形资源

`languages/zh-cn.cjk` 是游戏运行时使用的稀疏、固定 12×12 像素、1bpp 字形子集，不是系统字体文件。当前子集含 289 个码点，覆盖 `languages/zh-cn.ini` 的全部非 ASCII 字符。中文字形的行间距和 advance 为 12px；英文和 ASCII 标点仍由 DOOM 原有 `hu_font` 绘制和测量。有效字体中意外遇到的单个未收录码点显示一个 `?`。字体资源缺失、损坏或未覆盖译文所需字符时，启动记录原因并整体回退内建英文，不会以问号铺满中文包。

资源仅从程序目录下的 `languages/zh-cn.cjk` 读取，独立于当前工作目录。格式包含固定魔数、版本、字形数量、12×12尺寸/advance、索引/数据偏移、总长和 CRC32；码点索引严格递增，每个条目须为有效非 ASCII Unicode scalar，偏移必须连续且字形掩码恰为 18 bytes。加载时检查文件尺寸上限、索引、偏移、重复/无效码点、尺寸、advance、CRC 和所有译文码点覆盖。任何校验失败都会禁用该语言包并安全回退英文。

## 来源、许可与生成

位图来源为 Noto Sans CJK Regular 字体集合的简体中文 face（TTC face index 2，face name `Noto Sans CJK SC`）。来源是 Debian `fonts-noto-cjk` 版本 `1:20240730+repack1-1` 中的 `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc`，该源文件 SHA-256：

`b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a`

生成参数：FreeType 2.13.3、10px FreeType raster size、12×12 tile、以 96/255 灰度阈值转 1bpp、advance 12px、基线 10px。生成器对每个 glyph 检查非空墨迹和边界，任何一个墨迹像素落在 tile 外即失败。游戏运行时不链接 FreeType，也不读取源字体或依赖系统已安装字体。

仓库携带的位图是字体软件衍生品，按 SIL Open Font License 1.1 发行；完整许可与版权声明位于 [`languages/FONT-OFL-1.1.txt`](../languages/FONT-OFL-1.1.txt)。发行时须随字体保留该声明和许可，并继续按 OFL 发行位图衍生品。OFL 的 Reserved Font Name 条款仅适用于许可中明确声明的保留名称；上游随 Debian 包提供的许可未声明 Reserved Font Name。本项目将生成资源命名为 `zh-cn.cjk`，不把 Noto 名称作为衍生字体名称。

可用含该确切源文件的 Debian 字体包、Python 3 和 FreeType 2.13.3 在 Linux x86-64 离线重建；生成器通过 ctypes 调用 FreeType C ABI，不需要 Pillow 或系统字体安装步骤。例：

```sh
python3 tools/generate_cjk_font.py --font /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc
```

源 SHA 不匹配或 FreeType 版本不同会拒绝生成，避免静默漂移。生成后可用 `sha256sum languages/zh-cn.cjk` 记录产物摘要。

## 显示范围

字体仅覆盖当前中文包译文中的 289 个非 ASCII 码点；它不替换未迁移的图片菜单、关卡文字、剧情、地图名、聊天宏或存档槽输入。字体实际显示效果仍需在 Windows 游戏实机上检查；文件生成、校验通过或代码路径接通均不能作为画面可读通过证据。
