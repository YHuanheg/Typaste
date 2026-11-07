# CLAUDE.md — Typaste

Win32 桌面程序（C++17，MSVC）：拦截全局热键 Ctrl+V，把剪贴板文本逐字符"打"进前台窗口。
目标系统 Windows Vista+；x64 为主，Win32 (x86) 兼容。GUI 为纯 Win32 对话框（无 MFC/Qt）。

## 目录与职责

| 路径 | 职责 | 改动注意 |
| --- | --- | --- |
| `Typaste.cpp` | WinMain、主窗口、托盘、热键、输入线程、剪贴板预览同步 | 不要在此加设置读写 |
| `Settings.cpp` | 设置数据、白名单判定、设置/白名单对话框 | UI 文案一律走资源 |
| `Config.cpp` | 存储后端：`Typaste.ini` 优先，注册表回退 | 新增键必须同时改 Load/Save |
| `Clipboard.cpp` | 剪贴板读写、编辑框"选中或全部" | — |
| `Ime.cpp` | 输入法状态读取/切换/恢复 | 只用 IMM32 消息，失败要能回退 |
| `Dpi.cpp` | 96 DPI 基准布局记录 + `WM_DPICHANGED` 重算 | 新增控件自动纳入，勿手写缩放 |
| `Lang.cpp/.hpp` | 按 LANGID 显式加载对话框与字符串 | 资源缺失时逐级回退 |
| `AutoType.cpp` | 打字引擎（`AT_OPTIONS`：输出模式 / 暂停 / 断点索引） | 改节奏只动这里 |
| `Util.cpp/.hpp` | `LoadStr` / `FormatStr` / `FormatInt` | — |
| `App.hpp` | 全局变量与常量、`APP_SETTINGS` | 新全局先加 `extern` 再在此定义 |
| `resource.h` | 控件 ID、命令 ID、字符串 ID | 见下"资源红线" |
| `lang/*.rc` | **生成物**：4 语言 + `neutral.rc` | 直接改会被下次生成覆盖 |
| `tools/gen_lang.py` | 唯一布局与文案来源 | 改 UI 只能改这里 |
| `tools/verify_lang.cpp` | 资源校验（含 LANG_NEUTRAL） | CI 会跑 |
| `tools/sign.ps1` | 可选 Authenticode 签名 | 读环境变量，不读文件 |

## 构建与校验（提交前必跑）

```bat
build_x64.bat                      :: 产出 build-x64\Typaste.exe（静态 CRT）
tools\verify_lang.bat              :: 期望最后一行 RESULT: OK（含 [neutral]）
```

CMake 等价命令：`cmake -S . -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=Release` + `cmake --build build-x64 --config Release`。
CI（`.github/workflows/build.yml`）跑 x64 + Win32 矩阵，并在每个架构上执行 `verify_lang`。

## 资源红线（最容易踩）

1. `lang/*.rc` **只由** `python tools/gen_lang.py` 生成；要改布局或文案，改生成器再重跑。
2. 生成的 `.rc` 必须是 **UTF-16LE 且无 BOM**；`Typaste_res.rc` 用 `#include` 引入它们，并先引 `neutral.rc`。
3. `resource.h` 里不要复用 `<dlgs.h>` 的 ID（`ico1..16`、`edt1..16`、`lst1..16`、`psh1..16`、`chx1..3`、`cmb1..2`、`scr1..2`、`grp1..4`）——会触发 RC4005 并改变取值。自定义控件用 `edtClip`、`pshCopy`、`sld1`、`btnWhitelist` 这类名字。
4. `.cpp/.hpp` 保持纯 ASCII；界面中文只能出现在生成器里，源码中的中文用 `\uXXXX`。
5. 版本号三处必须同步：`tools/gen_lang.py` 的 `VERSION`/`VERSION_TUPLE`、`HISTORY.txt`、GitHub Release 标题。

## 设置存储

- 首选 `Typaste.exe` 同目录的 `Typaste.ini`（`[Typaste]`，键见 `Config.cpp`）；文件存在时注册表被忽略。
- 目录不可写（Program Files）时自动回退 `HKCU\SOFTWARE\Katayama Hirofumi MZ\Typaste`。
- 新增配置键：`Settings_LoadDefaults` → `Settings_Load` → `Settings_Save` → `README.md` 的 Portable configuration，四处同时改。

## 代码约定

- 字符串一律 `tstring_t` + `LoadStr(IDS_*)`；不要硬编码用户可见文案。
- 对话框进程内常量放 `App.hpp`；`WM_APP_TRAY` / `WM_APP_TYPING_DONE` 已占用 `WM_APP+1/+2`。
- 新对话框必须在 `WM_INITDIALOG` 里调 `Dpi_InitTracking(hwnd)`，并在 `WM_DPICHANGED` 里 `return Dpi_HandleChanged(...)`。
- 输入线程只做打字，不碰 UI；结束时 `PostMessage(WM_APP_TYPING_DONE)`。
- 三种输出模式（`OUTPUT_AUTO/ADAPTIVE_IME/UNICODE`）只在 `AutoType()` 里分支；自适应模式由引擎逐字符切 IME，线程里不要重复套用 `ImeMode`。
- 输入法只走 `Ime_GetState/Apply/Restore`：不要直接 `SendMessage(IMC_SETOPENSTATUS)`，否则会漏掉转换模式的恢复。
- 暂停只看 `g_bPauseTyping`（字符边界生效），中止只看 `g_bAbortTyping`；进度写 `g_nTypedChars`，不要再加第二套标志。
- 主窗口是 modeless 对话框：关闭用 `DestroyWindow`，不要 `EndDialog`。

## 红线

- 不要删除 `MAINWINDOW_PROP`（单实例定位）或 `Local\Typaste.SingleInstance` 互斥体。
- 不要在 `OnHotKey` 里同步打字：必须走线程，否则窗口卡死。
- 不要在未跑 `verify_lang` 的情况下提交资源改动。
- 不要提交 `build*`、`*.exe`、`*.obj`、`tools/verify_lang.exe`（`.gitignore` 已覆盖）。

## 发布

1. `git add -A && git commit`，push 到 `https://github.com/YHuanheg/Typaste.git`。
2. `gh release create vX.Y.Z build-x64/Typaste.exe --title "Typaste X.Y.Z (x64)" --notes-file ...`。
3. 确认 CI 绿（x64 + Win32），再对外说"已发布"。

## 深入文档

| 主题 | 文件 |
| --- | --- |
| 模块划分与设计理由、INI 格式 | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| 历史改进项与遗留建议 | [docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md) |
| 代码签名（本地与 CI） | [docs/SIGNING.md](docs/SIGNING.md) |
| 文档总索引 | [docs/index.md](docs/index.md) |

<!-- neat-freak: initialized at 2026-11-07 -->
