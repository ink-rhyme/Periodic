# 从 WSL 侧调用，调整 WSLg 窗口在 Windows 上的呈现
# 用法: powershell -File winframe.ps1 decor|full|restore|watch
#   decor   : 去圆角/阴影，并持续 2 秒，防止 Weston 立刻回写样式
#   full    : 钉到主屏整屏矩形（含任务栏），同时起守护循环持续纠正
#   restore : 按记录的原矩形还原，停止守护
#   watch   : 内部用，循环纠正窗口
param([string]$Mode = "decor")

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public class PWin {
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc f, IntPtr l);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern int GetClassName(IntPtr hWnd, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr MonitorFromPoint(POINT p, uint flags);
    [DllImport("user32.dll")] public static extern bool GetMonitorInfo(IntPtr mon, ref MONITORINFO mi);
    [DllImport("user32.dll", SetLastError = true)] public static extern int GetWindowLong(IntPtr h, int idx);
    [DllImport("user32.dll", SetLastError = true)] public static extern int SetWindowLong(IntPtr h, int idx, int val);
    [DllImport("dwmapi.dll")] public static extern int DwmSetWindowAttribute(IntPtr h, int attr, ref int val, int size);

    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct MONITORINFO { public int cbSize; public RECT rcMonitor; public RECT rcWork; public int dwFlags; }

    // WSLg 应用窗口的类名是 RAIL_WINDOW；终端窗口类名是 CASCADIA_HOSTING_WINDOW_CLASS，
    // 它的标题里同样带 "Periodic"，只按标题找会误伤终端，所以先按类名筛
    public static IntPtr Find(string titleKey) {
        IntPtr rail = IntPtr.Zero, railTitled = IntPtr.Zero, anyTitled = IntPtr.Zero;
        long railArea = -1, railTitledArea = -1, anyTitledArea = -1;
        EnumWindows((h, l) => {
            if (!IsWindowVisible(h)) return true;
            RECT r; GetWindowRect(h, out r);
            long a = (long)(r.Right - r.Left) * (r.Bottom - r.Top);
            var sb = new StringBuilder(512); GetWindowText(h, sb, 512);
            var cn = new StringBuilder(256); GetClassName(h, cn, 256);
            bool titleOk = sb.ToString().Contains(titleKey);
            if (cn.ToString() == "RAIL_WINDOW") {
                if (a > railArea) { railArea = a; rail = h; }
                if (titleOk && a > railTitledArea) { railTitledArea = a; railTitled = h; }
            }
            if (titleOk && a > anyTitledArea) { anyTitledArea = a; anyTitled = h; }
            return true;
        }, IntPtr.Zero);
        if (railTitled != IntPtr.Zero) return railTitled;
        if (rail != IntPtr.Zero) return rail;
        return anyTitled;
    }

    public static int[] ScreenRect() {
        MONITORINFO mi = new MONITORINFO(); mi.cbSize = Marshal.SizeOf(mi);
        IntPtr mon = MonitorFromPoint(new POINT { X = 0, Y = 0 }, 1);
        GetMonitorInfo(mon, ref mi);
        return new int[] { mi.rcMonitor.Left, mi.rcMonitor.Top,
                           mi.rcMonitor.Right - mi.rcMonitor.Left,
                           mi.rcMonitor.Bottom - mi.rcMonitor.Top };
    }

    // Windows 对带 WS_THICKFRAME / WS_CAPTION 的窗口会画投影，改成无框 POPUP 才彻底没有
    public static void StripFrame(IntPtr h) {
        const int GWL_STYLE = -16;
        int style = GetWindowLong(h, GWL_STYLE);
        int mask = unchecked((int)0xFF3CF7FF);
        int want = (int)((uint)(style & mask) | 0x80000000u);   // 去掉标题栏/粗细框，补 WS_POPUP
        if (style != want) {
            SetWindowLong(h, GWL_STYLE, want);
            const uint SWP_FRAMECHANGED = 0x0020, SWP_NOMOVE = 0x0002, SWP_NOSIZE = 0x0001, SWP_NOZORDER = 0x0004;
            SetWindowPos(h, IntPtr.Zero, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
        }
        int noRound = 1;   // DWMWCP_DONOTROUND
        DwmSetWindowAttribute(h, 33, ref noRound, 4);
        int noNc = 1;      // DWMNCRP_DISABLED
        DwmSetWindowAttribute(h, 2, ref noNc, 4);
    }

    public static int[] Rect(IntPtr h) {
        RECT r; GetWindowRect(h, out r);
        return new int[] { r.Left, r.Top, r.Right - r.Left, r.Bottom - r.Top };
    }

    public static bool AtRect(IntPtr h, int x, int y, int w, int hh) {
        RECT r; GetWindowRect(h, out r);
        return r.Left == x && r.Top == y && (r.Right - r.Left) == w && (r.Bottom - r.Top) == hh;
    }

    public static void Pin(IntPtr h, int x, int y, int w, int hh) {
        const uint SWP_SHOW = 0x0040, SWP_NOACTIVATE = 0x0010;
        SetWindowPos(h, new IntPtr(-1), x, y, w, hh, SWP_SHOW | SWP_NOACTIVATE);   // HWND_TOPMOST
    }

    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
    [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr dc, int idx);

    public static int Dpi() {
        IntPtr dc = GetDC(IntPtr.Zero);
        int d = GetDeviceCaps(dc, 88);   // LOGPIXELSX
        ReleaseDC(IntPtr.Zero, dc);
        return d > 0 ? d : 96;
    }

    public static string RectText(IntPtr h) {
        int[] r = Rect(h);
        return "" + r[0] + "," + r[1] + " " + r[2] + "x" + r[3];
    }
}
"@

$stateFile = Join-Path $env:TEMP "periodic_frame.txt"
$lockFile = Join-Path $env:TEMP "periodic_fullscreen.lock"
$titleKey = "周期表"
$self = $PSCommandPath

# 把主屏尺寸（DIP）写到 WSL 的 /tmp，供应用把 GL 画布切成同样大小
$parts = $PSCommandPath.Split('\')
if ($parts.Length -gt 3) {
    $wslRoot = '\\wsl$\' + $parts[3]
    $screenTxt = Join-Path $wslRoot 'tmp\periodic_screen.txt'
    $sr = [PWin]::ScreenRect()
    try { Set-Content -LiteralPath $screenTxt -Value ("{0} {1}" -f $sr[2], $sr[3]) -Encoding ascii -ErrorAction Stop } catch { }
}

function Read-Lock {
    if (Test-Path -LiteralPath $lockFile) { return (Get-Content -LiteralPath $lockFile -Raw).Trim() }
    return ""
}

# 反复纠正：Weston 会在我们钉屏之后再把几何写回自己的尺寸
function Watch-Loop([int]$seconds) {
    $t = Read-Lock
    if (-not $t) { return }
    $p = $t.Split(' ')
    if ($p.Length -ne 4) { return }
    $tx = [int]$p[0]; $ty = [int]$p[1]; $tw = [int]$p[2]; $th = [int]$p[3]
    $end = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $end) {
        if (-not (Test-Path -LiteralPath $lockFile)) { return }
        $h = [PWin]::Find($titleKey)
        if ($h -eq [IntPtr]::Zero) { return }
        [PWin]::StripFrame($h)
        if (-not [PWin]::AtRect($h, $tx, $ty, $tw, $th)) { [PWin]::Pin($h, $tx, $ty, $tw, $th) }
        Start-Sleep -Milliseconds 120
    }
}

switch ($Mode) {
    "full" {
        $h = [PWin]::Find($titleKey)
        if ($h -eq [IntPtr]::Zero) { "no-window"; break }
        $cur = [PWin]::Rect($h)
        Set-Content -LiteralPath $stateFile -Value ("{0} {1} {2} {3}" -f $cur[0], $cur[1], $cur[2], $cur[3]) -Encoding ascii
        $sr = [PWin]::ScreenRect()
        Set-Content -LiteralPath $lockFile -Value ("{0} {1} {2} {3}" -f $sr[0], $sr[1], $sr[2], $sr[3]) -Encoding ascii
        [PWin]::StripFrame($h)
        # WSLg 的 RDP 画面在宿主窗口里左/上各内缩一段（实测 40 / 60 像素 @150%），
        # 把宿主窗口往左上顶出去同样多，画面才真正从 0,0 开始铺到屏幕边缘
        $dpi = [PWin]::Dpi()
        $sc = $dpi / 96.0
        $dx = [int][Math]::Round(26.7 * $sc)
        $dy = [int][Math]::Round(40.0 * $sc)
        $tx = $sr[0] - $dx; $ty = $sr[1] - $dy
        $tw = $sr[2] + $dx; $th = $sr[3] + $dy
        Set-Content -LiteralPath $lockFile -Value ("{0} {1} {2} {3}" -f $tx, $ty, $tw, $th) -Encoding ascii
        [PWin]::Pin($h, $tx, $ty, $tw, $th)
        Watch-Loop 900
        "rect=" + [PWin]::RectText($h) + " target=" + ($sr -join " ")
    }
    "restore" {
        if (Test-Path -LiteralPath $lockFile) { Remove-Item -LiteralPath $lockFile -Force }
        $h = [PWin]::Find($titleKey)
        if ($h -eq [IntPtr]::Zero) { "no-window"; break }
        $txt = ""
        if (Test-Path -LiteralPath $stateFile) { $txt = (Get-Content -LiteralPath $stateFile -Raw).Trim() }
        $p = $txt.Split(' ')
        if ($p.Length -eq 4) {
            [PWin]::StripFrame($h)
            [PWin]::SetWindowPos($h, [IntPtr]-2, [int]$p[0], [int]$p[1], [int]$p[2], [int]$p[3], 0x0040) | Out-Null
        }
        "rect=" + [PWin]::RectText($h)
    }
    "decor" {
        $h = [PWin]::Find($titleKey)
        if ($h -eq [IntPtr]::Zero) { "no-window"; break }
        $end = (Get-Date).AddSeconds(3)
        while ((Get-Date) -lt $end) {
            $h = [PWin]::Find($titleKey)
            if ($h -eq [IntPtr]::Zero) { break }
            [PWin]::StripFrame($h)
            Start-Sleep -Milliseconds 120
        }
        "rect=" + [PWin]::RectText($h)
    }
    "watch" {
        $t = Read-Lock
        if (-not $t) { break }
        $p = $t.Split(' ')
        if ($p.Length -ne 4) { break }
        $tx = [int]$p[0]; $ty = [int]$p[1]; $tw = [int]$p[2]; $th = [int]$p[3]
        while (Test-Path -LiteralPath $lockFile) {
            $h = [PWin]::Find($titleKey)
            if ($h -eq [IntPtr]::Zero) { break }
            [PWin]::StripFrame($h)
            if (-not [PWin]::AtRect($h, $tx, $ty, $tw, $th)) { [PWin]::Pin($h, $tx, $ty, $tw, $th) }
            Start-Sleep -Milliseconds 120
        }
        if (Test-Path -LiteralPath $lockFile) { Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue }
    }
}
