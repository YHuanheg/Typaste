(UTF-8, 繁體中文)

# Typaste

Typing + Paste == Typaste。

Typaste 會在本視窗顯示期間攔截 Ctrl+V 貼上，把剪貼簿的文字逐字模擬成
鍵盤輸入打出，藉此繞過「禁止貼上」的限制。

按 Esc 鍵可停止輸入。

授權：MIT

## 功能

- 中文、日文、emoji 等字元透過 SendInput + KEYEVENTF_UNICODE 送出，不再依賴鍵盤配置
- 輸入在背景執行緒執行，主視窗不會凍結
- 可調輸入延遲、開始前延遲、隨機間隔、熱鍵、輸入法控制、提示音
- 介面語言：English / 日本語 / 简体中文 / 繁體中文
- 支援高 DPI、關於對話方塊、一鍵恢復預設設定

## 支援平台

Windows Vista/7/8.1/10/11 x64（主要目標），同時保留 Win32 (x86) 建置。

## 介面語言

在「設定...」的「介面語言」中選擇。選「跟隨系統」時使用 Windows 的顯示語言，
切換後立即生效，不需重新啟動。

## 建置

    cmake -S . -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=Release
    cmake --build build-x64 --config Release

或直接執行 build_x64.bat（不需要 CMake）。

## 致謝

原作者：片山博文MZ（Katayama Hirofumi MZ）
現維護者：YHuanheg（https://github.com/YHuanheg）

## 警告

請勿將本軟體用於非法用途。
