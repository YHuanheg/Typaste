#!/usr/bin/env python3
# gen_lang.py --- generate the localized resource files (lang/*.rc)
#
#   python tools/gen_lang.py
#
# Every dialog layout lives here once; only the text differs per language.
# A LANG_NEUTRAL copy of the English resources is generated as well, so that
# the Win32 loader has a deterministic fallback when the UI language of the
# system is not one of the supported ones.
import os
import re
import sys

VERSION = "1.6.0"
VERSION_TUPLE = "1, 6, 0, 0"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TEMPLATE = r'''// Typaste localized resources for @@tag@@
// GENERATED FILE --- edit tools/gen_lang.py instead of this file.

#pragma code_page(65001) // UTF-8

LANGUAGE @@rc_language@@

//////////////////////////////////////////////////////////////////////////////
// RT_DIALOG

IDD_MAIN DIALOG 0, 0, 320, 282
CAPTION "@@main_title@@"
STYLE DS_CENTER | DS_MODALFRAME | WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX
FONT 9, "@@font@@"
{
    ICON 0, ico1, 12, 12, 0, 0
    LTEXT "@@main_title@@", txtTitle, 58, 12, 250, 12
    LTEXT "@@subtitle@@", -1, 58, 26, 250, 10
    LTEXT "@@select_hint@@", -1, 12, 44, 296, 10
    EDITTEXT edtClip, 12, 58, 296, 128, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP
    LTEXT "", txtHotkey, 12, 192, 180, 10
    LTEXT "", txtStatus, 12, 204, 180, 10
    PUSHBUTTON "@@btn_copy@@", pshCopy, 200, 190, 108, 16
    PUSHBUTTON "@@btn_pause@@", pshPause, 200, 208, 52, 16
    PUSHBUTTON "@@btn_resume@@", pshResume, 256, 208, 52, 16
    PUSHBUTTON "@@btn_exit@@", IDCANCEL, 200, 226, 108, 18
    PUSHBUTTON "@@btn_readme@@", psh2, 12, 250, 92, 22
    PUSHBUTTON "@@btn_settings@@", psh1, 114, 250, 92, 22
    PUSHBUTTON "@@btn_about@@", btnAbout, 216, 250, 92, 22
}

IDD_SETTINGS DIALOG 0, 0, 260, 328
CAPTION "@@settings_caption@@"
STYLE DS_CENTER | DS_MODALFRAME | WS_POPUPWINDOW | WS_CAPTION
FONT 9, "@@font@@"
{
    GROUPBOX "@@grp_input@@", grpInput, 7, 7, 246, 96
    RTEXT "@@delay_type@@", -1, 14, 24, 74, 10
    EDITTEXT edt1, 92, 22, 36, 14, ES_NUMBER | ES_RIGHT
    CONTROL "", scr1, "msctls_updown32", UDS_NOTHOUSANDS | UDS_ARROWKEYS | UDS_AUTOBUDDY | UDS_ALIGNRIGHT | UDS_SETBUDDYINT, 128, 22, 12, 14
    LTEXT "@@ms@@", -1, 142, 24, 30, 10
    AUTOCHECKBOX "@@random@@", chx2, 172, 22, 76, 14
    LTEXT "@@lbl_speed@@", -1, 14, 46, 74, 10
    CONTROL "", sld1, "msctls_trackbar32", TBS_AUTOTICKS | TBS_HORZ, 92, 42, 130, 20
    RTEXT "@@delay_start@@", -1, 14, 72, 74, 10
    EDITTEXT edt3, 92, 70, 36, 14, ES_NUMBER | ES_RIGHT
    CONTROL "", scr2, "msctls_updown32", UDS_NOTHOUSANDS | UDS_ARROWKEYS | UDS_AUTOBUDDY | UDS_ALIGNRIGHT | UDS_SETBUDDYINT, 128, 70, 12, 14
    LTEXT "@@ms@@", -1, 142, 72, 30, 10

    GROUPBOX "@@grp_keys@@", grpKeys, 7, 110, 246, 76
    RTEXT "@@hotkey@@", -1, 14, 126, 74, 10
    CONTROL "", edt2, "msctls_hotkey32", 0, 92, 123, 120, 14
    LTEXT "@@sound@@", -1, 14, 148, 74, 10
    COMBOBOX cmb1, 92, 146, 100, 200, CBS_NOINTEGRALHEIGHT | CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP
    PUSHBUTTON "@@browse@@", psh2, 196, 146, 50, 14
    PUSHBUTTON "@@play@@", psh3, 196, 164, 50, 14

    GROUPBOX "@@grp_other@@", grpOther, 7, 193, 246, 96
    AUTOCHECKBOX "@@ime@@", chx1, 14, 208, 140, 14
    COMBOBOX cmbIme, 14, 224, 150, 90, CBS_DROPDOWNLIST | CBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP
    LTEXT "@@lbl_language@@", -1, 14, 246, 62, 10
    COMBOBOX cmb2, 74, 244, 172, 120, CBS_DROPDOWNLIST | CBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP
    PUSHBUTTON "@@btn_whitelist@@", btnWhitelist, 14, 262, 130, 16

    PUSHBUTTON "@@btn_reset@@", btnReset, 7, 300, 74, 16
    DEFPUSHBUTTON "@@btn_ok@@", IDOK, 104, 300, 68, 16
    PUSHBUTTON "@@btn_cancel@@", IDCANCEL, 180, 300, 68, 16
}

IDD_WHITELIST DIALOG 0, 0, 260, 196
CAPTION "@@whitelist_caption@@"
STYLE DS_CENTER | DS_MODALFRAME | WS_POPUPWINDOW | WS_CAPTION
FONT 9, "@@font@@"
{
    AUTOCHECKBOX "@@whitelist_enable@@", chkWhitelist, 7, 7, 246, 14
    LISTBOX lstPrograms, 7, 26, 246, 112, LBS_NOINTEGRALHEIGHT | LBS_SORT | WS_VSCROLL | WS_TABSTOP
    EDITTEXT edtName, 7, 144, 120, 14, ES_AUTOHSCROLL
    PUSHBUTTON "@@whitelist_add@@", btnAdd, 132, 144, 54, 14
    PUSHBUTTON "@@whitelist_browse@@", btnBrowseExe, 190, 144, 63, 14
    PUSHBUTTON "@@whitelist_remove@@", btnRemove, 7, 166, 60, 16
    DEFPUSHBUTTON "@@btn_ok@@", IDOK, 120, 166, 60, 16
    PUSHBUTTON "@@btn_cancel@@", IDCANCEL, 186, 166, 67, 16
}

IDD_ABOUT DIALOG 0, 0, 260, 124
CAPTION "@@about_caption@@"
STYLE DS_CENTER | DS_MODALFRAME | WS_POPUPWINDOW | WS_CAPTION
FONT 9, "@@font@@"
{
    ICON 0, ico1, 12, 12, 0, 0
    LTEXT "@@main_title@@", txtTitle, 58, 12, 190, 11
    LTEXT "@@about_text@@", -1, 58, 26, 190, 62
    DEFPUSHBUTTON "@@btn_ok@@", IDOK, 100, 96, 60, 16
}

//////////////////////////////////////////////////////////////////////////////
// RT_VERSION

1 VERSIONINFO
FILEVERSION     @@ver_tuple@@
PRODUCTVERSION  @@ver_tuple@@
FILEOS          0x40004
FILETYPE        0x1
FILESUBTYPE     0x0
{
    BLOCK "StringFileInfo"
    {
        BLOCK "@@lang_hex@@@@cp_hex@@"
        {
            VALUE "CompanyName", "YHuanheg\0"
            VALUE "FileDescription", "Typaste\0"
            VALUE "FileVersion", "@@version@@\0"
            VALUE "LegalCopyright", "Copyright (C) 2019-2026 Katayama Hirofumi MZ, (C) 2026 YHuanheg\0"
            VALUE "OriginalFilename", "Typaste.exe\0"
            VALUE "ProductName", "Typaste\0"
            VALUE "ProductVersion", "@@version@@\0"
        }
    }
    BLOCK "VarFileInfo"
    {
        VALUE "Translation", @@langid_hex@@, 0x@@cp_hex@@
    }
}

//////////////////////////////////////////////////////////////////////////////
// RT_STRING

STRINGTABLE
{
    IDS_README, "@@s_readme@@"
    IDS_DELETESETTINGS, "@@s_reset@@"
    IDS_TITLETEXT, "@@main_title@@"
    IDS_NONE, "@@s_none@@"
    IDS_CHOOSESOUNDFILE, "@@s_choosesound@@"
    IDS_SOUNDFILTER, "@@s_soundfilter@@"
    IDS_FAILDIALOG, "@@s_faildialog@@"
    IDS_FAILHOTKEY, "@@s_failhotkey@@"
    IDS_AUTOLANG, "@@s_autolang@@"
    IDS_HOTKEY_FMT, "@@s_hotkeyfmt@@"
    IDS_STATUS_TYPING, "@@s_typing@@"
    IDS_STATUS_READY, "@@s_ready@@"
    IDS_TEXT_TOO_LONG, "@@s_toolong@@"
    IDS_BUSY, "@@s_busy@@"
    IDS_RESET_CONFIRM, "@@s_resetconfirm@@"
    IDS_WHITELIST_FILTER, "@@s_whitelistfilter@@"
    IDS_TRAY_SHOW, "@@s_trayshow@@"
    IDS_TRAY_SETTINGS, "@@s_traysettings@@"
    IDS_TRAY_ABOUT, "@@s_trayabout@@"
    IDS_TRAY_EXIT, "@@s_trayexit@@"
    IDS_TRAY_TIP, "@@main_title@@"
    IDS_COPIED, "@@s_copied@@"
    IDS_CONFIG_PATH, "@@s_configpath@@"
    IDS_CLIP_EMPTY, "@@s_clipempty@@"
    IDS_IME_CLOSE, "@@s_ime_close@@"
    IDS_IME_ALPHANUM, "@@s_ime_alphanum@@"
    IDS_PAUSE, "@@btn_pause@@"
    IDS_CONTINUE, "@@btn_continue@@"
    IDS_RESUME, "@@btn_resume@@"
    IDS_STATUS_TYPING_FMT, "@@s_typingfmt@@"
    IDS_STATUS_PAUSED_FMT, "@@s_pausedfmt@@"
    IDS_STATUS_STOPPED_FMT, "@@s_stoppedfmt@@"
}

//////////////////////////////////////////////////////////////////////////////
'''

