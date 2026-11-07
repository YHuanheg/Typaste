
[![build](https://github.com/YHuanheg/Typaste/actions/workflows/build.yml/badge.svg)](https://github.com/YHuanheg/Typaste/actions/workflows/build.yml)
# ![](typaste.png "") Typaste

Typing + Paste == Typaste.

![Screenshot](img/screenshot.png)

Typaste intercepts the pasting process by Ctrl+V while its window is displayed
and emulates the text input as if it were typed on the keyboard. This allows you
to break through the "paste prohibition".

Press the Esc key to stop typing.

## Features

- Types any clipboard text character by character, like a human would
- **CJK and emoji work**: characters that the keyboard layout cannot produce are
  sent as Unicode input (`SendInput` + `KEYEVENTF_UNICODE`), not as `WM_CHAR`
- Typing runs on a worker thread, so the window stays responsive
- **Pause / Continue**: hold the typing at any character and resume it later
- **Resume after a stop**: press Esc, then click Continue to type the rest
- Configurable typing delay / start delay / random interval
- Global hot-key (Ctrl+V by default)
- **Input method control**: while typing, the IME is switched to English mode
  (default) or closed, then the previous state is restored
- **Adaptive typing**: each character gets the IME mode it needs (ASCII -> English,
  CJK -> the native IME mode)
- **As-is output**: everything is sent as Unicode input with the IME out of the way,
  so the text arrives exactly as it is
- Optional typing sound
- Four UI languages: English, Japanese, Simplified Chinese, Traditional Chinese
- High-DPI aware, modern dialog layout, About box, one-click reset to defaults
- **Portable configuration**: `Typaste.ini` next to the exe (falls back to the registry)
- **Typing speed slider** in the settings dialog
- **Clipboard preview**: the current clipboard text is shown and can be edited; selecting a
  part of it makes the hot-key type only the selection
- Tray icon with a context menu; the window minimizes to the notification area
- **Program whitelist**: intercept Ctrl+V only in the programs you choose
- Per-Monitor V2 DPI awareness (crisp on every monitor)
- CI builds for x64 and Win32, optional Authenticode signing
- Single instance; settings live in `Typaste.ini`, or in the registry when the exe folder is read-only

## Input method

While the text is typed, Typaste takes the IME out of the way:

| Setting | Behaviour |
| --- | --- |
| Switch to English mode (default) | Keeps the IME alive and sets `IME_CMODE_ALPHANUMERIC`, so the characters pass straight through |
| Close the IME | The behaviour of the older versions: the IME is closed and reopened |
| Off | The IME is left alone |

The open state and the conversion mode are saved before the typing and restored
afterwards, so Chinese punctuation and the full/half width setting come back.

### Character output

| Output | What happens | When to use it |
| --- | --- | --- |
| Automatic (default) | ASCII goes through the keyboard, the rest as Unicode input | Balanced speed and compatibility |
| Adapt the IME per character | The IME is switched for every character: ASCII -> `IME_CMODE_ALPHANUMERIC`, CJK -> `IME_CMODE_NATIVE` | You want the IME to follow the text itself |
| As is (ignore the IME) | Every character is sent as Unicode input and the IME is closed for the time of the typing | The target is a TSF/UWP editor that would otherwise eat the characters |

The as-is mode is the answer to "type exactly this, whatever the IME is doing":
the IME is taken out of the loop for the duration of the job and restored after.

## Pause, Continue and Resume

While Typaste is typing you can press **Pause** at any time: the engine stops at
a character boundary, so the target never receives a half character. **Continue**
picks up from exactly that character.

Pressing Esc stops the typing; the main window then shows how much was typed and
enables **Continue**, which types only the remaining part of the text.

## Portable configuration

On the first run Typaste creates `Typaste.ini` next to the executable (when the
folder is writable) and keeps all settings there — language, delays, hot-key,
sound, whitelist. Delete the file to fall back to the registry again.

## Download

Grab the latest x64 build from the
[Releases](https://github.com/YHuanheg/Typaste/releases) page.

## Supported Platforms

- Windows Vista / 7 / 8.1 / 10 / 11, **x64** (primary target)
- The legacy Win32 (x86) build is still supported

## Languages

"Follow the system" keeps the UI in the language of Windows. To pick a language
explicitly, open **Settings...** and choose it in **Interface language**. The
change takes effect immediately.

## Building

With Visual Studio / MSVC (CMake 3.15+):

```
cmake -S . -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-x64 --config Release
```

`-A x64` is the default: the project selects it when the generator is MSVC.
Without CMake, run `build_x64.bat`: it produces `build-x64\Typaste.exe` with the
static CRT and the same hardening flags.

The localized resources are generated from a single layout by
`python tools/gen_lang.py`. Run `tools\verify_lang.bat` to check that all five language blocks (four
locales plus the `LANG_NEUTRAL` fallback) made it into the binary.

## Documentation

- [docs/index.md](docs/index.md) — documentation index
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — modules and the design decisions
- [docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md) — improvement report (history)
- [docs/SIGNING.md](docs/SIGNING.md) — optional code signing

## Credits

- Original author: **Katayama Hirofumi MZ** (katahiromz)
- Maintainer: **YHuanheg** — https://github.com/YHuanheg

## License

MIT License. See [LICENSE.txt](LICENSE.txt).

## Warning

Please do not use this software for illegal purposes.
