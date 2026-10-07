# 外部语言包（schema 1）

启动参数 `-lang es-ascii` 加载程序所在目录的 `languages/es-ascii.ini`，替换退出确认提示。成功选择写入同目录 `language.cfg`，下次启动沿用；`-lang en` 恢复内建英文。优先级为启动参数、保存选择、英文。语言只在启动时加载；重新选择需重启。文件查找与启动工作目录无关。程序目录不可写时，本次选择仍有效，日志报告保存失败。

首版迁移退出、Messages 开关反馈、gamma 提示、物品拾取反馈和快速保存/加载确认。Options 菜单的 Messages 项、开关值和细节档标签在对应译文存在时走文字绘制入口；缺键和英文默认仍使用原菜单图片。菜单位置、选择键、选择行为与其他图片保持原样。退出键保持 Y，示例也据此提示。UTF-8 文件加载并不提供中文字体。原 DOOM 字体显示空格、`!` 到 `_` 以及英文字母，小写使用已有大写字形；菜单和 HUD 文字将每个不支持的 UTF-8 字符显示为一个 `?`，换行、限定行数和截断保障安全。示例西班牙语使用 ASCII，不带重音。

## 格式

```ini
[pack]
schema=1
[strings]
quit.prompt=Seguro que quieres salir\nde este gran juego?
quit.confirm=(pulsa y para salir)
```

- UTF-8，可有开头 BOM，支持 LF 或 CRLF。
- 恰好一个 `[pack]` 和一个 `[strings]`；元数据只接受 `schema=1`。空字符串目录可用，用于全部回退。
- 键是 1–64 字节的小写 ASCII、数字、点或下划线；稳定键不随英文文案改变。允许未来文本键，但当前消费者只查询已接入键。
- 值非空，首尾空格/制表符会移除。支持 `\n`、`\t` 和 `\\` 三种转义，其他转义拒绝；值不支持引号语法和行内注释。独立空行、`;` 或 `#` 开头的注释可用。
- 文件上限 65,536 字节，最多 256 个键，解码后单值上限 2,048 字节。重复键或段、未知版本、非法 UTF-8、NUL、非法控制字符和超限输入使整包拒绝，日志说明原因并整体回退英文。完整解析成功后才发布，缺少某个键则只对该键回退。
- 语言标识最多 32 个小写 ASCII 字母、数字、`-` 或 `_`。不接受路径，因此 `-lang` 不会加载任意文件；`en` 是保留的内建语言。
- 退出正文最多五行，确认说明最多两行，各 30 个字符；超出内容截断。包文本从未作为 `printf` 格式串执行。

## 开发接口与验证

`Lang_Parse`、`Lang_Lookup`、`Lang_Free` 提供独立目录接口。解析错误返回空指针和诊断；成功目录的字符串只读，直到 `Lang_Free` 前有效。游戏目录只启动一次，`Lang_Text` 返回稳定指针直到 `Lang_Shutdown`；无热加载。`Lang_QuitMessage` 是退出提示消费者使用的公开边界，进行字体转换和长度限制。

`tests/language_test.c` 验证公开解析、查找和提示接口，`tests/test_language_startup.py` 通过真实文件和新进程验证启动选择、保存优先级、工作目录无关和坏包整体回退。它们只需要 C99 编译器和 Python 3，不依赖 GPU。Windows 编译和实际退出画面验收仍需游戏实机验证。

可在仓库根目录运行以下命令；测试程序和临时包使用 `/tmp`，不会修改实际游戏语言设置：

```sh
cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Ilinuxdoom-1.10 linuxdoom-1.10/language.c tests/language_test.c -o /tmp/windoom-language-test
/tmp/windoom-language-test
cc -std=c99 -Wall -Wextra -Werror -Ilinuxdoom-1.10 linuxdoom-1.10/language.c tests/language_startup.c -o /tmp/windoom-language-startup
python3 tests/test_language_startup.py /tmp/windoom-language-startup
```

## 已迁移键与消费者

| 文字 | 稳定键 | 行为 |
| --- | --- | --- |
| 退出正文、确认 | `quit.prompt`、`quit.confirm` | 实际退出弹窗 |
| Messages 菜单项 | `menu.messages` | 对应译文存在时使用原字体，最多 13 字符；其他菜单项仍绘制原图片 |
| Messages 开关反馈 | `option.messages.on`、`option.messages.off` | 点击选项或 F8 后的实际 HUD 提示 |
| 开关标签 | `option.state.on`、`option.state.off` | Options 菜单状态文字，最多 12 字符 |
| 细节标签 | `option.detail.high`、`option.detail.low` | Options 菜单标签，最多 8 字符；原代码的 low detail 渲染仍未实现 |
| gamma 提示 | `option.gamma.0` 到 `option.gamma.4` | F11 切换后的实际提示 |
| 快速保存、加载 | `quicksave.prompt`、`quickload.prompt`、`prompt.yes_no` | 实际保存名称作为受限 TEXT 参数，退出/确认键未修改 |
| 所有拾取反馈 | `pickup.*`（见下表） | 保持原拾取规则与副作用，仅替换提示 |

