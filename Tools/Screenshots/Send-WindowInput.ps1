<#
.SYNOPSIS
  アプリのいちばん大きいウィンドウを前面に出し、クリックかキーを送る（PIE の操作の確認に使う）。
  クリックの位置は、Capture-Window.ps1 で撮った画像の座標で渡す（同じウィンドウ・同じ切り詰め・同じ縮小）。
  前面に出せなければ、何も送らずにエラーで止まる（ほかのアプリに入力を送らない）。
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Screenshots/Send-WindowInput.ps1 -X 548 -Y 355
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Screenshots/Send-WindowInput.ps1 -Key D -HoldMs 1000
#>
param(
    [int]$X = -1,
    [int]$Y = -1,
    [string]$Key = "",
    [ValidateRange(10, 10000)][int]$HoldMs = 60,
    [string]$Process = "UnrealEditor",
    [int]$MaxWidth = 1280
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class ClickNative {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
    public static IntPtr LargestWindowOf(int processId) {
        IntPtr best = IntPtr.Zero; long bestArea = 0;
        EnumWindows((h, l) => {
            uint pid; GetWindowThreadProcessId(h, out pid);
            RECT r;
            if (pid == processId && IsWindowVisible(h) && GetWindowRect(h, out r)) {
                long area = (long)(r.Right - r.Left) * (r.Bottom - r.Top);
                if (area > bestArea) { bestArea = area; best = h; }
            }
            return true;
        }, IntPtr.Zero);
        return best;
    }
}
"@
[void][ClickNative]::SetProcessDPIAware()
$proc = Get-Process -Name $Process -ErrorAction Stop | Select-Object -First 1
$window = [ClickNative]::LargestWindowOf($proc.Id)
if ($window -eq [IntPtr]::Zero) { throw "No visible window for process '$Process'" }
[ClickNative]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
[ClickNative]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
[void][ClickNative]::SetForegroundWindow($window)
Start-Sleep -Milliseconds 400
if ([ClickNative]::GetForegroundWindow() -ne $window) { throw "Could not bring '$Process' to the front (nothing was sent)" }

if ($X -ge 0 -and $Y -ge 0) {
    $rect = New-Object ClickNative+RECT
    [void][ClickNative]::GetWindowRect($window, [ref]$rect)
    $screen = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $left = [Math]::Max($rect.Left, $screen.Left); $top = [Math]::Max($rect.Top, $screen.Top)
    $width = [Math]::Min($rect.Right, $screen.Right) - $left
    $scale = [Math]::Min(1.0, $MaxWidth / $width)
    $sx = [int]($left + $X / $scale); $sy = [int]($top + $Y / $scale)
    [void][ClickNative]::SetCursorPos($sx, $sy)
    Start-Sleep -Milliseconds 150
    [ClickNative]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)  # left down
    Start-Sleep -Milliseconds 60
    [ClickNative]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)  # left up
    "clicked image ($X, $Y) -> screen ($sx, $sy)"
}
if ($Key -ne "") {
    $vk = [byte][System.Windows.Forms.Keys]::$Key
    [ClickNative]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $HoldMs
    [ClickNative]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
    "pressed $Key for $HoldMs ms"
}