LANGS = [
    {
        "file": "lang/en_US.rc",
        "tag": "en-US",
        "language": "LANG_ENGLISH, SUBLANG_ENGLISH_US",
        "langid_hex": "0x0409", "lang_hex": "0409", "cp_hex": "04B0",
        "font": "Segoe UI",
        "text": dict(
            main_title="Typaste " + VERSION,
            subtitle="Typing + Paste == Typaste",
            desc="Typaste intercepts the pasting process by Ctrl+V and emulates text input like typing.",
            select_hint="Select a part of the text below; the hot-key types only the selection.",
            btn_readme="&Open ReadMe", btn_settings="&Settings...", btn_about="&About",
            btn_exit="E&xit", btn_copy="&Copy to clipboard",
            settings_caption="Typaste Settings",
            grp_input="Typing", delay_type="&Delay to type:", delay_start="Delay to s&tart:",
            ms="ms", random="&Random", lbl_speed="&Speed:",
            grp_keys="Hot-key and sound", hotkey="&HotKey:", sound="&Sound:",
            browse="&Browse...", play="&Play",
            grp_other="Other", ime="Control &IME On/Off", lbl_language="&Language:",
            btn_whitelist="&Program whitelist...",
            btn_reset="Reset", btn_ok="OK", btn_cancel="Cancel",
            whitelist_caption="Program whitelist",
            whitelist_enable="Only intercept Ctrl+V in these programs (&W)",
            whitelist_add="Add", whitelist_remove="Remove", whitelist_browse="B&rowse...",
            about_caption="About Typaste",
            about_text="Original author: Katayama Hirofumi MZ\\n"
                       "Maintainer: YHuanheg\\nLicense: MIT License\\n\\n"
                       "https://github.com/YHuanheg/Typaste",
            s_readme="\\README.txt",
            s_reset="Do you want to reset the Typaste settings?",
            s_none="(None)",
            s_choosesound="Choose an audio file",
            s_soundfilter="WAV Audio File (*.wav)|*.wav|All Files (*.*)|*.*|",
            s_faildialog="Failed to create the dialog.",
            s_failhotkey="Failed to register the hot-key.",
            s_autolang="Follow the system",
            s_hotkeyfmt="Hot-key: %s",
            s_typing="Typing... press Esc to stop",
            s_ready="Ready",
            s_toolong="The clipboard text is too long to type (limit: %d characters).",
            s_busy="Typaste is already typing.",
            s_resetconfirm="Reset all settings to the defaults?",
            s_whitelistfilter="Programs (*.exe)|*.exe|All Files (*.*)|*.*|",
            s_trayshow="Show window",
            s_traysettings="Settings...",
            s_trayabout="About",
            s_trayexit="Exit",
            s_copied="Copied to the clipboard.",
            s_configpath="Settings file: %s",
            s_clipempty="(The clipboard is empty)",
            s_ime_close="Close the IME", s_ime_alphanum="Switch to English mode",
            btn_pause="&Pause", btn_continue="&Resume", btn_resume="Cont&inue",
            s_typingfmt="Typing... %d/%d (Esc stops)",
            s_pausedfmt="Paused %d/%d - press Resume",
            s_stoppedfmt="Stopped %d/%d - press Continue",
        ),
    },
    {
        "file": "lang/ja_JP.rc",
        "tag": "ja-JP",
        "language": "LANG_JAPANESE, SUBLANG_DEFAULT",
        "langid_hex": "0x0411", "lang_hex": "0411", "cp_hex": "04B0",
        "font": "Meiryo UI",
        "text": dict(
            main_title="Typaste " + VERSION,
            subtitle="Typing + Paste == Typaste",
            desc="Typaste は Ctrl+V の貼り付けに割り込み、打ち込むように入力をエミュレートします。",
            select_hint="下のテキストを選択しておくと、ホットキーは選択部分だけを入力します。",
            btn_readme="ReadMe を開く(&O)", btn_settings="設定変更(&S)...",
            btn_about="バージョン情報(&A)", btn_exit="終了(&X)",
            btn_copy="クリップボードへコピー(&C)",
            settings_caption="Typaste 設定",
            grp_input="タイピング", delay_type="タイプ遅延(&D):",
            delay_start="開始遅延(&T):", ms="ミリ秒", random="ランダム(&R)",
            lbl_speed="速度(&S):",
            grp_keys="ホットキーと音声", hotkey="ホットキー(&H):", sound="音声(&S):",
            browse="参照(&B)...", play="再生(&P)",
            grp_other="その他", ime="&IME のオン／オフを制御", lbl_language="言語(&L):",
            btn_whitelist="プログラムの許可リスト(&P)...",
            btn_reset="初期化", btn_ok="OK", btn_cancel="キャンセル",
            whitelist_caption="プログラムの許可リスト",
            whitelist_enable="これらのプログラムでのみ Ctrl+V を横取りする(&W)",
            whitelist_add="追加", whitelist_remove="削除", whitelist_browse="参照(&R)...",
            about_caption="Typaste について",
            about_text="原作者: 片山博文MZ (Katayama Hirofumi MZ)\\n"
                       "メンテナー: YHuanheg\\nライセンス: MIT License\\n\\n"
                       "https://github.com/YHuanheg/Typaste",
            s_readme="\\README_ja.txt",
            s_reset="設定を初期化しますか？",
            s_none="(なし)",
            s_choosesound="音声ファイルを選んで下さい",
            s_soundfilter="WAV音声ファイル (*.wav)|*.wav|すべてのファイル (*.*)|*.*|",
            s_faildialog="ダイアログの作成に失敗しました。",
            s_failhotkey="ホットキーの登録に失敗しました。",
            s_autolang="システムに合わせる",
            s_hotkeyfmt="ホットキー: %s",
            s_typing="入力中... Esc キーで停止",
            s_ready="待機中",
            s_toolong="クリップボードのテキストが長すぎます (上限 %d 文字)。",
            s_busy="すでに入力中です。",
            s_resetconfirm="すべての設定を初期化しますか？",
            s_whitelistfilter="プログラム (*.exe)|*.exe|すべてのファイル (*.*)|*.*|",
            s_trayshow="ウィンドウを表示",
            s_traysettings="設定変更...",
            s_trayabout="バージョン情報",
            s_trayexit="終了",
            s_copied="クリップボードにコピーしました。",
            s_configpath="設定ファイル: %s",
            s_clipempty="(クリップボードは空です)",
            s_ime_close="IME を閉じる", s_ime_alphanum="英数モードに切り替える",
            btn_pause="一時停止(&P)", btn_continue="再開(&R)", btn_resume="続きから(&I)",
            s_typingfmt="入力中... %d/%d (Esc で停止)",
            s_pausedfmt="一時停止中 %d/%d — 「再開」で続行",
            s_stoppedfmt="停止 %d/%d — 「続きから」で再開",
        ),
    },
    {
        "file": "lang/zh_CN.rc",
        "tag": "zh-CN",
        "language": "LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED",
        "langid_hex": "0x0804", "lang_hex": "0804", "cp_hex": "04B0",
        "font": "Microsoft YaHei UI",
        "text": dict(
            main_title="Typaste " + VERSION,
            subtitle="Typing + Paste == Typaste",
            desc="Typaste 会拦截 Ctrl+V 粘贴，并像打字一样模拟输入。",
            select_hint="在下面的文本里选中一段，按热键时只输入选中的内容。",
            btn_readme="打开说明(&O)", btn_settings="设置(&S)...", btn_about="关于(&A)",
            btn_exit="退出(&X)", btn_copy="复制到剪贴板(&C)",
            settings_caption="Typaste 设置",
            grp_input="输入", delay_type="输入延迟(&D):", delay_start="开始前延迟(&T):",
            ms="毫秒", random="随机间隔(&R)", lbl_speed="速度(&S):",
            grp_keys="热键与提示音", hotkey="热键(&H):", sound="提示音(&S):",
            browse="浏览(&B)...", play="试听(&P)",
            grp_other="其他", ime="自动切换输入法状态(&I)", lbl_language="界面语言(&L):",
            btn_whitelist="程序白名单(&P)...",
            btn_reset="恢复默认", btn_ok="确定", btn_cancel="取消",
            whitelist_caption="程序白名单",
            whitelist_enable="仅在这些程序中拦截 Ctrl+V(&W)",
            whitelist_add="添加", whitelist_remove="移除", whitelist_browse="浏览(&R)...",
            about_caption="关于 Typaste",
            about_text="原作者：片山博文MZ（Katayama Hirofumi MZ）\\n"
                       "现维护者：YHuanheg\\n许可协议：MIT License\\n\\n"
                       "https://github.com/YHuanheg/Typaste",
            s_readme="\\README_zh.txt",
            s_reset="是否将 Typaste 的设置恢复为默认值？",
            s_none="(无)",
            s_choosesound="选择提示音文件",
            s_soundfilter="WAV 音频文件 (*.wav)|*.wav|所有文件 (*.*)|*.*|",
            s_faildialog="无法创建对话框。",
            s_failhotkey="注册热键失败。",
            s_autolang="跟随系统",
            s_hotkeyfmt="热键：%s",
            s_typing="正在输入… 按 Esc 停止",
            s_ready="就绪",
            s_toolong="剪贴板文本过长，无法输入（上限 %d 字符）。",
            s_busy="正在输入中，请稍候。",
            s_resetconfirm="确定要把所有设置恢复为默认值吗？",
            s_whitelistfilter="程序 (*.exe)|*.exe|所有文件 (*.*)|*.*|",
            s_trayshow="显示主窗口",
            s_traysettings="设置...",
            s_trayabout="关于",
            s_trayexit="退出",
            s_copied="已复制到剪贴板。",
            s_configpath="设置文件：%s",
            s_clipempty="（剪贴板为空）",
            s_ime_close="关闭输入法", s_ime_alphanum="切到英文模式",
            btn_pause="暂停(&P)", btn_continue="继续(&C)", btn_resume="续写(&I)",
            s_typingfmt="正在输入 %d/%d（Esc 停止）",
            s_pausedfmt="已暂停 %d/%d，点“继续”恢复",
            s_stoppedfmt="已停止 %d/%d，点“续写”接着打",
        ),
    },
    {
        "file": "lang/zh_TW.rc",
        "tag": "zh-TW",
        "language": "LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL",
        "langid_hex": "0x0404", "lang_hex": "0404", "cp_hex": "04B0",
        "font": "Microsoft JhengHei UI",
        "text": dict(
            main_title="Typaste " + VERSION,
            subtitle="Typing + Paste == Typaste",
            desc="Typaste 會攔截 Ctrl+V 貼上，並像打字一樣模擬輸入。",
            select_hint="在下面的文字裡選取一段，按熱鍵時只輸入選取的內容。",
            btn_readme="開啟說明(&O)", btn_settings="設定(&S)...", btn_about="關於(&A)",
            btn_exit="結束(&X)", btn_copy="複製到剪貼簿(&C)",
            settings_caption="Typaste 設定",
            grp_input="輸入", delay_type="輸入延遲(&D):", delay_start="開始前延遲(&T):",
            ms="毫秒", random="隨機間隔(&R)", lbl_speed="速度(&S):",
            grp_keys="熱鍵與提示音", hotkey="熱鍵(&H):", sound="提示音(&S):",
            browse="瀏覽(&B)...", play="試聽(&P)",
            grp_other="其他", ime="自動切換輸入法狀態(&I)", lbl_language="介面語言(&L):",
            btn_whitelist="程式白名單(&P)...",
            btn_reset="恢復預設", btn_ok="確定", btn_cancel="取消",
            whitelist_caption="程式白名單",
            whitelist_enable="僅在這些程式中攔截 Ctrl+V(&W)",
            whitelist_add="新增", whitelist_remove="移除", whitelist_browse="瀏覽(&R)...",
            about_caption="關於 Typaste",
            about_text="原作者：片山博文MZ（Katayama Hirofumi MZ）\\n"
                       "現維護者：YHuanheg\\n授權條款：MIT License\\n\\n"
                       "https://github.com/YHuanheg/Typaste",
            s_readme="\\README_zh_TW.txt",
            s_reset="是否將 Typaste 的設定恢復為預設值？",
            s_none="(無)",
            s_choosesound="選擇提示音檔案",
            s_soundfilter="WAV 音訊檔 (*.wav)|*.wav|所有檔案 (*.*)|*.*|",
            s_faildialog="無法建立對話方塊。",
            s_failhotkey="註冊熱鍵失敗。",
            s_autolang="跟隨系統",
            s_hotkeyfmt="熱鍵：%s",
            s_typing="正在輸入… 按 Esc 停止",
            s_ready="就緒",
            s_toolong="剪貼簿文字過長，無法輸入（上限 %d 字元）。",
            s_busy="正在輸入中，請稍候。",
            s_resetconfirm="確定要把所有設定恢復為預設值嗎？",
            s_whitelistfilter="程式 (*.exe)|*.exe|所有檔案 (*.*)|*.*|",
            s_trayshow="顯示主視窗",
            s_traysettings="設定...",
            s_trayabout="關於",
            s_trayexit="結束",
            s_copied="已複製到剪貼簿。",
            s_configpath="設定檔：%s",
            s_clipempty="（剪貼簿為空）",
            s_ime_close="關閉輸入法", s_ime_alphanum="切到英文模式",
            btn_pause="暫停(&P)", btn_continue="繼續(&C)", btn_resume="續寫(&I)",
            s_typingfmt="正在輸入 %d/%d（Esc 停止）",
            s_pausedfmt="已暫停 %d/%d，點「繼續」恢復",
            s_stoppedfmt="已停止 %d/%d，點「續寫」接著打",
        ),
    },
]


