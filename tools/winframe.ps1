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
    [DllImport("user32.dll")] public static extern IntPtr MonitorFromPoint(POINT p, uint flags);
    [DllImport("user32.dll")] public static extern bool GetMonitorInfo(IntPtr mon, ref MONITORINFO mi);
    [DllImport("user32.dll", SetLastError = true)] public static extern int GetWindowLong(IntPtr h, int idx);
    [DllImport("user32.dll", SetLastError = true)] public static extern int SetWindowLong(IntPtr h, int idx, int val);
    [DllImport("dwmapi.dll")] public static extern int DwmSetWindowAttribute(IntPtr h, int attr, ref int val, int size);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int attr, out int val, int size);

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

    // 去掉窗口的系统阴影：DWM 对带 WS_THICKFRAME/WS_CAPTION 的窗口会画阴影/圆角，
    // 把窗口样式改成无框 POPUP 并让 DWM 重新计算框架，再关掉非客户区渲染
    public static string StripFrame(IntPtr h) {
        const int GWL_STYLE = -16;
        int style = GetWindowLong(h, GWL_STYLE);
        int mask = unchecked((int)0xFF3CF7FF);   // 清掉 CAPTION/THICKFRAME/SYSMENU/BORDER/DLGFRAME/MIN/MAX
        style &= mask;
        style = unchecked((int)(style | 0x80000000));  // WS_POPUP
        SetWindowLong(h, GWL_STYLE, style);
        const uint SWP_FRAMECHANGED = 0x0020, SWP_NOMOVE = 0x0002, SWP_NOSIZE = 0x0001, SWP_NOZORDER = 0x0004;
        SetWindowPos(h, IntPtr.Zero, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

        int noRound = 1;   // DWMWCP_DONOTROUND
        DwmSetWindowAttribute(h, 33, ref noRound, 4);
        int noNc = 1;      // DWMNCRP_DISABLED
        DwmSetWindowAttribute(h, 2, ref noNc, 4);
        int noShadow = 1;  // DWMWA_NCRENDERING_POLICY 之外的补充：关掉扩展框架
        DwmSetWindowAttribute(h, 3, ref noShadow, 4);
        return "style=0x" + style.ToString("X8");
    }

    public static string RectText(IntPtr h) { RECT r; GetWindowRect(h, out r); return "" + r.Left + "," + r.Top + " " + (r.Right - r.Left) + "x" + (r.Bottom - r.Top); }

    public static string Apply(string mode, string stateFile) {
        IntPtr h = Find("Periodic");
        if (h == IntPtr.Zero) return "no-window";
        string st = StripFrame(h);
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
        int corner = -1; DwmGetWindowAttribute(h, 33, out corner, 4);
        return st + " rect=" + RectText(h) + " corner=" + corner;
    }
}
"@

$stateFile = Join-Path $env:TEMP "periodic_frame.txt"
[PWin]::Apply($Mode, $stateFile) | Write-Output
