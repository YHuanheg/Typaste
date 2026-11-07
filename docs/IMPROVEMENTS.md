# Typaste 改进报告（历史：1.0.5 → 1.4.0）

本报告对应三项需求：**（1）中文翻译可选语言项**、**（2）x64 改进**、**（3）可改进点分析**。
第 1、2 项已经落地并做了实测；第 3 项按优先级列出，附文件位置与做法。

> 本文按版本累积，每版在最前面加一节。"已实现"清单里的编号对应第三节目录项；
> 仍未完成的编号散落在第三节，属于后续建议。模块与设计理由见 [ARCHITECTURE.md](ARCHITECTURE.md)。


> **更新（1.2.0）**：下面的 P0-1、P0-2、P1-1～P1-5、P2-1、P2-3、P2-5、P2-6 与 UI 部分已经实现，详见本节；其余保留为后续建议。

## 1.2.0 已实现

| 项 | 说明 |
| --- | --- |
| P0-1 | 无法映射的字符改用 SendInput + KEYEVENTF_UNICODE（含代理对，emoji 可用） |
| P0-2 | WaitModifierRelease 增加 5 秒超时，按键卡住不再死循环 |
| P1-1 | 输入移到工作线程；剪贴板文本上限 100000 字符并提示 |
| P1-2 | 0 延迟时每 16 个字符 Sleep(1)，不再丢字 |
| P1-3 | 设置对话框新增「恢复默认」，真正用上 Settings_Delete |
| P1-4 | WAV 列表排序 |
| P1-5 | 热键注册失败时自动打开设置 |
| P2-1 | 四语言资源由 tools/gen_lang.py 从单一布局生成（配合 tools/verify_lang） |
| P2-3 | manifest 声明 system DPI aware |
| P2-5 | 增加命名 mutex 单实例（窗口属性作为回退） |
| P2-6 | 注册表写入 SettingsVersion |
| UI | 分组式设置对话框、粗体标题、关于对话框（原作者/维护者/许可/仓库）、主窗口热键与状态行 |

以上各项均已完成；后续建议见第三节。

## 1.3.0 已实现

| 项 | 说明 |
| --- | --- |
| P1-6 | 程序白名单：仅对列表内的进程拦截 Ctrl+V，列表外的 Ctrl+V 原样放行 |
| Tray | 托盘图标 + 右键菜单（显示主窗口 / 设置 / 关于 / 退出），最小化收进托盘 |
| P2-3 | Per-Monitor V2：manifest 声明 + WM_DPICHANGED 处理（缩放子控件与字体） |
| CI | GitHub Actions：x64 + Win32 矩阵构建、资源校验、上传产物 |
| 签名 | tools/sign.ps1 + CI 可选签名（CODESIGN_PFX_BASE64 / CODESIGN_PASSWORD） |

## 1.4.0 已实现

| 项 | 说明 |
| --- | --- |
| P2-7 | Typaste.cpp 拆分为 Settings / Config / Clipboard / Dpi / Util / App 模块 |
| P2-4 | 新增 LANG_NEUTRAL 资源回退块（见 docs/ARCHITECTURE.md） |
| P2-8 | 显式 C++17（CMake CXX_STANDARD 17） |
| DPI | WM_DPICHANGED 改为"96 DPI 基准布局 × dpi"重算，消除累积误差 |
| 配置 | Typaste.ini（exe 同目录，优先于注册表） |
| UI | 输入速度滑条；主窗口剪贴板预览 + "复制到剪贴板"；选中部分只输入选中内容 |


---

## 1.7.0 新增

| 项 | 说明 |
| --- | --- |
| 自适应输入 | `OUTPUT_ADAPTIVE_IME`：逐字符切换 IME 转换模式（ASCII → alphanumeric，非 ASCII → native），中文段用 Unicode 兜底 |
| 原样输出 | `OUTPUT_UNICODE`：全部字符走 `KEYEVENTF_UNICODE`，并在打字期间关闭 IME（TSF 输入法会拦截 Unicode 输入） |
| 引擎重构 | `AutoType()` 改为接收 `AT_OPTIONS`（delay / sound / random / start / output mode / IME window） |
| 新设置 | `OutputMode`（自动 / 自适应 / 原样），设置界面新增「字符输出」下拉 |
| 回归测试 | `tools/regress_output.ps1`：自适应模式断言 IME 在 0/1 间切换且文本为 `a中b`；原样模式断言 `abc123` 原样到达 |

