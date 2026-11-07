(UTF-8, 简体中文)

# Typaste

Typing + Paste == Typaste。

Typaste 会在本窗口显示期间拦截 Ctrl+V 粘贴，把剪贴板里的文本逐字符模拟成
键盘输入打出来，从而绕过“禁止粘贴”的限制。

按 Esc 键可停止输入。

许可证：MIT

## 功能

- 中文、日文、emoji 等字符通过 SendInput + KEYEVENTF_UNICODE 发送，不再依赖键盘布局
- 输入在后台线程执行，主窗口不会卡死
- 暂停 / 继续：在任意字符处暂停，之后从该字符继续
- 停止后续写：按 Esc 停止后，点“续写”只输入剩下的部分
- 可调输入延迟、开始前延迟、随机间隔、热键、输入法控制、提示音
- 输入法控制：打字时把输入法切到英文模式（或关闭），结束后恢复原状态
- 自适应输入：按字符切换输入法（ASCII → 英文，中文/日文 → native 模式）
- 原样输出：暂时让输入法退出，全部字符以 Unicode 输入发送
- 设置里提供输入速度滑条
- 界面语言：English / 日本語 / 简体中文 / 繁體中文
- 支持高 DPI（Per-Monitor V2）、关于对话框、一键恢复默认设置
- 剪贴板预览：主窗口显示当前剪贴板内容；选中其中一段后，按热键只输入选中的部分
- 托盘图标与右键菜单，最小化时收进通知区域
- 程序白名单：只在选中的程序里拦截 Ctrl+V
- 便携配置 Typaste.ini（与 exe 同目录；不可写时回退注册表）

## 支持平台

Windows Vista/7/8.1/10/11 x64（主要目标），同时保留 Win32 (x86) 构建。

## 界面语言

在“设置...”里的“界面语言”中选择。选“跟随系统”时使用 Windows 的显示语言，
切换后立即生效，无需重启。若系统语言不属于上述四种，界面回退为英文
（LANG_NEUTRAL 资源）。

## 配置文件

设置保存在 Typaste.exe 同目录的 Typaste.ini（语言、延迟、热键、提示音、白名单）。
删除该文件即回到注册表模式。

## 构建

    cmake -S . -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=Release
    cmake --build build-x64 --config Release

或者直接运行 build_x64.bat（不需要 CMake）。
界面资源由 python tools/gen_lang.py 统一生成。

## 致谢

原作者：片山博文MZ（Katayama Hirofumi MZ）
现维护者：YHuanheg（https://github.com/YHuanheg）

## 警告

请勿将本软件用于非法用途。