def render(lang, language_line=None, drop_version=False):
    values = dict(lang["text"])
    values.update({
        "tag": lang["tag"],
        "rc_language": language_line or lang["language"],
        "font": lang["font"],
        "langid_hex": lang["langid_hex"],
        "lang_hex": lang["lang_hex"],
        "cp_hex": lang["cp_hex"],
        "version": VERSION,
        "ver_tuple": VERSION_TUPLE,
    })
    text = TEMPLATE
    for key, value in values.items():
        text = text.replace("@@" + key + "@@", value)
    if drop_version:
        start = text.find("//////////////////////////////////////////////////////////////////////////////\\n// RT_VERSION")
        end = text.find("//////////////////////////////////////////////////////////////////////////////\\n// RT_STRING")
        if start >= 0 and end > start:
            text = text[:start] + text[end:]
    if "@@" in text:
        raise RuntimeError("unreplaced placeholder")
    return text


def main():
    for lang in LANGS:
        path = os.path.join(ROOT, lang["file"])
        data = render(lang).encode("utf-16-le")
        with open(path, "wb") as fp:
            fp.write(data)
        print("generated %s (%d bytes)" % (lang["file"], len(data)))

    # The LANG_NEUTRAL fallback: English text without the version block
    neutral = render(LANGS[0], language_line="LANG_NEUTRAL, SUBLANG_NEUTRAL", drop_version=True)
    neutral = neutral.replace("// Typaste localized resources for en-US",
                              "// Typaste LANG_NEUTRAL fallback resources\\n// GENERATED FILE --- edit tools/gen_lang.py instead of this file.")
    path = os.path.join(ROOT, "lang/neutral.rc")
    data = neutral.encode("utf-16-le")
    with open(path, "wb") as fp:
        fp.write(data)
    print("generated lang/neutral.rc (%d bytes)" % len(data))
    return 0


if __name__ == "__main__":
    sys.exit(main())
