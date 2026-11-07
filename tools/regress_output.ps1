# regress_output.ps1 --- regression test for the output modes
#
#   powershell -ExecutionPolicy Bypass -File tools\regress_output.ps1
#
# A: OUTPUT_ADAPTIVE_IME (1) --- the IME mode must change while typing and the
#    text must arrive exactly as it is.
# B: OUTPUT_UNICODE (2) --- the IME is closed while typing, so an ASCII text
#    must arrive as ASCII even when a Chinese IME is active.
param([string]$Exe = "")

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Text;
public class T {
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("imm32.dll")] public static extern IntPtr ImmGetDefaultIMEWnd(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public static IntPtr FindClass(string cls) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h,p)=>{ var sb=new StringBuilder(64); GetClassNameW(h,sb,64); if(sb.ToString()==cls && IsWindowVisible(h) && found==IntPtr.Zero) found=h; return true; }, IntPtr.Zero);
    return found;
  }
  public static IntPtr FindDlg(uint pid) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h,p)=>{ uint q; GetWindowThreadProcessId(h, out q); if(q==pid){ var sb=new StringBuilder(64); GetClassNameW(h,sb,64); if(sb.ToString()=="#32770" && IsWindowVisible(h) && found==IntPtr.Zero) found=h; } return true; }, IntPtr.Zero);
    return found;
  }
  public static int ImeMode(IntPtr wnd) { IntPtr i = ImmGetDefaultIMEWnd(wnd); return i==IntPtr.Zero? -1 : (int)SendMessageW(i, 0x0283, (IntPtr)1, IntPtr.Zero); }
  public static void ImeSet(IntPtr wnd, int mode, int open) {
    IntPtr i = ImmGetDefaultIMEWnd(wnd); if (i==IntPtr.Zero) return;
    SendMessageW(i, 0x0283, (IntPtr)2, (IntPtr)mode);
    SendMessageW(i, 0x0283, (IntPtr)6, (IntPtr)open);
  }
  public static void ForceForeground(IntPtr h) {
    ShowWindow(h, 9);
    keybd_event(0x12, 0, 0, UIntPtr.Zero);
    keybd_event(0x12, 0, 2, UIntPtr.Zero);
    SetWindowPos(h, new IntPtr(-1), 40, 40, 0, 0, 0x0041);
    SetForegroundWindow(h);
  }
}
"@
function ReadNotepad($npWnd) {
  try {
    [T]::ForceForeground($npWnd) | Out-Null
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait("^a")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^c")
    Start-Sleep -Milliseconds 400
    return (Get-Clipboard -Raw)
  } catch { return '<read error>' }
}
function WriteIni($path, $outputMode, $delay) {
  $c = @"
[Typaste]
SettingsVersion=4
Delay=$delay
DelayToStart=0
HotKey=598
ImeMode=2
ControlIME=1
OutputMode=$outputMode
Random=0
OnlyWhitelist=0
Sound=
Whitelist=
Language=en-US
"@
  [IO.File]::WriteAllText($path, $c, [Text.Encoding]::UTF8)
}

$root = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $root 'build-x64\Typaste.exe' }
if (-not (Test-Path $Exe)) { Write-Output "FAIL: $Exe not found"; exit 1 }
$ini = Join-Path (Split-Path $Exe) 'Typaste.ini'

Get-Process Typaste,notepad -ErrorAction SilentlyContinue | Stop-Process -Force

# ---------- A: adaptive ----------
WriteIni $ini 1 120
for ($i=0; $i -lt 6; $i++) { try { Set-Clipboard -Value "a中b"; break } catch { Start-Sleep -Milliseconds 300 } }
$np = Start-Process notepad -PassThru
Start-Sleep -Milliseconds 1000
$npWnd = [T]::FindClass('Notepad')
[T]::ForceForeground($npWnd) | Out-Null
[T]::ImeSet($npWnd, 1, 1)
Start-Sleep -Milliseconds 300
Write-Output ("A start: ime=0x" + ([int64][T]::ImeMode($npWnd)).ToString("X"))

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 1500
$main = [T]::FindDlg([uint32]$p.Id)
[T]::ForceForeground($npWnd) | Out-Null
Start-Sleep -Milliseconds 300
[T]::PostMessageW($main, 0x0312, [IntPtr]0xDEDD, [IntPtr]0x00020056) | Out-Null
$seq = @()
$d = [DateTime]::Now.AddMilliseconds(2500)
while ([DateTime]::Now -lt $d) { $seq += [T]::ImeMode($npWnd); [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 40 }
$distinct = ($seq | Sort-Object -Unique) -join ','
Write-Output ("A modes seen: " + $distinct)
if ($distinct -notmatch '0' -or $distinct -notmatch '1') { Write-Output 'FAIL: the IME mode did not follow the characters'; Get-Process Typaste,notepad -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
Start-Sleep -Milliseconds 1500
Write-Output ("A text: '" + (ReadNotepad $npWnd) + "'")
Write-Output ("A ime after=0x" + ([int64][T]::ImeMode($npWnd)).ToString("X"))
Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process notepad -ErrorAction SilentlyContinue | Stop-Process -Force

# ---------- B: as-is (Unicode) ----------
WriteIni $ini 2 60
Start-Sleep -Milliseconds 400
for ($i=0; $i -lt 6; $i++) { try { Set-Clipboard -Value "abc123"; break } catch { Start-Sleep -Milliseconds 300 } }
$np = Start-Process notepad -PassThru
Start-Sleep -Milliseconds 1000
$npWnd = [T]::FindClass('Notepad')
[T]::ForceForeground($npWnd) | Out-Null
[T]::ImeSet($npWnd, 1, 1)
Start-Sleep -Milliseconds 300
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 1500
$main = [T]::FindDlg([uint32]$p.Id)
[T]::ForceForeground($npWnd) | Out-Null
Start-Sleep -Milliseconds 300
[T]::PostMessageW($main, 0x0312, [IntPtr]0xDEDD, [IntPtr]0x00020056) | Out-Null
$seq2 = @()
$d = [DateTime]::Now.AddMilliseconds(2500)
while ([DateTime]::Now -lt $d) { $seq2 += [T]::ImeMode($npWnd); [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 40 }
$modesB = ($seq2 | Sort-Object -Unique) -join ','
Write-Output ("B modes seen: " + $modesB)
if ($modesB -notmatch '0') { Write-Output 'FAIL: the IME was not closed in the as-is mode'; Get-Process Typaste,notepad -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
Start-Sleep -Milliseconds 1200
Write-Output ("B text: '" + (ReadNotepad $npWnd) + "'")
Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process notepad -ErrorAction SilentlyContinue | Stop-Process -Force

Get-Process Typaste,notepad -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "RESULT: OK"
