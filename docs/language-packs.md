# 外部语言包（schema 1）

启动参数 `-lang es-ascii` 加载程序所在目录的 `languages/es-ascii.ini`，替换退出确认提示。成功选择写入同目录 `language.cfg`，下次启动沿用；`-lang en` 恢复内建英文。优先级为启动参数、保存选择、英文。语言只在启动时加载；重新选择需重启。文件查找与启动工作目录无关。程序目录不可写时，本次选择仍有效，日志报告保存失败。

首版只迁移 `quit.prompt` 和 `quit.confirm`；其他文字继续使用原定义，菜单图像未替换。退出键保持 Y，示例也据此提示。UTF-8 文件加载并不提供中文字体。原 DOOM 字体显示可打印 ASCII，小写使用已有大写字形；退出提示将每个不支持的 UTF-8 字符显示为一个 `?`，换行、限定行数和截断保障安全。示例西班牙语使用 ASCII，不带重音。

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