## 1.6.0 改进

| 项 | 说明 |
| --- | --- |
| 输入法切换方式 | 新增"切到英文模式"（默认）：保持 IME 打开、只把转换模式设为 `IME_CMODE_ALPHANUMERIC`，不销毁候选上下文 |
| 完整保存/恢复 | 同时保存开关状态与转换模式，打完先恢复模式再恢复开关，中文标点与全半角回来 |
| 可配置 | 设置里"自动切换输入法状态"下方新增下拉：关闭输入法 / 切到英文模式；旧 `ControlIME` 键自动迁移到 `ImeMode` |
| 新模块 | `Ime.hpp/.cpp`（`IME_STATE`、`Ime_GetState/Apply/Restore`） |
| 回归测试 | `tools/regress_ime.ps1`：记事本设为中文模式 → 打字中应为 0x0 → 打完恢复 0x1 |

## 1.5.0 新增

| 功能 | 实现 |
| --- | --- |
| 暂停 / 继续 | `g_bPauseTyping` 在字符边界生效；按钮与托盘菜单都可切换，状态行显示 `已暂停 X/Y` |
| 停止后续写 | `AutoType()` 返回断点索引，主窗口保存续写上下文；「续写」从断点继续，不重打已输入内容 |
| 进度显示 | 200 ms 定时器刷新 `正在输入 / 已暂停 / 已停止 X/Y` |
| 回归测试 | `tools/regress_typing.ps1`：暂停冻结、继续完成、Esc 停止、续写完成四条断言 |

## 1.4.1 修复

| 问题 | 根因 | 修复 |
| --- | --- | --- |
| 设置对话框点“确定/取消”后程序直接退出 | 主窗口被 `DestroyWindow` 之后，WinMain 循环无条件 `break`（重建标志从未被设置/读取） | 在设置命令里置 `s_bRebuildMain`，循环按该标志重建窗口；新增 tools/regress_settings.ps1 回归测试（OK / Cancel / 切语言三条路径） |

---

## 一、已完成：中文翻译 + 可选语言项

### 为什么不能只加一个 lang/zh_CN.rc

原来的本地化依赖 LoadString / CreateDialog，而这两个 API **没有语言参数**：
Win32 资源加载器只按线程的 UI 语言挑选资源块，用户无法在程序内选择。
所以只加 .rc 文件，中文只会在"Windows 是中文"时才出现，做不到"可选"。

### 实现方式

新增语言模块：

| 文件 | 作用 |
| --- | --- |
| Lang.hpp | 语言枚举（auto / en-US / ja-JP / zh-CN / zh-TW）、Lang_LoadString、Lang_CreateDialog、Lang_DialogBox |
| Lang.cpp | 用 FindResourceExW(..., langid) 精确定位资源；手工解析 STRINGTABLE；CreateDialogIndirectParamW / DialogBoxIndirectParamW 直接吃模板 |

要点：

1. **精确语言查找** FindLocalizedResource()：选中语言 → LANG_NEUTRAL → en-US → 任意 → FindResource，逐级回退。
2. **字符串表自己解析**：STRINGTABLE 每块 16 项，格式为 WORD cch; WCHAR sz[cch];，并用 SizeofResource 做边界检查。
3. **对话框模板直接加载**：LockResource 拿 DLGTEMPLATE，走 *IndirectParam，绕过语言自动选择。
4. **设置项持久化**：注册表 HKCU\SOFTWARE\Katayama Hirofumi MZ\Typaste 的 Language（REG_SZ，取值 auto / en-US / ja-JP / zh-CN / zh-TW）。
5. **切换立即生效**：设置对话框确定后 Lang_SetSetting() 更新当前语言，WinMain 循环重建主窗口（原有循环），无需重启。
6. **单实例识别不再依赖标题**：原来用 FindWindow("#32770", 本地化标题)，翻译后必然失配；改为 SetProp(hwnd, "Typaste.MainWindow") + EnumWindows。

### 新增/修改的资源

