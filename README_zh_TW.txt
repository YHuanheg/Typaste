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
- 暫停 / 繼續：在任意字元處暫停，之後從該字元繼續
- 停止後續寫：按 Esc 停止後，點「續寫」只輸入剩下的部分
- 可調輸入延遲、開始前延遲、隨機間隔、熱鍵、輸入法控制、提示音
- 輸入法控制：打字時把輸入法切到英文模式（或關閉），結束後恢復原狀態
- 設定中提供輸入速度滑桿
- 介面語言：English / 日本語 / 简体中文 / 繁體中文
- 支援高 DPI（Per-Monitor V2）、關於對話方塊、一鍵恢復預設設定
- 剪貼簿預覽：主視窗顯示目前剪貼簿內容；選取其中一段後，按熱鍵只輸入選取的部分
- 托盤圖示與右鍵選單，最小化時收進通知區域
- 程式白名單：只在選取的程式中攔截 Ctrl+V
- 可攜設定 Typaste.ini（與 exe 同目錄；不可寫時回退登錄檔）

## 支援平台

Windows Vista/7/8.1/10/11 x64（主要目標），同時保留 Win32 (x86) 建置。

## 介面語言

在「設定...」的「介面語言」中選擇。選「跟隨系統」時使用 Windows 的顯示語言，
切換後立即生效，不需重新啟動。若系統語言不屬於上述四種，介面回退為英文
（LANG_NEUTRAL 資源）。

## 設定檔

設定保存在 Typaste.exe 同目錄的 Typaste.ini（語言、延遲、熱鍵、提示音、白名單）。
刪除該檔案即回到登錄檔模式。

## 建置

    cmake -S . -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=Release
    cmake --build build-x64 --config Release

或直接執行 build_x64.bat（不需要 CMake）。
介面資源由 python tools/gen_lang.py 統一產生。

## 致謝

原作者：片山博文MZ（Katayama Hirofumi MZ）
現維護者：YHuanheg（https://github.com/YHuanheg）

## 警告

請勿將本軟體用於非法用途。
