<#
.SYNOPSIS
  アプリのメインウィンドウを前面に出し、その範囲だけを撮影して、幅を MaxWidth 以下に縮小して PNG で保存する。
  デスクトップ全体（タスクバー・ほかのアプリ）は写さない。置き場所の規則は Docs/Screenshots/README.md。

.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Screenshots/Capture-Window.ps1 `
    -Process UnrealEditor -Out Docs/Screenshots/2026-11-04/test-court/court_oblique.png
#>
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Process = "UnrealEditor",
    [ValidateRange(320, 3840)][int]$MaxWidth = 1280
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
# C# の部分は英語だけで書く（Windows PowerShell 5.1 が文字コードを取り違えても壊れないように）
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class CaptureNative {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    // DWMWA_EXTENDED_FRAME_BOUNDS (9): the visible bounds, without the invisible resize frame around the window
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hWnd, int attribute, out RECT rect, int size);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);

    // Largest visible top-level window of the process (the editor itself, not a small floating window)
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
[void][CaptureNative]::SetProcessDPIAware()  # 座標と画面の画素を合わせる

$proc = Get-Process -Name $Process -ErrorAction Stop | Select-Object -First 1
$window = [CaptureNative]::LargestWindowOf($proc.Id)
if ($window -eq [IntPtr]::Zero) { throw "No visible window for process '$Process'" }

# ほかのウィンドウが重なっていると、その範囲に写ってしまうので、先に前面に出す。
# Windows は裏のプロセスからの前面化を止めるので、Alt キーを一度押してから前面に出す
$VK_MENU = 0x12; $KEYEVENTF_KEYUP = 2
[CaptureNative]::keybd_event($VK_MENU, 0, 0, [UIntPtr]::Zero)
[CaptureNative]::keybd_event($VK_MENU, 0, $KEYEVENTF_KEYUP, [UIntPtr]::Zero)
[void][CaptureNative]::SetForegroundWindow($window)
Start-Sleep -Milliseconds 500
if ([CaptureNative]::GetForegroundWindow() -ne $window) {
    throw "Could not bring '$Process' to the front (nothing was captured)"
}

$rect = New-Object CaptureNative+RECT
# 見えている範囲（ウィンドウの外側の見えない枠を除く）。GetWindowRect は枠の分まで含み、最大化したウィンドウでは
# タスクバーやデスクトップが端に数画素写る（2026-11-18 に見つけた）。取れなければ GetWindowRect
if ([CaptureNative]::DwmGetWindowAttribute($window, 9, [ref]$rect, [System.Runtime.InteropServices.Marshal]::SizeOf($rect)) -ne 0) {
    [void][CaptureNative]::GetWindowRect($window, [ref]$rect)
}
# さらに、そのウィンドウのある画面の作業領域（タスクバーを除く）に切り詰める
$screen = [System.Windows.Forms.Screen]::FromHandle($window).WorkingArea
$left = [Math]::Max($rect.Left, $screen.Left); $top = [Math]::Max($rect.Top, $screen.Top)
$width = [Math]::Min($rect.Right, $screen.Right) - $left
$height = [Math]::Min($rect.Bottom, $screen.Bottom) - $top
if ($width -le 0 -or $height -le 0) { throw "The window has no visible area (minimized?)" }

$full = New-Object System.Drawing.Bitmap $width, $height
$g = [System.Drawing.Graphics]::FromImage($full)
try {
    $g.CopyFromScreen($left, $top, 0, 0, (New-Object System.Drawing.Size $width, $height))
    $scale = [Math]::Min(1.0, $MaxWidth / $width)
    $w = [int]($width * $scale); $h = [int]($height * $scale)
    $small = New-Object System.Drawing.Bitmap $w, $h
    $g2 = [System.Drawing.Graphics]::FromImage($small)
    try {
        $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g2.DrawImage($full, 0, 0, $w, $h)
        $dir = Split-Path -Parent ([System.IO.Path]::GetFullPath($Out))
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        $small.Save([System.IO.Path]::GetFullPath($Out), [System.Drawing.Imaging.ImageFormat]::Png)
        "saved $Out ($w x $h)"
    }
    finally { $g2.Dispose(); $small.Dispose() }
}
finally { $g.Dispose(); $full.Dispose() }
