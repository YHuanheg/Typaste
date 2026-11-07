# regress_settings.ps1 --- regression test for the settings dialog
#
#   powershell -ExecutionPolicy Bypass -File tools\regress_settings.ps1
#
# Covers the regression fixed in 1.4.1: pressing OK (or Cancel) in the settings
# dialog must rebuild the main window instead of terminating the program.
param([string]$Exe = "")

$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class T {
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public static List<IntPtr> Dlgs(uint pid) {
    List<IntPtr> l = new List<IntPtr>();
    EnumWindows((h,p)=>{ uint q; GetWindowThreadProcessId(h, out q); if(q==pid){ var sb=new StringBuilder(64); GetClassNameW(h,sb,64); if(sb.ToString()=="#32770" && IsWindowVisible(h)) l.Add(h);} return true; }, IntPtr.Zero);
    return l;
  }
  public static string Text(IntPtr h){ var sb=new StringBuilder(256); GetWindowTextW(h,sb,256); return sb.ToString(); }
}
"@

$root = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $root 'build-x64\Typaste.exe' }
if (-not (Test-Path $Exe)) { Write-Output "FAIL: $Exe not found"; exit 1 }

$ini = Join-Path (Split-Path $Exe) 'Typaste.ini'
$regKey = 'HKCU:\SOFTWARE\Katayama Hirofumi MZ\Typaste'

function Fail($msg) { Write-Output ("FAIL: " + $msg); Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }

Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $ini -Force -ErrorAction SilentlyContinue

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 1500
$dlgs = [T]::Dlgs([uint32]$p.Id)
if ($dlgs.Count -eq 0) { Fail 'main window did not appear' }
$main = $dlgs[0]

# --- 1) OK rebuilds the window ---
[T]::PostMessageW($main, 0x0111, [IntPtr]1024, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1100
$st = ([T]::Dlgs([uint32]$p.Id) | Where-Object { $_ -ne $main })[0]
if (-not $st) { Fail 'settings dialog did not open' }
[T]::PostMessageW($st, 0x0111, [IntPtr]1, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1400
if (-not (Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) { Fail 'process exited after OK' }
$main2 = ([T]::Dlgs([uint32]$p.Id))[0]
if (-not $main2) { Fail 'no main window after OK' }
Write-Output ("ok: rebuilt " + $main + " -> " + $main2)

# --- 2) Cancel rebuilds the window ---
[T]::PostMessageW($main2, 0x0111, [IntPtr]1024, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1100
$st2 = ([T]::Dlgs([uint32]$p.Id) | Where-Object { $_ -ne $main2 })[0]
if (-not $st2) { Fail 'settings dialog did not open (2)' }
[T]::PostMessageW($st2, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1400
if (-not (Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) { Fail 'process exited after Cancel' }
$main3 = ([T]::Dlgs([uint32]$p.Id))[0]
if (-not $main3) { Fail 'no main window after Cancel' }
Write-Output ("ok: cancel rebuilt " + $main2 + " -> " + $main3)

# --- 3) switching the language rebuilds in the new language ---
$before = [T]::Text([T]::GetDlgItem($main3, 1024))
[T]::PostMessageW($main3, 0x0111, [IntPtr]1024, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1100
$st3 = ([T]::Dlgs([uint32]$p.Id) | Where-Object { $_ -ne $main3 })[0]
$cmb = [T]::GetDlgItem($st3, 1137)   # cmb2 = 0x0471
if (-not $cmb) { Fail 'language combo not found' }
$target = 1
if ($before -like '*Settings*') { $target = 3 }   # switch to zh-CN when in English
[T]::SendMessageW($cmb, 0x014E, [IntPtr]$target, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 200
[T]::PostMessageW($st3, 0x0111, [IntPtr]1, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1500
if (-not (Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) { Fail 'process exited after language switch' }
$main4 = ([T]::Dlgs([uint32]$p.Id))[0]
$after = [T]::Text([T]::GetDlgItem($main4, 1024))
if ($after -eq $before) { Fail 'language did not change' }
Write-Output ("ok: language '" + $before + "' -> '" + $after + "'")

Remove-Item $regKey -Recurse -Force -ErrorAction SilentlyContinue
Get-Process Typaste -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output 'RESULT: OK'
