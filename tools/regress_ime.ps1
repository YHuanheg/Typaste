# regress_ime.ps1 --- regression test for the IME control around the typing
#
#   powershell -ExecutionPolicy Bypass -File tools\regress_ime.ps1
#
# The test puts Notepad into the Chinese IME mode, types into it and checks that
# the IME is switched to the English mode while typing and restored afterwards.
param([string]$Exe = "")

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class F {
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("imm32.dll")] public static extern IntPtr ImmGetDefaultIMEWnd(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
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
  public static int ImeMode(IntPtr wnd) {
    IntPtr i = ImmGetDefaultIMEWnd(wnd);
    return i == IntPtr.Zero ? -1 : (int)SendMessageW(i, 0x0283, (IntPtr)1, IntPtr.Zero);
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

$root = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $root 'build-x64\Typaste.exe' }
if (-not (Test-Path $Exe)) { Write-Output "FAIL: $Exe not found"; exit 1 }
function Fail($m) { Write-Output ("FAIL: " + $m); Get-Process Typaste,notepad -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }

Get-Process Typaste,notepad -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item (Join-Path (Split-Path $Exe) 'Typaste.ini') -Force -ErrorAction SilentlyContinue
for ($i = 0; $i -lt 6; $i++) { try { Set-Clipboard -Value ("abcdefghij" * 3); break } catch { Start-Sleep -Milliseconds 400 } }

$np = Start-Process notepad -PassThru
Start-Sleep -Milliseconds 900
$npWnd = [F]::FindClass('Notepad')
if ($npWnd -eq [IntPtr]::Zero) { Fail 'notepad window not found' }
[F]::ForceForeground($npWnd) | Out-Null
Start-Sleep -Milliseconds 500

$ime = [F]::ImmGetDefaultIMEWnd($npWnd)
if ($ime -eq [IntPtr]::Zero) { Fail 'the target has no IME window (is a Chinese IME installed?)' }
[F]::SendMessageW($ime, 0x0283, [IntPtr]6, [IntPtr]1) | Out-Null   # open
[F]::SendMessageW($ime, 0x0283, [IntPtr]2, [IntPtr]1) | Out-Null   # native (Chinese)
Start-Sleep -Milliseconds 200
$before = [F]::ImeMode($npWnd)
Write-Output ("before: mode=0x" + ([int64]$before).ToString("X"))
if ($before -ne 1) { Fail 'could not put the target into the Chinese IME mode' }

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 1500
$main = [F]::FindDlg([uint32]$p.Id)
if ($main -eq [IntPtr]::Zero) { Fail 'main window not found' }
[F]::ForceForeground($npWnd) | Out-Null
Start-Sleep -Milliseconds 400
[F]::PostMessageW($main, 0x0312, [IntPtr]0xDEDD, [IntPtr]0x00020056) | Out-Null
Start-Sleep -Milliseconds 700
$during = [F]::ImeMode($npWnd)
Write-Output ("during: mode=0x" + ([int64]$during).ToString("X"))
if ($during -ne 0) { Fail 'the IME was not switched to the English mode while typing' }

$d = [DateTime]::Now.AddMilliseconds(5000); while ([DateTime]::Now -lt $d) { [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 20 }
$after = [F]::ImeMode($npWnd)
Write-Output ("after : mode=0x" + ([int64]$after).ToString("X"))
if ($after -ne $before) { Fail 'the IME state was not restored' }

Get-Process Typaste,notepad -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output 'RESULT: OK'