拾取键的后缀为：`armor`、`mega_armor`、`health_bonus`、`armor_bonus`、`supercharge`、`mega_sphere`、`blue_card`、`yellow_card`、`red_card`、`blue_skull`、`yellow_skull`、`red_skull`、`stimpack`、`medikit_needed`、`medikit`、`invulnerability`、`berserk`、`invisibility`、`radiation_suit`、`map`、`light_visor`、`clip`、`clip_box`、`rocket`、`rocket_box`、`cell`、`cell_box`、`shells`、`shell_box`、`backpack`、`bfg`、`chaingun`、`chainsaw`、`launcher`、`plasma`、`shotgun`、`super_shotgun`。

图形菜单新增稳定键，英文内建目录和 `es-ascii` 示例包均提供文字；NR 身份仍与 SR、DLAA 分开。`graphics.reason.none` 是空内部哨兵，不写入语言包（包格式拒绝空值）。

| 文字 | 稳定键 |
| --- | --- |
| Options 入口、图形标题 | `graphics.title` |
| 三项标签、实际状态标题 | `graphics.rt`、`graphics.sr`、`graphics.nr`、`graphics.actual` |
| 实际状态 | `graphics.state.off`、`pending`、`active`、`unavailable`、`fallback`、`unverified`、`unknown`、`paused`（后七项同 `graphics.state.` 前缀） |
| 正常关闭、等待、场景暂停 | `graphics.reason.off`、`pending`、`non_scene`（同 `graphics.reason.` 前缀） |
| RT 能力、配置、局部失败 | `graphics.reason.no_dxr`、`no_effects`、`rt_partial`、`rt_failed`、`effects_disabled` |
| NGX 未编译、运行库、初始化及执行 | `graphics.reason.not_compiled`、`runtime`、`init`、`capability`、`create`、`evaluate`、`hires`、`carrier`、`legacy_rr` |
| 可选 NR 后端和确认限制 | `graphics.reason.backend_missing`、`restart`、`unsupported`、`confirmation_pending`、`confirmation_failed`、`execution_unverified`、`no_input` |
| 偏好诊断 | `graphics.reason.save_failed`、`prefs_invalid` |

上表省略前缀的后缀均与本行首键使用相同前缀；键本身不包含显示文字。请求的开关值复用 `option.state.on/off`。图形入口、标签、值、实际状态和原因均先做字体转换，再按真实 glyph 像素宽度换行或截断。原因最多四行；有保存错误时保留两行原因和两行保存诊断，正文限制在状态栏上方。缺键逐字段回退英文，坏包仍整体回退。详细操作和配置优先级见 [图形菜单](graphics-menu.md)。

2026-10-07 用户豁免当前 DLSS5／NR 功能验证。翻译键和原模式保留，NR 的 consumer、checkbox 或 GPU 执行不因文字存在而被声明通过。当前补充中文显示任务由 [中文显示扩展](chinese-display-task.md) 单独记录；初始 ASCII 包及初始字体限制的证据不能代替实际 CJK 字形验收。

未迁移文字包括其余菜单图片/标题、关卡和剧情、地图名称、聊天宏、门锁提示、普通保存完成提示、网络/新游戏/难度错误提示和存档槽名称输入。原英文宏仍可供静态初始化与历史 Linux 构建使用。首版不扩展聊天或存档名称的输入字符范围。

## 受限模板

仅 `quicksave.prompt`、`quickload.prompt` 接受模板；声明一个名为 `save_name`、类型为 TEXT 的参数，模板必须恰好包含一次 `{save_name}`。`{{` 和 `}}` 显示字面花括号。百分号没有格式意义，所以 `%s`、`%n` 等均按普通文本复制，参数内容中的花括号也不会再次解释。

公开 `Lang_Format` 校验参数名、数量、类型、UTF-8 和长度（最多 128 字节），拒绝控制字符。译文模板错误时回退英文模板并保留有效存档名；参数错误时回退英文并显示 `<unknown save>`，输出明确诊断。输出截断只发生在完整 UTF-8 码点边界。游戏的 `Lang_SaveMessage` 单独保留正文与 Y/N 提示的空间，长正文不能吞掉确认说明。`M_StartMessage` 拥有稳定的显示副本，最多八行、每行 30 字符；HUD 使用相同字体转换，最多一行，不将原始 UTF-8 逐字节大写。

## 消费者验证

`python3 tests/test_language_consumers.py` 使用 GCC 构建真实 `M_ChangeMessages`、`P_TouchSpecialThing`、HUD 和菜单提示/文字消费者，在隔离目录用英文与示例包验证选项切换、拾取规则和反馈、UTF-8 缺字一致性、长提示长度与显示副本的生命周期。仅平台声音/绘制输出与对象释放在链接处替换；不复制引擎实现。启用 ASan/UBSan 的堆栈/堆检查，关闭全局 ASan 注册以允许链接器裁掉无关引擎功能。

Windows 实际游戏验收：用 `-lang es-ascii` 启动，打开 Options 检查 `MENSAJES` 和 `SI/NO`，按 F8/F11 检查译文反馈；进入地图拾取护甲/血瓶/弹药，检查提示和游戏状态。建立名称为 `MY SAVE` 的存档，再按 F6/F9，确认西班牙语正文、名称插入和 Y/N 提示，按 N 取消。重启 `-lang en` 比较默认英文。长译文/错误模板可在独立测试包中验证并保存截图，不修改正式包。实际回放与截图证据由实机验收汇总。
