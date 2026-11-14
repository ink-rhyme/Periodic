# 从 WSL 侧调用，调整 WSLg 窗口在 Windows 上的呈现
# 用法: powershell -File winframe.ps1 decor|full|restore
param([string]$Mode = "decor")

Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public class PWin {
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc f, IntPtr l);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr h, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr MonitorFromPoint(POINT p, uint flags);
    [DllImport("user32.dll")] public static extern bool GetMonitorInfo(IntPtr mon, ref MONITORINFO mi);
    [DllImport("dwmapi.dll")] public static extern int DwmSetWindowAttribute(IntPtr h, int attr, ref int val, int size);

    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct MONITORINFO { public int cbSize; public RECT rcMonitor; public RECT rcWork; public int dwFlags; }

    public static IntPtr Find(string key) {
        IntPtr best = IntPtr.Zero; long area = -1;
        EnumWindows((h, l) => {
            var sb = new StringBuilder(512); GetWindowText(h, sb, 512);
            if (sb.ToString().Contains(key) && IsWindowVisible(h)) {
                RECT r; GetWindowRect(h, out r);
                long a = (long)(r.Right - r.Left) * (r.Bottom - r.Top);
                if (a > area) { area = a; best = h; }
            }
            return true;
        }, IntPtr.Zero);
        return best;
    }

    public static RECT ScreenRect() {
        MONITORINFO mi = new MONITORINFO(); mi.cbSize = Marshal.SizeOf(mi);
        IntPtr mon = MonitorFromPoint(new POINT { X = 0, Y = 0 }, 1);
        GetMonitorInfo(mon, ref mi);
        return mi.rcMonitor;
    }

    public static RECT WorkRect() {
        MONITORINFO mi = new MONITORINFO(); mi.cbSize = Marshal.SizeOf(mi);
        IntPtr mon = MonitorFromPoint(new POINT { X = 0, Y = 0 }, 1);
        GetMonitorInfo(mon, ref mi);
        return mi.rcWork;
    }

    public static string RectText(IntPtr h) { RECT r; GetWindowRect(h, out r); return "" + r.Left + "," + r.Top + " " + (r.Right - r.Left) + "x" + (r.Bottom - r.Top); }

    public static string Apply(string mode, string stateFile) {
        IntPtr h = Find("Periodic");
        if (h == IntPtr.Zero) return "no-window";
        int noRound = 1;       // DWMWCP_DONOTROUND
        int noNc = 1;          // DWMNCRP_DISABLED
        DwmSetWindowAttribute(h, 33, ref noRound, 4);
        DwmSetWindowAttribute(h, 2, ref noNc, 4);
        const uint SWP_SHOW = 0x0040;
        if (mode == "full") {
            RECT r; GetWindowRect(h, out r);
            System.IO.File.WriteAllText(stateFile, r.Left + " " + r.Top + " " + (r.Right - r.Left) + " " + (r.Bottom - r.Top));
            RECT s = ScreenRect();
            SetWindowPos(h, new IntPtr(-1), s.Left, s.Top, s.Right - s.Left, s.Bottom - s.Top, SWP_SHOW);
        } else if (mode == "restore") {
            string txt = System.IO.File.Exists(stateFile) ? System.IO.File.ReadAllText(stateFile) : "";
            string[] p = txt.Split(' ');
            if (p.Length == 4) {
                SetWindowPos(h, new IntPtr(-2), int.Parse(p[0]), int.Parse(p[1]), int.Parse(p[2]), int.Parse(p[3]), SWP_SHOW);
            }
        }
        return RectText(h);
    }
}
"@

$stateFile = Join-Path $env:TEMP "periodic_frame.txt"
[PWin]::Apply($Mode, $stateFile) | Write-Output
