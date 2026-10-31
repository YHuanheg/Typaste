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
- 可调输入延迟、开始前延迟、随机间隔、热键、输入法控制、提示音
- 界面语言：English / 日本語 / 简体中文 / 繁體中文
- 支持高 DPI、关于对话框、一键恢复默认设置

## 支持平台

Windows Vista/7/8.1/10/11 x64（主要目标），同时保留 Win32 (x86) 构建。

## 界面语言

在“设置...”里的“界面语言”中选择。选“跟随系统”时使用 Windows 的显示语言，
切换后立即生效，无需重启。

## 构建

    cmake -S . -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=Release
    cmake --build build-x64 --config Release

或者直接运行 build_x64.bat（不需要 CMake）。

## 致谢

原作者：片山博文MZ（Katayama Hirofumi MZ）
现维护者：YHuanheg（https://github.com/YHuanheg）

## 警告

请勿将本软件用于非法用途。
