# Typaste 架构说明

## 模块划分（1.4.0）

| 文件 | 职责 |
| --- | --- |
| Typaste.cpp | WinMain、主窗口、托盘图标、全局热键、输入线程、剪贴板预览同步 |
| Settings.cpp | 设置数据、白名单判断、设置对话框、白名单对话框 |
| Config.cpp | 存储后端：Typaste.ini（首选）与注册表（回退） |
| Clipboard.cpp | 系统剪贴板读写、编辑框"选中或全部"取值 |
| Dpi.cpp | Per-Monitor V2：基准布局记录、按 DPI 重算、标题字体 |
| Lang.cpp / Lang.hpp | 按 LANGID 显式加载对话框与字符串资源 |
| AutoType.cpp | 打字引擎（SendInput + KEYEVENTF_UNICODE） |
| Util.cpp | LoadStr / FormatStr / FormatInt |
| App.hpp | 全局变量与常量声明、APP_SETTINGS 结构 |

## 为什么把 Typaste.cpp 拆开

1. **职责混杂**：1.3.0 的 Typaste.cpp 已经有 1500 行，同时包含设置读写、白名单、
   托盘、对话框、输入线程。每次改设置都要在同一个大文件里滚动，改错的风险高。
2. **新增功能需要落点**：这一版加了配置文件、速度滑条、剪贴板预览与选中输入。
   如果继续堆在同一个文件里，会突破 2000 行，review 和 diff 都难以阅读。
3. **改变更小的爆炸半径**：拆开之后，"改设置格式"只动 Config.cpp，"改输入节奏"
   只动 AutoType.cpp，互不影响；编译也是按文件增量。
4. **依赖变清晰**：App.hpp 只放共享状态，Settings 不依赖主窗口，Dpi 不依赖设置，
   循环依赖被结构性地挡住。

## 为什么要有 LANG_NEUTRAL 资源块

Win32 的 \`LoadString\` / \`CreateDialog\` 没有语言参数，只用"精确 LANGID → 语言中性 →
任意一个"的顺序挑资源。本项目只有在 \`en-US / ja-JP / zh-CN / zh-TW\` 四份资源，
系统 UI 语言是德语时既没有精确匹配、也没有中性块，资源加载器会挑"目录中任意一份"
（实践中经常是日文）。加上 \`LANG_NEUTRAL\` 的英文副本后，回退是确定的英文，
而不是随机语言。

Typaste 自己的 \`Lang_LoadString\` 已经做了显式回退，但任何将来走系统 API 的代码
（包括第三方对话框、版本信息查看器）仍然依赖这份中性资源。

## 为什么用 C++17

- \`std::wstring\` 的 \`data()\` 在 C++17 起才保证可写，用于 \`GetWindowText\` 之类的
  缓冲区写法不再依赖实现细节；
- 结构化绑定、\`if constexpr\`、\`std::string_view\` 让配置解析与语言匹配更短；
- MSVC 默认是 C++14，不显式声明标准时，不同 VS 版本对同一段代码的解释可能不同；
- 与 CMake 的 \`CXX_STANDARD 17\` 一起，把工具链要求写死在工程里。

## 为什么跨屏 DPI 需要"手工微调"

Per-Monitor V2 只负责发 \`WM_DPICHANGED\`，不会替你重排控件。1.3.0 的实现是
"按上一次的比例再缩放一次"，问题：

- 每次 \`MulDiv\` 都有取整，来回拖动显示器会累积误差（控件慢慢偏移）；
- 字体行高不是线性的，纯比例会让文字与控件高度对不齐；
- 下拉框、列表框的宽度不跟随父窗口缩放。

1.4.0 改为：初始化时把每个子控件的矩形与字体**归一化到 96 DPI**存下来，
之后每次 DPI 变化都按 \`基准 × dpi / 96\` 重算，误差不累积，且控件类型无关。

## 便携配置（Typaste.ini）

首次运行时，如果 exe 目录可写，会在同目录生成 \`Typaste.ini\`：

\`\`\`ini
[Typaste]
SettingsVersion=4
Delay=20
DelayToStart=0
HotKey=598
ControlIME=1
Random=0
OnlyWhitelist=0
Sound=
Whitelist=notepad.exe;chrome.exe
Language=auto
\`\`\`

存在该文件时，设置只从 INI 读写，注册表被忽略；删除文件即回到注册表模式。
放在 Program Files 之类只读目录时，自动继续使用注册表。