- lang/zh_CN.rc（简体，LANG_CHINESE / SUBLANG_CHINESE_SIMPLIFIED = 0x0804）
- lang/zh_TW.rc（繁体，SUBLANG_CHINESE_TRADITIONAL = 0x0404）
- lang/en_US.rc、lang/ja_JP.rc：设置对话框新增"语言"行（cmb2 下拉框），对话框高度 140 → 154
- resource.h：新增 IDS_AUTOLANG（"跟随系统"）；版本号升到 1.1.0
- README_zh.txt、README_zh_TW.txt（"打开说明"按钮按语言打开对应文件）

> 注：ico1 / edt1 / cmb1 / psh1 等控件 ID 来自 <dlgs.h>，不要在 resource.h 里重复定义（会触发 C4005 并改变取值）。

### 实测结果

| 检查项 | 结果 |
| --- | --- |
| 四种语言的 IDD_MAIN / IDD_SETTINGS / STRINGTABLE 都编进 exe | PASS（按 LANGID 分离） |
| 语言下拉 5 项：跟随系统 / English / 日本語 / 简体中文 / 繁體中文 | PASS（CB_GETCOUNT=5，当前项正确） |
| 运行中切到 English，主窗口立即变英文，注册表写入 en-US | PASS |
| 中文界面渲染（字体、换行、按钮加速键） | PASS（截图确认） |

---

## 二、已完成：x64 改进

1. **CMake 默认 x64**：CMAKE_GENERATOR_PLATFORM 必须在 project() **之前**设置，且不能用 if(MSVC) 判断（此时编译器还没检测）。改为判断生成器名；显式 -A Win32 仍可覆写成 32 位。
2. **静态 CRT 现代化**：CMP0091 NEW + CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"，不再靠字符串替换 /MD→/MT。
3. **编译选项**：/utf-8、/W4、/permissive-、/Zc:__cplusplus。
4. **链接加固**：/DYNAMICBASE /NXCOMPAT，x64 额外 /HIGHENTROPYVA；保留 /MANIFEST:NO。
5. **targetver.h**：原来硬编码 Win2000 的 WINVER 且不可覆盖。改为 #ifndef 包裹、默认 Vista（0x0600），命令行可覆盖。
6. **新增 build_x64.bat**：用 vswhere 自动定位 MSVC，一键产出 build-x64\Typaste.exe，不依赖 CMake。
7. **64 位正确性审查**：SendInput 的 ULONG_PTR dwExtraInfo、SendMessageTimeout 的 DWORD_PTR、LPARAM/WPARAM 强转都正确；发现并修掉 MAKEINTRESOURCEW(RT_STRING) 的重复包装（RT_* 本身就是 MAKEINTRESOURCE，会触发 C4302 截断）以及 size_t→DWORD 的 C4267。

实测：CMake 与 build_x64.bat 两条链都产出 machine=0x8664（x64）的 exe，四语言资源完整。

---

## 三、可改进点分析（按优先级）

### P0 —— 影响核心功能

**P0-1 无法输入的字符走的是不可靠路径**（AutoType.cpp:201-211）

VkKeyScanEx 失败时（中文、假名、emoji、非当前键盘布局的符号），代码用 SendMessageTimeout(info.hwndFocus, WM_CHAR, ...) 兜底。问题：

- info.hwndFocus 可能为 NULL，此时会广播到所有顶层窗口；
- UIPI：目标进程完整性更高（管理员程序）时消息被静默丢弃；
- 目标不处理 WM_CHAR（DirectInput 游戏、部分编辑器）则完全无效；
- 代理对（emoji）逐 UTF-16 单元发送，部分程序只接受完整代理对。

建议改用 SendInput + KEYEVENTF_UNICODE，不依赖键盘布局：

    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wScan = (WORD)ch;
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));

**P0-2 WaitModifierRelease 是无超时死循环**（AutoType.cpp:40-48）

按住 Ctrl 不放（或键盘卡键）时 while (GetAsyncKeyState(...) < 0) 永不退出，程序像卡死。建议加 3~5 秒上限（GetTickCount64 计时），超时后强制继续。

### P1 —— 稳定性与体验

