# regress_typing.ps1 --- regression test for pause / continue / resume
#
#   powershell -ExecutionPolicy Bypass -File tools\regress_typing.ps1
#
# The test drives the real UI through the window messages the buttons send:
#   pshPause (2023), pshResume (2024), WM_HOTKEY and the Esc key.
# It reads the status line (txtStatus, 2003) instead of the target window, so it
# does not need to steal the foreground focus.
param([string]$Exe = "")

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class R {
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public static IntPtr Find(uint pid) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h,p)=>{ uint q; GetWindowThreadProcessId(h, out q); if(q==pid){ var sb=new StringBuilder(64); GetClassNameW(h,sb,64); if(sb.ToString()=="#32770" && IsWindowVisible(h) && found==IntPtr.Zero) found=h; } return true; }, IntPtr.Zero);
    return found;
  }
  public static string Txt(IntPtr h, int id){ var sb=new StringBuilder(512); GetWindowTextW(GetDlgItem(h,id),sb,512); return sb.ToString(); }
}
"@

$root = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $root 'build-x64\Typaste.exe' }
if (-not (Test-Path $Exe)) { Write-Output "FAIL: $Exe not found"; exit 1 }

function Pump([int]$ms) { $d=[DateTime]::Now.AddMilliseconds($ms); while([DateTime]::Now -lt $d){ [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 10 } }
function Status($h) { return [R]::Txt($h, 2003) }
function Fail($msg) { Write-Output ("FAIL: " + $msg); Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
function WaitDone($h, $sec) {
  $until = [DateTime]::Now.AddSeconds($sec)
  while ([DateTime]::Now -lt $until) {
    Pump 300
    $s = Status $h
    if ($s -match '就绪|Ready|待機') { return $s }
  }
  return (Status $h)
}

Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force
$ini = Join-Path (Split-Path $Exe) 'Typaste.ini'
Remove-Item $ini -Force -ErrorAction SilentlyContinue
Set-Clipboard -Value ("0123456789" * 6)

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 1500
$main = [R]::Find([uint32]$p.Id)
if ($main -eq [IntPtr]::Zero) { Fail 'main window not found' }
Write-Output ("start: " + (Status $main))

# --- pause freezes the progress ---
[R]::PostMessageW($main, 0x0312, [IntPtr]0xDEDD, [IntPtr]0x00020056) | Out-Null
Pump 400
[R]::PostMessageW($main, 0x0111, [IntPtr]2023, [IntPtr]::Zero) | Out-Null
Pump 250
$a = Status $main
Pump 800
$b = Status $main
if ($a -ne $b) { Fail "progress is not frozen while paused ($a -> $b)" }
Write-Output ("ok: paused '" + $a + "'")

# --- continue moves on and finishes ---
[R]::PostMessageW($main, 0x0111, [IntPtr]2023, [IntPtr]::Zero) | Out-Null
Pump 8000
$done = Status $main
if ($done -notmatch '就绪|Ready|待機') { Fail "typing did not finish: $done" }
Write-Output ("ok: finished '" + $done + "'")

# --- Esc stops, Continue resumes ---
[R]::PostMessageW($main, 0x0312, [IntPtr]0xDEDD, [IntPtr]0x00020056) | Out-Null
Pump 600
[R]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
Pump 400
[R]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)
Pump 700
$stopped = Status $main
$en = [R]::IsWindowEnabled([R]::GetDlgItem($main, 2024))
if (-not $en) { Fail "Continue is disabled after the stop: $stopped" }
Write-Output ("ok: stopped '" + $stopped + "'")

[R]::PostMessageW($main, 0x0111, [IntPtr]2024, [IntPtr]::Zero) | Out-Null
Pump 500
$cont = Status $main
if ($cont -eq $stopped) { Fail "Continue did not restart the typing" }
Write-Output ("ok: continued '" + $cont + "'")

Pump 8000
$final = Status $main
if ($final -notmatch '就绪|Ready|待機') { Fail "resumed typing did not finish: $final" }
Write-Output ("ok: final '" + $final + "'")

Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output 'RESULT: OK'