1. **剪贴板文本没有大小上限**（Typaste.cpp 的 OnHotKey）：输入在 UI 线程同步跑，几 MB 文本会让窗口长时间无响应。建议限制 256 KB 并考虑把循环挪到工作线程（SendInput 不要求同线程）。
2. **延迟 0 会丢字符**：MySleep(0) 直接返回，连续 SendInput 会被目标消息队列合并。建议下限 1~2 ms。
3. **死代码**：Settings_Delete() 与 IDS_DELETESETTINGS 已实现但无 UI 入口（ID_DELETE_SETTINGS 未接）。建议在设置里加"恢复默认设置"。
4. **声音文件枚举无排序**：FindFirstFile("*.wav") 顺序由文件系统决定。建议收集后 std::sort。
5. **热键注册失败后程序继续**：只弹 MessageBox，用户之后按 Ctrl+V 毫无反应。建议失败时自动打开设置。
6. **全局 Ctrl+V 过于激进**：任何程序里按 Ctrl+V 都被拦截。建议按进程名白名单，或默认改用 Ctrl+Shift+V。

### P2 —— 工程质量

1. **资源维护成本**：4 份 rc 里 90% 重复。建议抽 .rc2 公共布局块 + 每语言只写字符串，或脚本从 CSV 生成；配 CI 校验（本仓库临时用的 verify_lang.exe 思路：按 LANGID 检查每个 IDD_* 与关键 IDS_* 是否存在）。
2. **没有 CI**：建议 GitHub Actions 跑 x64 + Win32 矩阵 + 资源校验 + dumpbin /headers 确认架构。
3. **DPI 感知缺失**（res/Manifest_1.manifest）：没声明 dpiAware，高 DPI 屏上整个界面被位图拉伸发虚。最小改法：

       <application xmlns="urn:schemas-microsoft-com:asm.v3">
         <windowsSettings>
           <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
         </windowsSettings>
       </application>

   Per-Monitor V2 还需处理 WM_DPICHANGED（对话框要重新 MoveWindow）。
4. **没有 LANG_NEUTRAL 资源块**：任何直接 LoadString 的回退都会落到"任意语言"。建议英文再以 LANG_NEUTRAL 复制一份，或所有用户可见字符串统一走 Lang_LoadString。
5. **单实例方案**：窗口属性只在同一桌面会话有效。建议再加 CreateMutexW(NULL, FALSE, L"Local\Typaste")。
6. **注册表没有设置版本号**：将来改默认值/迁移会踩坑，建议加 SettingsVersion（DWORD）。
7. **代码组织**：Typaste.cpp 已 600+ 行且混合设置、对话框、热键、剪贴板、IME 控制。建议拆 Settings.* / MainWindow.*，把 g_* 全局收进 App 结构体。
8. **C++ 标准**：CMake 未指定 CXX_STANDARD，默认走 MSVC 的 C++14。建议 /std:c++17，并把语言匹配表改 constexpr。
9. **签名与分发**：（已完成）CI 用 tools/sign.ps1 签名，配置 CODESIGN_PFX_BASE64 / CODESIGN_PASSWORD 即可；do_sign_all.bat 已删除。

### P3 —— 可选功能

- 托盘图标 + 右键菜单（当前关窗即退出，没有常驻模式）；
- 开机自启（Run 注册表项）；
- 允许把热键设为无修饰键或组合序列；
- 输入前把剪贴板内容写入撤销栈，出错可恢复；
- 可开关的调试日志（目前只有 OutputDebugString）。

---

## 附：本次改动清单

- 新增 Lang.hpp / Lang.cpp：语言子系统（显式资源查找 + 语言选择）
- 新增 lang/zh_CN.rc / lang/zh_TW.rc：简体 / 繁体中文资源
- 新增 README_zh.txt / README_zh_TW.txt
- 新增 build_x64.bat：一键 x64 构建（MSVC，无需 CMake）
- 修改 Typaste.cpp：语言接入、单实例改用窗口属性、热键注销、C4267 修复
- 修改 Typaste.hpp：引入 Lang.hpp / <cstdlib>
- 修改 resource.h：IDS_AUTOLANG、版本 1.1.0
- 修改 lang/en_US.rc, lang/ja_JP.rc：语言下拉控件
- 修改 Typaste_res.rc：include 中文资源
- 修改 CMakeLists.txt：默认 x64、静态 CRT、/utf-8 /W4、加固链接
- 修改 targetver.h：默认 Vista，可用 -D 覆盖
- 修改 README.md/.txt/_ja.txt, HISTORY.txt, .gitignore
