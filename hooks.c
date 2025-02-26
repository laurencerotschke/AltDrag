/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright (C) 2015    Stefan Sundin                                   *
 * This program is free software: you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 or later.              *
 * Modified By Raymond Gillibert in 2020                                 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0400
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include <commctrl.h>
#define COBJMACROS
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "unfuck.h"

// App
#define APP_NAME L"AltDrag"

// Boring stuff
#define REHOOK_TIMER  WM_APP+1
#define INIT_TIMER    WM_APP+2

HWND g_hwnd;
static void UnhookMouse();
static int HookMouse();

// Enumerators
enum action { AC_NONE=0, AC_MOVE, AC_RESIZE, AC_MINIMIZE, AC_MAXIMIZE, AC_CENTER
            , AC_ALWAYSONTOP, AC_CLOSE, AC_LOWER, AC_ALTTAB, AC_VOLUME
		    , AC_TRANSPARENCY, AC_BORDERLESS };
enum button { BT_NONE=0, BT_LMB=0x0002, BT_MMB=0x0020, BT_RMB=0x0008, BT_MB4=0x0080, BT_MB5=0x0081 };
enum resize { RZ_NONE=0, RZ_TOP, RZ_RIGHT, RZ_BOTTOM, RZ_LEFT, RZ_CENTER };
enum cursor { HAND, SIZENWSE, SIZENESW, SIZENS, SIZEWE, SIZEALL };

// Window database
#define NUMWNDDB 32
struct wnddata {
    HWND hwnd;
    int width;
    int height;
    int restore;
};
struct {
    struct wnddata items[NUMWNDDB];
    struct wnddata *pos;
} wnddb;

RECT oldRect;
HDC hdcc;
HPEN hpenDot_Global=NULL;
HDWP hWinPosInfo;

struct windowRR {
    HWND hwnd;
    int x;
    int y;
    int width;
    int height;
} LastWin;

// State
struct {
    POINT clickpt;
    POINT prevpt;
    POINT offset;
    HWND hwnd;
    HWND mdiclient;
    struct wnddata *wndentry;
    DWORD clicktime;

    char alt;
    char activated;
    char ctrl;
    char blockaltup;

    char blockmouseup;
    char ignorectrl;
    char ignoreclick;
    char shift;

    char snap;

    struct {
        char maximized;
        HMONITOR monitor;
        int width;
        int height;
        int right;
        int bottom;
    } origin;

    enum action action;
    struct {
        enum resize x, y;
    } resize;

    struct {
        POINT Min;
        POINT Max;
    } mmi;
} state;

// Snap
RECT *monitors = NULL;
int nummonitors = 0;
RECT *wnds = NULL;
int numwnds = 0;
HWND *hwnds = NULL;
int numhwnds = 0;
HWND progman = NULL;

// Settings
#define MAXKEYS 10
struct {
    char AutoFocus;
    char AutoSnap;
    char AutoRemaximize;
    char Aero;

    char MDI;
    char InactiveScroll;
    char LowerWithMMB;
    char ResizeCenter;

    unsigned char MoveRate;
    unsigned char ResizeRate;
    unsigned char SnapThreshold;
    unsigned char AeroThreshold;

    unsigned char AVoff;
    unsigned char AHoff;
    unsigned short dbclktime;

    char FullWin;
    char ResizeAll;
    char AggressivePause;
    char AeroTopMaximizes;

    char UseCursor;
    char PearceDBClick;
    unsigned char CenterFraction;
    unsigned char RefreshRate;
    
    unsigned char MinAlpha;

    struct {
        unsigned char length;
        unsigned char keys[MAXKEYS];
    } Hotkeys;

    struct {
        enum action LMB, MMB, RMB, MB4, MB5, Scroll;
    } Mouse;
} conf ;

// Blacklist (dynamically allocated)
struct blacklistitem {
    wchar_t *title;
    wchar_t *classname;
};
struct blacklist {
    struct blacklistitem *items;
    int length;
    wchar_t *data;
};
struct {
    struct blacklist Processes;
    struct blacklist Windows;
    struct blacklist Snaplist;
    struct blacklist MDIs;
    struct blacklist Pause;
} BlkLst = { {NULL, 0, NULL}, {NULL, 0, NULL}, {NULL, 0, NULL}
           , {NULL, 0, NULL}, {NULL, 0, NULL} };

// Cursor data
HWND cursorwnd = NULL;
HCURSOR cursors[6];

// Hook data
HINSTANCE hinstDLL = NULL;
HHOOK mousehook = NULL;

/////////////////////////////////////////////////////////////////////////////
// wether a window is present or not in a blacklist
static int blacklisted(HWND hwnd, struct blacklist *list)
{
    wchar_t title[256]=L"", classname[256]=L"";
    int i;

    // Null hwnd or empty list
    if (! hwnd || !list || list->length == 0 || !list->items || !list->data)
        return 0;

    GetWindowText(hwnd, title, ARR_SZ(title));
    GetClassName(hwnd, classname, ARR_SZ(classname));
    for (i=0; i < list->length; i++) {
        if ((!list->items[i].title && !wcscmp(classname,list->items[i].classname))
         || (!list->items[i].classname && !wcscmp(title,list->items[i].title))
         || ( list->items[i].title && list->items[i].classname
            && !wcscmp(title,list->items[i].title)
            && !wcscmp(classname,list->items[i].classname))) {
            return 1;
        }
    }
    return 0;
}
static int blacklistedP(HWND hwnd, struct blacklist *list)
{
    wchar_t title[256]=L"";
    int i;

    // Null hwnd or empty list
    if (! hwnd || !list || list->length == 0 || !list->items || !list->data)
        return 0;

    // ProcessBlacklist is case-insensitive
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    GetProcessImageFileNameL(proc, title, ARR_SZ(title));
    CloseHandle(proc);
    PathStripPathL(title);
    for (i=0; i < list->length; i++) {
        if (!wcsicmp(title,list->items[i].title))
            return 1;
    }
    return 0;
}
// Limit x between l and h
static inline int CLAMP(int _l, int _x, int _h)
{
    return (_x<_l)? _l: ((_x>_h)? _h: _x);
}
// Macro to clamp width and height of windows
#define CLAMPW(width)  CLAMP(state.mmi.Min.x, width,  state.mmi.Max.x)
#define CLAMPH(height) CLAMP(state.mmi.Min.y, height, state.mmi.Max.y)
static inline int IsResizable(HWND hwnd)
{
    return (conf.ResizeAll || GetWindowLongPtr(hwnd, GWL_STYLE)&WS_THICKFRAME);
}
/////////////////////////////////////////////////////////////////////////////
static inline int IsSamePTT(POINT pt, POINT ptt)
{
    return !( pt.x > ptt.x+4 || pt.y > ptt.y+4 ||pt.x < ptt.x-4 || pt.y < ptt.y-4 );
}
/////////////////////////////////////////////////////////////////////////////
static void SendSizeMove_on(enum action action, int on)
{
    if (action == AC_MOVE || action == AC_RESIZE) {
        // Don't send WM_ENTER/EXIT SIZEMOVE if the window is iTunes
        wchar_t classname[8] = L"";
        GetClassName(state.hwnd, classname, ARR_SZ(classname));
        if (wcscmp(classname, L"iTunes")) {
            SendMessage(state.hwnd, on? WM_ENTERSIZEMOVE: WM_EXITSIZEMOVE, 0, 0);
        }
    }
}
/////////////////////////////////////////////////////////////////////////////
// Enumerate callback proc
int monitors_alloc = 0;
static BOOL CALLBACK EnumMonitorsProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    // Make sure we have enough space allocated
    if (nummonitors == monitors_alloc) {
        monitors_alloc++;
        monitors = realloc(monitors, monitors_alloc*sizeof(RECT));
    }
    // Add monitor
    monitors[nummonitors++] = *lprcMonitor;
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
int wnds_alloc = 0;
static BOOL CALLBACK EnumWindowsProc(HWND window, LPARAM lParam)
{
    // Make sure we have enough space allocated
    if (numwnds == wnds_alloc) {
        wnds_alloc += 8;
        wnds = realloc(wnds, wnds_alloc*sizeof(RECT));
    }

    // Only store window if it's visible, not minimized to taskbar,
    // not the window we are dragging and not blacklisted
    RECT wnd;
    LONG_PTR style;
    if (window != state.hwnd && window != progman
     && IsWindowVisible(window) && !IsIconic(window)
     &&( ((style=GetWindowLongPtr(window,GWL_STYLE))&WS_CAPTION) == WS_CAPTION
          || (style&WS_THICKFRAME) == WS_THICKFRAME
          || blacklisted(window,&BlkLst.Snaplist)
       )
     && GetWindowRectL(window,&wnd) != 0 ) {

        // Maximized?
        if (IsZoomed(window)) {
            // Get monitor size
            HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(MONITORINFO) };
            GetMonitorInfo(monitor, &mi);
            // Crop this window so that it does not exceed the size of the monitor
            // This is done because when maximized, windows have an extra invisible
            // border (a border that stretches onto other monitors)
            wnd.left = max(wnd.left, mi.rcMonitor.left);
            wnd.top = max(wnd.top, mi.rcMonitor.top);
            wnd.right = min(wnd.right, mi.rcMonitor.right);
            wnd.bottom = min(wnd.bottom, mi.rcMonitor.bottom);
        }

        // Return if this window is overlapped by another window
        int i;
        for (i=0; i < numwnds; i++) {
            if (wnd.left >= wnds[i].left && wnd.top >= wnds[i].top
            && wnd.right <= wnds[i].right && wnd.bottom <= wnds[i].bottom) {
                return TRUE;
            }
        }

        // Add window
        wnds[numwnds++] = wnd;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
int hwnds_alloc = 0;
static BOOL CALLBACK EnumAltTabWindows(HWND window, LPARAM lParam)
{
    // Make sure we have enough space allocated
    if (numhwnds == hwnds_alloc) {
        hwnds_alloc += 8;
        hwnds = realloc(hwnds, hwnds_alloc*sizeof(HWND));
    }

    // Only store window if it's visible, not minimized
    // to taskbar and on the same monitor as the cursor
    if (IsWindowVisible(window) && !IsIconic(window)
     && (GetWindowLongPtr(window, GWL_STYLE)&WS_CAPTION) == WS_CAPTION
     && state.origin.monitor == MonitorFromWindow(window, MONITOR_DEFAULTTONULL)
    ) {
        hwnds[numhwnds++] = window;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////
// Just used in Enum
static void EnumMdi()
{
    // Add MDIClient as the monitor
    RECT wnd;
    if (GetClientRect(state.mdiclient, &wnd) != 0) {
        monitors[nummonitors++] = wnd;
    }
    if (state.snap < 2) {
        return;
    }

    // Add all the siblings to the window
    POINT mdiclientpt = { 0, 0 };
    if (ClientToScreen(state.mdiclient, &mdiclientpt) == FALSE) {
        return;
    }
    HWND window = GetWindow(state.mdiclient, GW_CHILD);
    while (window != NULL) {
        if (window == state.hwnd) {
            window = GetWindow(window, GW_HWNDNEXT);
            continue;
        }
        if (numwnds == wnds_alloc) {
            wnds_alloc += 8;
            wnds = realloc(wnds, wnds_alloc*sizeof(RECT));
        }
        if (GetWindowRectL(window,&wnd) != 0) {
            wnds[numwnds++] =
               (RECT) { wnd.left-mdiclientpt.x, wnd.top-mdiclientpt.y
                      , wnd.right-mdiclientpt.x, wnd.bottom-mdiclientpt.y };
        }
        window = GetWindow(window, GW_HWNDNEXT);
    }
}

///////////////////////////////////////////////////////////////////////////
// Enumerate all monitors/windows/MDI depending on state.
static void Enum()
{
    nummonitors = 0;
    numwnds = 0;

    // MDI
    if (state.mdiclient) {
        EnumMdi();
        return;
    }

    // Update handle to progman
    if (!IsWindow(progman)) {
        progman = FindWindow(L"Progman", L"Program Manager");
    }

    // Enumerate monitors
    EnumDisplayMonitors(NULL, NULL, EnumMonitorsProc, 0);

    // Enumerate windows
    HWND taskbar = FindWindow(L"Shell_TrayWnd", NULL);
    RECT wnd;
    if (taskbar != NULL && GetWindowRectL(taskbar,&wnd) != 0) {
        wnds[numwnds++] = wnd;
    }
    if (state.snap >= 2) {
        EnumWindows(EnumWindowsProc, 0);
    }
}
///////////////////////////////////////////////////////////////////////////
char was_moving = 0;
static void MoveSnap(int *posx, int *posy, int wndwidth, int wndheight)
{
    static RECT borders = {0, 0, 0, 0};
    if(!was_moving) {
        Enum(); // Enumerate monitors and windows
        FixDWMRect(state.hwnd, NULL, NULL, NULL, NULL, &borders);
    }

    // thresholdx and thresholdy will shrink to make sure
    // the dragged window will snap to the closest windows
    int i, j, thresholdx, thresholdy, stuckx=0, stucky=0, stickx=0, sticky=0;
    thresholdx = thresholdy = conf.SnapThreshold;

    // Loop monitors and windows
    for (i=0, j=0; i < nummonitors || j < numwnds; ) {
        RECT snapwnd;
        int snapinside;

        // Get snapwnd
        if (i < nummonitors) {
            snapwnd = monitors[i];
            snapinside = 1;
            i++;
        } else if (j < numwnds) {
            snapwnd = wnds[j];
            snapinside = (state.snap != 2);
            j++;
        }

        // Check if posx snaps
        if ((snapwnd.top-thresholdx < *posy && *posy < snapwnd.bottom+thresholdx)
         || (*posy-thresholdx < snapwnd.top && snapwnd.top < *posy+wndheight+thresholdx)) {
            int snapinside_cond = (snapinside || *posy + wndheight - thresholdx < snapwnd.top
                                  || snapwnd.bottom < *posy + thresholdx);
            if (*posx-thresholdx < snapwnd.right && snapwnd.right < *posx+thresholdx) {
                // The left edge of the dragged window will snap to this window's right edge
                stuckx = 1;
                stickx = snapwnd.right - borders.left;
                thresholdx = snapwnd.right-*posx;
            } else if (snapinside_cond && *posx+wndwidth-thresholdx < snapwnd.right
                    && snapwnd.right < *posx+wndwidth+thresholdx) {
                // The right edge of the dragged window will snap to this window's right edge
                stuckx = 1;
                stickx = snapwnd.right + borders.right - wndwidth;
                thresholdx = snapwnd.right-(*posx+wndwidth);
            } else if (snapinside_cond && *posx-thresholdx < snapwnd.left
                    && snapwnd.left < *posx+thresholdx) {
                // The left edge of the dragged window will snap to this window's left edge
                stuckx = 1;
                stickx = snapwnd.left - borders.left;
                thresholdx = snapwnd.left-*posx;
            } else if (*posx+wndwidth-thresholdx < snapwnd.left
                    && snapwnd.left < *posx+wndwidth+thresholdx) {
                // The right edge of the dragged window will snap to this window's left edge
                stuckx = 1;
                stickx = snapwnd.left + borders.right -wndwidth;
                thresholdx = snapwnd.left-(*posx+wndwidth);
            }
        }// end if posx snaps

        // Check if posy snaps
        if ((snapwnd.left-thresholdy < *posx && *posx < snapwnd.right+thresholdy)
         || (*posx-thresholdy < snapwnd.left && snapwnd.left < *posx+wndwidth+thresholdy)) {
            int snapinside_cond = (snapinside || *posx + wndwidth - thresholdy < snapwnd.left
                                  || snapwnd.right < *posx+thresholdy);
            if (*posy-thresholdy < snapwnd.bottom && snapwnd.bottom < *posy+thresholdy) {
                // The top edge of the dragged window will snap to this window's bottom edge
                stucky = 1;
                sticky = snapwnd.bottom - borders.top;
                thresholdy = snapwnd.bottom-*posy;
            } else if (snapinside_cond && *posy+wndheight-thresholdy < snapwnd.bottom
                    && snapwnd.bottom < *posy+wndheight+thresholdy) {
                // The bottom edge of the dragged window will snap to this window's bottom edge
                stucky = 1;
                sticky = snapwnd.bottom + borders.bottom - wndheight;
                thresholdy = snapwnd.bottom-(*posy+wndheight);
            } else if (snapinside_cond && *posy-thresholdy < snapwnd.top
                    && snapwnd.top < *posy+thresholdy) {
                // The top edge of the dragged window will snap to this window's top edge
                stucky = 1;
                sticky = snapwnd.top - borders.top;
                thresholdy = snapwnd.top-*posy;
            } else if (*posy+wndheight-thresholdy < snapwnd.top
                    && snapwnd.top < *posy+wndheight+thresholdy) {
                // The bottom edge of the dragged window will snap to this window's top edge
                stucky = 1;
                sticky = snapwnd.top-wndheight + borders.bottom;
                thresholdy = snapwnd.top-(*posy+wndheight);
            }
        } // end if posy snaps
    } // end for

    // Update posx and posy
    if (stuckx) {
        *posx = stickx;
    }
    if (stucky) {
        *posy = sticky;
    }
}

///////////////////////////////////////////////////////////////////////////
static void ResizeSnap(int *posx, int *posy, int *wndwidth, int *wndheight)
{
    static RECT borders = {0, 0, 0, 0};
    if(!was_moving) {
        Enum(); // Enumerate monitors and windows
        FixDWMRect(state.hwnd, NULL, NULL, NULL, NULL, &borders);
    }

    // thresholdx and thresholdy will shrink to make sure
    // the dragged window will snap to the closest windows
    int i, j, thresholdx, thresholdy;
    int stuckleft=0, stucktop=0, stuckright=0, stuckbottom=0
      , stickleft=0, sticktop=0, stickright=0, stickbottom=0;
    thresholdx = thresholdy = conf.SnapThreshold;
    // Loop monitors and windows
    for (i=0, j=0; i < nummonitors || j < numwnds;) {
        RECT snapwnd;
        int snapinside;

        // Get snapwnd
        if (i < nummonitors) {
            snapwnd = monitors[i];
            snapinside = 1;
            i++;
        } else if (j < numwnds) {
            snapwnd = wnds[j];
            snapinside = (state.snap != 2);
            j++;
        }

        // Check if posx snaps
        if ((snapwnd.top-thresholdx < *posy && *posy < snapwnd.bottom+thresholdx)
         || (*posy-thresholdx < snapwnd.top && snapwnd.top < *posy+*wndheight+thresholdx)) {
            int snapinside_cond = (snapinside || *posy+*wndheight-thresholdx < snapwnd.top
                                  || snapwnd.bottom < *posy+thresholdx);
            if (state.resize.x == RZ_LEFT && *posx-thresholdx < snapwnd.right
             && snapwnd.right < *posx+thresholdx) {
                // The left edge of the dragged window will snap to this window's right edge
                stuckleft = 1;
                stickleft = snapwnd.right;
                thresholdx = snapwnd.right - *posx;
            } else if (snapinside_cond && state.resize.x == RZ_RIGHT
                    && *posx+*wndwidth-thresholdx < snapwnd.right
                    && snapwnd.right < *posx+*wndwidth+thresholdx) {
                // The right edge of the dragged window will snap to this window's right edge
                stuckright = 1;
                stickright = snapwnd.right;
                thresholdx = snapwnd.right - (*posx + *wndwidth);
            } else if (snapinside_cond && state.resize.x == RZ_LEFT
                    && *posx-thresholdx < snapwnd.left
                    && snapwnd.left < *posx + thresholdx) {
                // The left edge of the dragged window will snap to this window's left edge
                stuckleft = 1;
                stickleft = snapwnd.left;
                thresholdx = snapwnd.left-*posx;
            } else if (state.resize.x == RZ_RIGHT && *posx + *wndwidth - thresholdx < snapwnd.left
                    && snapwnd.left < *posx+*wndwidth+thresholdx) {
                // The right edge of the dragged window will snap to this window's left edge
                stuckright = 1;
                stickright = snapwnd.left;
                thresholdx = snapwnd.left - (*posx + *wndwidth);
            }
        }

        // Check if posy snaps
        if ((snapwnd.left-thresholdy < *posx && *posx < snapwnd.right+thresholdy)
         || (*posx-thresholdy < snapwnd.left && snapwnd.left < *posx+*wndwidth+thresholdy)) {
            int snapinside_cond = (snapinside || *posx+*wndwidth-thresholdy < snapwnd.left
                                || snapwnd.right < *posx+thresholdy);
            if (state.resize.y == RZ_TOP && *posy-thresholdy < snapwnd.bottom
             && snapwnd.bottom < *posy+thresholdy) {
                // The top edge of the dragged window will snap to this window's bottom edge
                stucktop = 1;
                sticktop = snapwnd.bottom;
                thresholdy = snapwnd.bottom-*posy;
            } else if (snapinside_cond && state.resize.y == RZ_BOTTOM
                     && *posy+*wndheight-thresholdy < snapwnd.bottom
                     && snapwnd.bottom < *posy+*wndheight+thresholdy) {
                // The bottom edge of the dragged window will snap to this window's bottom edge
                stuckbottom = 1;
                stickbottom = snapwnd.bottom;
                thresholdy = snapwnd.bottom-(*posy+*wndheight);
            } else if (snapinside_cond && state.resize.y == RZ_TOP
                    && *posy-thresholdy < snapwnd.top && snapwnd.top < *posy+thresholdy) {
                // The top edge of the dragged window will snap to this window's top edge
                stucktop = 1;
                sticktop = snapwnd.top;
                thresholdy = snapwnd.top-*posy;
            } else if (state.resize.y == RZ_BOTTOM && *posy+*wndheight-thresholdy < snapwnd.top
                    && snapwnd.top < *posy+*wndheight+thresholdy) {
                // The bottom edge of the dragged window will snap to this window's top edge
                stuckbottom = 1;
                stickbottom = snapwnd.top;
                thresholdy = snapwnd.top-(*posy+*wndheight);
            }
        }
    } // end for

    // Update posx, posy, wndwidth and wndheight
    if (stuckleft) {
        *wndwidth = *wndwidth+*posx-stickleft + borders.left;
        *posx = stickleft - borders.left;
    }
    if (stucktop) {
        *wndheight = *wndheight+*posy-sticktop + borders.top;
        *posy = sticktop - borders.top;
    }
    if (stuckright) {
        *wndwidth = stickright-*posx + borders.right;
    }
    if (stuckbottom) {
        *wndheight = stickbottom-*posy + borders.bottom;
    }
}
// Call with SW_MAXIMIZE or SW_RESTORE or below.
#define SW_TOGGLE_MAX_RESTORE 27
static void Maximize_Restore_atpt(HWND hwnd, POINT *pt, UINT sw_cmd, HMONITOR monitor)
{
    WINDOWPLACEMENT wndpl = { sizeof(WINDOWPLACEMENT) };
    GetWindowPlacement(hwnd, &wndpl);
    if(sw_cmd == SW_TOGGLE_MAX_RESTORE)
        wndpl.showCmd = (wndpl.showCmd==SW_MAXIMIZE)? SW_RESTORE: SW_MAXIMIZE;
    else
        wndpl.showCmd = sw_cmd;

    if(sw_cmd == SW_MAXIMIZE) {
        if(!monitor) monitor = MonitorFromPoint(*pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        GetMonitorInfo(monitor, &mi);
        RECT mon = mi.rcWork;

        // Center window on monitor, if needed
        HMONITOR wndmonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (monitor != wndmonitor) {
            int width  = wndpl.rcNormalPosition.right  - wndpl.rcNormalPosition.left;
            int height = wndpl.rcNormalPosition.bottom - wndpl.rcNormalPosition.top;
            wndpl.rcNormalPosition.left = mon.left + (mon.right-mon.left)/2-width/2;
            wndpl.rcNormalPosition.top  = mon.top  + (mon.bottom-mon.top)/2-height/2;
            wndpl.rcNormalPosition.right  = wndpl.rcNormalPosition.left + width;
            wndpl.rcNormalPosition.bottom = wndpl.rcNormalPosition.top  + height;
        }
    }
    SetWindowPlacement(hwnd, &wndpl);
}
/////////////////////////////////////////////////////////////////////////////
// Move the windows in a thread in case it is very slow to resize
static DWORD WINAPI EndMoveWindowThread(LPVOID LastWinV)
{
    int ret;
    struct windowRR *lw = LastWinV;

    if(!lw->hwnd || !IsWindow(lw->hwnd)) {return 1;}

    ret = MoveWindow(lw->hwnd, lw->x, lw->y, lw->width, lw->height, TRUE);
    if(!conf.FullWin)
        RedrawWindow(lw->hwnd, NULL, NULL, RDW_ERASE|RDW_FRAME|RDW_INVALIDATE|RDW_ALLCHILDREN);

    lw->hwnd = NULL;

    return !ret;
}
static DWORD WINAPI MoveWindowThread(LPVOID LastWinV)
{
    int ret;
    struct windowRR *lw = LastWinV;

    ret = MoveWindow(lw->hwnd, lw->x, lw->y, lw->width, lw->height, TRUE);
    if(conf.RefreshRate) Sleep(conf.RefreshRate);

    lw->hwnd = NULL;

    return !ret;
}
///////////////////////////////////////////////////////////////////////////
#define MM_THREAD_ON (LastWin.hwnd && conf.FullWin)
#define AERO_TH conf.AeroThreshold
static int AeroMoveSnap(POINT pt, int *posx, int *posy
                      , int *wndwidth, int *wndheight, RECT mon)
{
    // Aero Snap
    static int resizable=1;
    // return if last resizing is not finished or no Aero or not resizable.
    if (!conf.Aero || MM_THREAD_ON || !resizable) return 0;
    if (!was_moving && !(resizable=IsResizable(state.hwnd)) ) return 0;

    int Left  = mon.left   + 2*AERO_TH ;
    int Right = mon.right  - 2*AERO_TH ;
    int Top   = mon.top    + 2*AERO_TH ;
    int Bottom= mon.bottom - 2*AERO_TH ;

    // Move window
    if (pt.x < Left && pt.y < Top) {
        // Top left
        state.wndentry->restore = 1;
        *wndwidth =  ((mon.right-mon.left)*conf.AHoff)/100;
        *wndheight = ((mon.bottom-mon.top)*conf.AVoff)/100;
        *posx = mon.left;
        *posy = mon.top;
    } else if (Right < pt.x && pt.y < Top) {
        // Top right
        state.wndentry->restore = 1;
        *wndwidth = CLAMPW( (mon.right-mon.left)*(100-conf.AHoff)/100 );
        *wndheight = (mon.bottom-mon.top)*conf.AVoff/100;
        *posx = mon.right-*wndwidth;
        *posy = mon.top;
    } else if (pt.x < Left && Bottom < pt.y) {
        // Bottom left
        state.wndentry->restore = 1;
        *wndwidth = (mon.right-mon.left)*conf.AHoff/100;
        *wndheight = CLAMPH( (mon.bottom-mon.top)*(100-conf.AVoff)/100 );
        *posx = mon.left;
        *posy = mon.bottom - *wndheight;
    } else if (Right < pt.x && Bottom < pt.y) {
        // Bottom right
        state.wndentry->restore = 1;
        *wndwidth = CLAMPW( (mon.right-mon.left)*(100-conf.AHoff)/100 );
        *wndheight= CLAMPH( (mon.bottom-mon.top)*(100-conf.AVoff)/100 );
        *posx = mon.right - *wndwidth;
        *posy = mon.bottom - *wndheight;
    } else if (pt.y < mon.top + AERO_TH) {
        // Top
        if (state.shift ^ conf.AeroTopMaximizes) {
            Maximize_Restore_atpt(state.hwnd, &pt, SW_MAXIMIZE, NULL);
            LastWin.hwnd=NULL;
            was_moving = 2;
            return 1;
        } else {
            state.wndentry->restore = 1;
            *wndwidth = CLAMPW(mon.right - mon.left);
            *wndheight = (mon.bottom-mon.top)*conf.AVoff/100;
            *posx = mon.left + (mon.right-mon.left)/2 - *wndwidth/2; // Center
            *posy = mon.top;
        }
    } else if (mon.bottom-AERO_TH < pt.y) {
        // Bottom
        state.wndentry->restore = 1;
        *wndwidth  = CLAMPW( mon.right-mon.left);
        *wndheight = CLAMPH( (mon.bottom-mon.top)*(100-conf.AVoff)/100 );
        *posx = mon.left + (mon.right-mon.left)/2 - *wndwidth/2; // Center
        *posy = mon.bottom - *wndheight;
    } else if (pt.x < mon.left+AERO_TH) {
        // Left
        state.wndentry->restore = 1;
        *wndwidth = (mon.right-mon.left)*conf.AHoff/100;
        *wndheight = CLAMPH( mon.bottom-mon.top );
        *posx = mon.left;
        *posy = mon.top + (mon.bottom-mon.top)/2 - *wndheight/2; // Center
    } else if (mon.right - AERO_TH < pt.x) {
        // Right
        state.wndentry->restore = 1;
        *wndwidth =  CLAMPW( (mon.right-mon.left)*(100-conf.AHoff)/100 );
        *wndheight = CLAMPH( (mon.bottom-mon.top) );
        *posx = mon.right - *wndwidth;
        *posy = mon.top + (mon.bottom-mon.top)/2 - *wndheight/2; // Center
    } else if (state.wndentry->restore) {
        // Restore original window size
        state.wndentry->restore = 0;
        *wndwidth = state.origin.width;
        *wndheight = state.origin.height;
    }

    // Aero-move the window?
    if (state.wndentry->restore) {
        *wndwidth  = CLAMPW(*wndwidth);
        *wndheight = CLAMPH(*wndheight);

        state.wndentry->width = state.origin.width;
        state.wndentry->height = state.origin.height;

        FixDWMRect(state.hwnd, posx, posy, wndwidth, wndheight, NULL);
        if(conf.FullWin) {
            MoveWindow(state.hwnd, *posx, *posy, *wndwidth, *wndheight, TRUE);
            return 1;
        }
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////////
static RECT GetMonitorRect(POINT pt)
{
    static RECT mon;
    static int single_mon = -1;
    switch(single_mon) {
    case -1:
        single_mon = (GetSystemMetrics(SM_CMONITORS) <= 1);
    case 0: /* If we have several monitors... */
        ;MONITORINFO mi = { sizeof(MONITORINFO) };
        GetMonitorInfo(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
        mon = mi.rcWork;
    }
    return mon;
}
///////////////////////////////////////////////////////////////////////////
static int AeroResizeSnap(POINT pt, int *posx, int *posy
                         , int *wndwidth, int *wndheight, RECT *mon_)
{
    // return if last resizing is not finished or no Aero
    if(!conf.Aero || MM_THREAD_ON) return 0;

    static RECT borders = {0, 0, 0, 0};
    static RECT mon;
    if(!was_moving) {
        if(!mon_) mon = GetMonitorRect(pt);
        else mon = *mon_;
        FixDWMRect(state.hwnd, NULL, NULL, NULL, NULL, &borders);
    }
    if (state.resize.x == RZ_CENTER && state.resize.y == RZ_TOP && pt.y < mon.top + AERO_TH) {
        state.wndentry->restore = 1;
        *wndheight = CLAMPH(mon.bottom - mon.top + borders.bottom + borders.top);
        *posy = mon.top - borders.top;
    } else if (state.resize.x == RZ_LEFT && state.resize.y == RZ_CENTER && pt.x < mon.left + AERO_TH) {
        state.wndentry->restore = 1;
        *wndwidth = CLAMPW(mon.right - mon.left + borders.left + borders.right);
        *posx = mon.left - borders.left;
    }

    // Aero-move the window?
    if (state.wndentry->restore) {
        state.wndentry->width = state.origin.width;
        state.wndentry->height = state.origin.height;
        if(conf.FullWin) {
            MoveWindow(state.hwnd, *posx, *posy, *wndwidth, *wndheight, TRUE);
            return 1;
        }
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////////
// Get action of button
static enum action GetAction(int button)
{
    if      (button == BT_LMB) return conf.Mouse.LMB;
    else if (button == BT_MMB) return conf.Mouse.MMB;
    else if (button == BT_RMB) return conf.Mouse.RMB;
    else if (button == BT_MB4) return conf.Mouse.MB4;
    else if (button == BT_MB5) return conf.Mouse.MB5;
    else return AC_NONE;
}

///////////////////////////////////////////////////////////////////////////
// Check if key is assigned
static int IsHotkey(int key)
{
    int i;
    for (i=0; i < conf.Hotkeys.length; i++) {
        if (key == conf.Hotkeys.keys[i]) {
            return 1;
        }
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////////////
//// This is used to detect is the window was snapped normally outside of
//// AltDrag, in this case if windows was in the database, we restore it.
//static int IsWindowSnapped(HWND hwnd)
//{
//    RECT rect, mon;
//    if(!GetWindowRectL(state.hwnd, &rect)) return 0;
//    
//    MONITORINFO mi = { sizeof(MONITORINFO) };
//    GetMonitorInfo(state.origin.monitor, &mi);
//    mon = mi.rcWork;
//
//    int W = rect.right  - rect.left;
//    int H = rect.bottom - rect.top;
//    int SW = mon.right  - mon.left;
//    int SH = mon.bottom - mon.top;
//
//    if (W == SW/2) {
//        if(H == SH){
//            if (rect.top == 0 && (rect.left == 0 || rect.right == SW))
//                return 1;
//        } else if (H == SH/2) {
//            if (rect.top == 0 || rect.bottom == SH)
//                return 1;
//        }
//    }
//    return 0;
//}
/////////////////////////////////////////////////////////////////////////////
static void RestoreOldWin(POINT pt)
{
    // Restore old width/height?
    int restore = 0;
    if (state.wndentry->restore 
//     || (IsWindowSnapped(state.hwnd) && state.wndentry->width && state.wndentry->height)
     ) {
        restore = 1;
        state.origin.width = state.wndentry->width;
        state.origin.height = state.wndentry->height;
    }
    state.wndentry->restore = 0;

    POINT mdiclientpt = { 0, 0 };
    RECT wnd;
    GetWindowRect(state.hwnd, &wnd);
    ClientToScreen(state.mdiclient, &mdiclientpt);

    // Set offset
    state.offset.x = (state.origin.width  * (pt.x-wnd.left))/(wnd.right-wnd.left);
    state.offset.y = (state.origin.height * (pt.y-wnd.top)) /(wnd.bottom-wnd.top);

    if (state.origin.maximized) {
        // NOTHING
    } else if (restore) {
        MoveWindow(state.hwnd, pt.x - state.offset.x - mdiclientpt.x
                             , pt.y - state.offset.y - mdiclientpt.y
                             , state.origin.width, state.origin.height, TRUE);
    } else {
        state.offset.x = pt.x-wnd.left;
        state.offset.y = pt.y-wnd.top;
    }
}
///////////////////////////////////////////////////////////////////////////
static void MouseMove(POINT pt)
{
    int posx, posy, wndwidth, wndheight;

    // Check if window still exists
    if (!state.hwnd || !IsWindow(state.hwnd))
        { LastWin.hwnd = NULL; UnhookMouse(); return; }

    // Restore Aero snapped window
    if(state.action == AC_MOVE && !was_moving) RestoreOldWin(pt);

    static RECT wnd;
    if (!was_moving && !GetWindowRect(state.hwnd, &wnd)) return;

    RECT mdimon;
    POINT mdiclientpt = { 0, 0 };
    if (state.mdiclient) { // MDI
        if (!GetClientRect(state.mdiclient, &mdimon)
         || !ClientToScreen(state.mdiclient, &mdiclientpt)) {
            return;
        }
        pt.x -= mdiclientpt.x;
        pt.y -= mdiclientpt.y;
    }

    // Restrict pt within origin monitor if Ctrl is being pressed
    if ((GetAsyncKeyState(VK_CONTROL)&0x8000) && !state.ignorectrl) {
        static HMONITOR origMonitor;
        static RECT fmon;
        if(origMonitor != state.origin.monitor) {
            origMonitor = state.origin.monitor;
            MONITORINFO mi = { sizeof(MONITORINFO) };
            GetMonitorInfo(state.origin.monitor, &mi);
            fmon = mi.rcMonitor;
            fmon.left++; fmon.top++;
            fmon.right--; fmon.bottom--;
        }
        RECT clip = !state.mdiclient?fmon: (RECT)
                  { mdiclientpt.x, mdiclientpt.y
                  , mdiclientpt.x + mdimon.right, mdiclientpt.y + mdimon.bottom};
        ClipCursor(&clip);
        pt.x = CLAMP(fmon.left, pt.x, fmon.right);
        pt.y = CLAMP(fmon.top, pt.y, fmon.bottom);
    }

    // Get new position for window
    if (state.action == AC_MOVE) {
        posx = pt.x-state.offset.x;
        posy = pt.y-state.offset.y;
        wndwidth = wnd.right-wnd.left;
        wndheight = wnd.bottom-wnd.top;

        // Check if the window will snap anywhere
        if (state.snap) MoveSnap(&posx, &posy, wndwidth, wndheight);
        int ret = AeroMoveSnap(pt, &posx, &posy, &wndwidth, &wndheight
                       , state.mdiclient? mdimon: GetMonitorRect(pt));
        if ( ret == 1) return;

        // Restore window if maximized
        if (IsZoomed(state.hwnd)) {
            WINDOWPLACEMENT wndpl = { sizeof(WINDOWPLACEMENT) };
            GetWindowPlacement(state.hwnd, &wndpl);
            wndpl.showCmd = SW_RESTORE;
            SetWindowPlacement(state.hwnd, &wndpl);
            // Update wndwidth and wndheight
            wndwidth = wndpl.rcNormalPosition.right-wndpl.rcNormalPosition.left;
            wndheight = wndpl.rcNormalPosition.bottom-wndpl.rcNormalPosition.top;
        }
    } else if (state.action == AC_RESIZE) {
        // Restore the window (to monitor size) if it's maximized
        if (!was_moving && IsZoomed(state.hwnd)) {
            WINDOWPLACEMENT wndpl = { sizeof(WINDOWPLACEMENT) };
            GetWindowPlacement(state.hwnd, &wndpl);

            // Set size to monitor to prevent flickering
            wnd = wndpl.rcNormalPosition = state.mdiclient? mdimon: GetMonitorRect(pt);

            if (state.mdiclient) {
                // Make it a little smaller since MDIClients by
                // default have scrollbars that would otherwise appear
                wndpl.rcNormalPosition.right -= 8;
                wndpl.rcNormalPosition.bottom -= 8;
            }
            wndpl.showCmd = SW_RESTORE;
            SetWindowPlacement(state.hwnd, &wndpl);
            if (state.mdiclient) {
                // Get new values from MDIClient, since restoring the child have changed them,
                // The amount they change with differ depending on implementation (compare mIRC and Spy++)
                Sleep(1); // Sometimes needed
                mdiclientpt = (POINT) { 0, 0 };
                if (!GetClientRect(state.mdiclient, &wnd)
                 || !ClientToScreen(state.mdiclient, &mdiclientpt) ) {
                     return;
                }
                state.origin.right=wnd.right; state.origin.bottom=wnd.bottom;
            }
            // Fix wnd for MDI offset and invisible borders
            RECT borders;
            FixDWMRect(state.hwnd, NULL, NULL, NULL, NULL, &borders);
            wnd = (RECT) { wnd.left  + mdiclientpt.x - borders.left
                         , wnd.top   + mdiclientpt.y - borders.top
                         , wnd.right + mdiclientpt.x + borders.right
                         , wnd.bottom+ mdiclientpt.y + borders.bottom };

        }
        // Clear restore flag
        state.wndentry->restore = 0;

        // Figure out new placement
        if (state.resize.x == RZ_CENTER && state.resize.y == RZ_CENTER) {
            posx = wnd.left - (pt.x-state.offset.x) - mdiclientpt.x;
            posy = wnd.top - (pt.y-state.offset.y) - mdiclientpt.y;
            wndwidth = wnd.right-wnd.left+2*(pt.x-state.offset.x);
            wndheight = wnd.bottom-wnd.top+2*(pt.y-state.offset.y);
            state.offset.x = pt.x;
            state.offset.y = pt.y;
        } else {
            if (state.resize.y == RZ_TOP) {
                wndheight = CLAMPH( (wnd.bottom-pt.y+state.offset.y) - mdiclientpt.y );
                posy = state.origin.bottom - wndheight;
            } else if (state.resize.y == RZ_CENTER) {
                posy = wnd.top - mdiclientpt.y;
                wndheight = wnd.bottom - wnd.top;
            } else if (state.resize.y == RZ_BOTTOM) {
                posy = wnd.top - mdiclientpt.y;
                wndheight = pt.y - posy + state.offset.y;
            }
            if (state.resize.x == RZ_LEFT) {
                wndwidth = CLAMPW( (wnd.right-pt.x+state.offset.x) - mdiclientpt.x );
                posx = state.origin.right - wndwidth;
            } else if (state.resize.x == RZ_CENTER) {
                posx = wnd.left - mdiclientpt.x;
                wndwidth = wnd.right - wnd.left;
            } else if (state.resize.x == RZ_RIGHT) {
                posx = wnd.left - mdiclientpt.x;
                wndwidth = pt.x - posx+state.offset.x;
            }
            wndwidth =CLAMPW(wndwidth);
            wndheight=CLAMPH(wndheight);

            // Check if the window will snap anywhere
            if (state.snap) {
                ResizeSnap(&posx, &posy, &wndwidth, &wndheight);
            }
            if(AeroResizeSnap(pt, &posx, &posy, &wndwidth
                  , &wndheight, state.mdiclient? &mdimon: NULL))
                return;
        }
    }

    MoveWindow(cursorwnd, pt.x-127, pt.y-127, 256, 256, FALSE);
    // LastWin is GLOBAL !
    int mouse_thread_finished = !LastWin.hwnd;
    LastWin.hwnd   = state.hwnd;
    LastWin.x      = posx;
    LastWin.y      = posy;
    LastWin.width  = wndwidth;
    LastWin.height = wndheight;

    wnd.left   = posx + mdiclientpt.x;
    wnd.top    = posy + mdiclientpt.y;
    wnd.right  = posx + mdiclientpt.x + wndwidth;
    wnd.bottom = posy + mdiclientpt.y + wndheight;

    if (!conf.FullWin) {
        RECT newRect;
        newRect.left = wnd.left + 1;
        newRect.top  = wnd.top + 1;
        newRect.right = wnd.right - 1;
        newRect.bottom= wnd.bottom - 1;
        if (!hdcc) {
            if (!hpenDot_Global)
                hpenDot_Global = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            hdcc = CreateDCA("DISPLAY", NULL, NULL, NULL);
            SetROP2(hdcc, R2_NOTXORPEN);
            SelectObject(hdcc, hpenDot_Global);
        }
        Rectangle(hdcc, newRect.left, newRect.top, newRect.right, newRect.bottom);
        if (was_moving == 1)
            Rectangle(hdcc, oldRect.left, oldRect.top, oldRect.right, oldRect.bottom);

        oldRect=newRect; // oldRect is GLOBAL!

    } else if (mouse_thread_finished) {
        DWORD lpThreadId;
        HANDLE thread;
        thread = CreateThread(NULL, 0, MoveWindowThread, &LastWin, 0, &lpThreadId);
        CloseHandle(thread);
    } else {
        Sleep(0);
    }
    was_moving = 1;
}
/////////////////////////////////////////////////////////////////////////////
static void Send_CTRL()
{
    state.ignorectrl = 1;
    KEYBDINPUT ctrl[2] = { {VK_CONTROL, 0, 0, 0, 0}, {VK_CONTROL, 0 , KEYEVENTF_KEYUP, 0, 0} };
    ctrl[0].dwExtraInfo = ctrl[1].dwExtraInfo = GetMessageExtraInfo();
    INPUT input[2] = { {INPUT_KEYBOARD,{.ki = ctrl[0]}}, {INPUT_KEYBOARD,{.ki = ctrl[1]}} };
    SendInput(2, input, sizeof(INPUT));
    state.ignorectrl = 0;
}
/////////////////////////////////////////////////////////////////////////////
static void Send_DoubleClick(DWORD button)
{
    state.ignoreclick = 1;
    if (!button) return;
    DWORD MouseEvent = button&(~0x0001); // Remove the LSB
    // In case of XButton (0x0080) set mouse data to 1 or 2
    DWORD mdata = 0;
    if(button&0x0080) mdata = 0x0001 + (button&0x0001);
    MOUSEINPUT click[2] = { {0, 0, mdata, MouseEvent, 0, 0}
                          , {0, 0, mdata, MouseEvent, 0, 0} };
    click[0].dwExtraInfo = click[1].dwExtraInfo = GetMessageExtraInfo();
    INPUT input[2] = { {INPUT_MOUSE, {.mi = click[0]}}, {INPUT_MOUSE, {.mi = click[1]}} };

    SendInput(2, input, sizeof(INPUT));
    Sleep(1);
    click[0].dwExtraInfo = click[1].dwExtraInfo = GetMessageExtraInfo();
    SendInput(2, input, sizeof(INPUT));
    Sleep(0);
    state.ignoreclick = 0;
}

///////////////////////////////////////////////////////////////////////////
// Keep this one minimalist, it is always on.
__declspec(dllexport) LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode != HC_ACTION) return CallNextHookEx(NULL, nCode, wParam, lParam);

    int vkey = ((PKBDLLHOOKSTRUCT)lParam)->vkCode;

    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        if (IsHotkey(vkey)) {
            // Block this keypress if an we're already pressing a hotkey or an action is happening
            if (state.activated && !state.alt && state.action) {
                state.alt = 1;
            }
            if (state.activated && state.alt) {
                return 1;
            }

            // Update state
            state.alt = 1;
            state.blockaltup = 0;
            state.blockmouseup = 0;

            // Ctrl as hotkey should not trigger Ctrl-focusing when starting dragging,
	        // releasing and pressing it again will focus though
            if (!state.action && (vkey == VK_LCONTROL || vkey == VK_RCONTROL)) {
                state.ignorectrl = 1;
            }

            // Hook mouse
            HookMouse();
        } else if (vkey == VK_LSHIFT || vkey == VK_RSHIFT) {
            state.snap = 3;
            state.shift = 1;

            // Block keydown to prevent Windows from changing keyboard layout
            if (state.alt && state.action) {
                return 1;
            }
        } else if (vkey == VK_SPACE && state.action && state.snap) {
            state.snap = 0;
            return 1;
        } else if (vkey == VK_ESCAPE && state.action != AC_NONE) {
            if (!conf.FullWin && was_moving) {
                Rectangle(hdcc, oldRect.left, oldRect.top, oldRect.right, oldRect.bottom);
            }
            // Send WM_EXITSIZEMOVE
            SendSizeMove_on(state.action , 0);

            was_moving = 0;
            state.action = AC_NONE;
            LastWin.hwnd = NULL;

            UnhookMouse();

            return 1;
        } else if (conf.AggressivePause && vkey == VK_PAUSE && state.alt) {
            POINT pt;
            DWORD pid;
            GetCursorPos(&pt);
            HWND hwnd = WindowFromPoint(pt);
            if (!blacklistedP(hwnd, &BlkLst.Pause)) {
                GetWindowThreadProcessId(hwnd, &pid);
                HANDLE ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
                if(ProcessHandle){
                    if(state.shift) NtSuspendProcess(ProcessHandle);
                    else            NtResumeProcess(ProcessHandle);
                }
                CloseHandle(ProcessHandle);
                return 1;
            }
        }
        if ( (state.action == AC_MOVE || state.action == AC_RESIZE)
          && (vkey == VK_LCONTROL || vkey == VK_RCONTROL)
          && !state.ignorectrl && !state.ctrl) {
            POINT pt;
            GetCursorPos(&pt);
            state.origin.maximized = 0;
            state.origin.monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            SetForegroundWindow(state.hwnd);
            MouseMove(pt);
            state.ctrl = 1;
        }

    } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        if (vkey == VK_LCONTROL || vkey == VK_RCONTROL) {
            state.ctrl = 0;
            ClipCursor(NULL); // Release cursor trapping
        }
        if (IsHotkey(vkey)) {
            // Prevent the alt keyup from triggering the window menu to be selected
            // The way this works is that the alt key is "disguised" by sending ctrl keydown/keyup events
            if (state.blockaltup || state.action) {
                Send_CTRL();
            }

            // Hotkeys have been released
            state.alt = 0;
            state.ignorectrl = 0;

            // Unhook mouse if not moving or resizing
            if (!state.action) {
                UnhookMouse();
            }
        } else if (vkey == VK_LSHIFT || vkey == VK_RSHIFT) {
            state.shift = 0;
            state.snap = conf.AutoSnap;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
/////////////////////////////////////////////////////////////////////////////
static HWND GetClass_HideIfTooltip(POINT pt, HWND hwnd, wchar_t *classname, size_t classlen)
{
    GetClassName(hwnd, classname, classlen);

    if (!wcscmp(classname, TOOLTIPS_CLASS)) {
        ShowWindowAsync(hwnd, SW_HIDE);
        hwnd = WindowFromPoint(pt);
        if (!hwnd) return NULL;

        GetClassName(hwnd, classname, classlen);
    }
    return hwnd;
}
/////////////////////////////////////////////////////////////////////////////
static int ScrollPointedWindow(POINT pt, DWORD mouseData, WPARAM wParam)
{
    // Get window and foreground window
    HWND hwnd = WindowFromPoint(pt);
    HWND foreground = GetForegroundWindow();

    // Return if no window or if foreground window is blacklisted
    if (!hwnd || ( foreground && (
        blacklisted(foreground, &BlkLst.Windows)
      ||blacklistedP(foreground, &BlkLst.Processes) ) )
      ) {
        return 0;
    }
    // Get class behind eventual tooltip
    wchar_t classname[256] = L"";
    hwnd = GetClass_HideIfTooltip(pt, hwnd, classname, ARR_SZ(classname));

    // If it's a groupbox, grab the real window
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if ((style&BS_GROUPBOX) && !wcscmp(classname, L"Button")) {
        HWND groupbox = hwnd;
        EnableWindow(groupbox, FALSE);
        hwnd = WindowFromPoint(pt);
        EnableWindow(groupbox, TRUE);
        if (!hwnd) return 0;
    } else if (!wcscmp(classname, L"Windows.UI.Core.CoreWindow")) {
        return 0;
    }

    // Get wheel info
    WPARAM wp = GET_WHEEL_DELTA_WPARAM(mouseData) << 16;
    LPARAM lp = (pt.y << 16) | (pt.x & 0xFFFF);

    // Change WM_MOUSEWHEEL to WM_MOUSEHWHEEL if shift is being depressed
    // Introduced in Vista and far from all programs have implemented it.
    if (wParam == WM_MOUSEWHEEL && state.shift) {
        wParam = WM_MOUSEHWHEEL;
        wp = (-GET_WHEEL_DELTA_WPARAM(mouseData)) << 16; // Up is left, down is right
    }

    // Add button information since we don't get it with the hook
    if (GetAsyncKeyState(VK_CONTROL) &0x8000) wp |= MK_CONTROL;
    if (GetAsyncKeyState(VK_LBUTTON) &0x8000) wp |= MK_LBUTTON;
    if (GetAsyncKeyState(VK_MBUTTON) &0x8000) wp |= MK_MBUTTON;
    if (GetAsyncKeyState(VK_RBUTTON) &0x8000) wp |= MK_RBUTTON;
    if (GetAsyncKeyState(VK_SHIFT)   &0x8000) wp |= MK_SHIFT;
    if (GetAsyncKeyState(VK_XBUTTON1)&0x8000) wp |= MK_XBUTTON1;
    if (GetAsyncKeyState(VK_XBUTTON2)&0x8000) wp |= MK_XBUTTON2;

    // Forward scroll message
    if(PostMessage(hwnd, wParam, wp, lp))
        return 1;
    else
        return 0;
}
/////////////////////////////////////////////////////////////////////////////
static int ActionAltTab(POINT pt, int delta)
{
    numhwnds = 0;
    HWND hwnd = WindowFromPoint(pt);
    HWND root = GetAncestor(hwnd, GA_ROOT);

    if (conf.MDI && !blacklisted(root, &BlkLst.MDIs)) {

        // Get Class and Hide if tooltip
        wchar_t classname[100] = L"";
        hwnd = GetClass_HideIfTooltip(pt, hwnd, classname, ARR_SZ(classname));

        if (hwnd) {
            // Get MDIClient
            HWND mdiclient = NULL;
            if (!wcscmp(classname, L"MDIClient")) {
                mdiclient = hwnd;
            } else {
                while (hwnd != NULL) {
                    HWND parent = GetParent(hwnd);
                    LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                    if (exstyle&WS_EX_MDICHILD) {
                        mdiclient = parent;
                        break;
                    }
                    hwnd = parent;
                }
            }
            // Enumerate and then reorder MDI windows
            if (mdiclient != NULL) {
                hwnd = GetWindow(mdiclient, GW_CHILD);
                while (hwnd != NULL) {
                    if (numhwnds == hwnds_alloc) {
                        hwnds_alloc += 8;
                        hwnds = realloc(hwnds, hwnds_alloc*sizeof(HWND));
                    }
                    hwnds[numhwnds++] = hwnd;
                    hwnd = GetWindow(hwnd, GW_HWNDNEXT);
                }
                if (numhwnds > 1) {
                    if (delta > 0) {
                        PostMessage(mdiclient, WM_MDIACTIVATE, (WPARAM) hwnds[numhwnds-1], 0);
                    } else {
                        SetWindowPos(hwnds[0], hwnds[numhwnds-1], 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
                        PostMessage(mdiclient, WM_MDIACTIVATE, (WPARAM) hwnds[1], 0);
                    }
                }
            }
        }
    }// End if MDI

    // Enumerate windows
    if (numhwnds <= 1) {
        state.origin.monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        numhwnds = 0;
        EnumWindows(EnumAltTabWindows, 0);
        if (numhwnds <= 1) {
            return 0;
        }

        // Reorder windows
        if (delta > 0) {
            SetForegroundWindow(hwnds[numhwnds-1]);
        } else {
            SetWindowPos(hwnds[0], hwnds[numhwnds-1], 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
            SetForegroundWindow(hwnds[1]);
        }
    }
    return -1;
}
/////////////////////////////////////////////////////////////////////////////
// Under Vista this will change the main volume with ole interface,
// Under NT4-XP, this will change the waveOut volume.
static int ActionVolume(int delta)
{
    static int HaveV=-1;
    static HINSTANCE hOLE32DLL=NULL, hMMdll=NULL;
    if(HaveV == -1){
        hOLE32DLL = LoadLibraryA("OLE32.DLL");
        if(hOLE32DLL){
            myCoInitialize = (void *)GetProcAddress(hOLE32DLL, "CoInitialize");
            myCoUninitialize= (void *)GetProcAddress(hOLE32DLL, "CoUninitialize");
            myCoCreateInstance= (void *)GetProcAddress(hOLE32DLL, "CoCreateInstance");
            if(!myCoCreateInstance || !myCoUninitialize || !myCoInitialize){
                FreeLibrary(hOLE32DLL);
                HaveV = 0;
            } else {
                HaveV = 1;
            }
        } else {
            HaveV = 0;
        }
    }
    if (HaveV == 1) {
        HRESULT hr;
        IMMDeviceEnumerator *pDevEnumerator = NULL;
        IMMDevice *pDev = NULL;
        IAudioEndpointVolume *pAudioEndpoint = NULL;

        // Get audio endpoint
        myCoInitialize(NULL); // Needed for IAudioEndpointVolume
        hr = myCoCreateInstance(&my_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL
                              , &my_IID_IMMDeviceEnumerator, (void**)&pDevEnumerator);
        if (hr != S_OK){
            myCoUninitialize();
            FreeLibrary(hOLE32DLL);
            HaveV = 2;
            return 0;
        }

        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pDevEnumerator, eRender, eMultimedia, &pDev);
        IMMDeviceEnumerator_Release(pDevEnumerator);
        if (hr != S_OK) return 0;

        hr = IMMDevice_Activate(pDev, &my_IID_IAudioEndpointVolume, CLSCTX_ALL
                                    , NULL, (void**)&pAudioEndpoint);
        IMMDevice_Release(pDev);
        if (hr != S_OK) return 0;

        // Function pointer so we only need one for-loop
        typedef HRESULT WINAPI (*_VolumeStep)(IAudioEndpointVolume*, LPCGUID pguidEventContext);
        _VolumeStep VolumeStep = (_VolumeStep)(pAudioEndpoint->lpVtbl->VolumeStepDown);
        if (delta > 0)
            VolumeStep = (_VolumeStep)(pAudioEndpoint->lpVtbl->VolumeStepUp);

        // Hold shift to make 5 steps
        int i;
        int num = (state.shift)?5:1;

        for (i=0; i < num; i++) {
            hr = VolumeStep(pAudioEndpoint, NULL);
        }

        IAudioEndpointVolume_Release(pAudioEndpoint);
        myCoUninitialize();
    } else {
        if (HaveV == 2) {
            if (!hMMdll) hMMdll = LoadLibraryA("WINMM.DLL");
            if (!hMMdll) return -1;
            mywaveOutGetVolume = (void *)GetProcAddress(hMMdll, "waveOutGetVolume");
            mywaveOutSetVolume = (void *)GetProcAddress(hMMdll, "waveOutSetVolume");
            if(!mywaveOutSetVolume  || !mywaveOutGetVolume) {
                FreeLibrary(hMMdll); hMMdll=NULL;
                return 0;
            }
            HaveV = 3;
        }
        DWORD Volume;
        mywaveOutGetVolume(NULL, &Volume);

        DWORD tmp = Volume&0xFFFF0000 >> 16;
        int leftV = (int)tmp;
        tmp = (Volume&0x0000FFFF);
        int rightV = (int)tmp;
        rightV += (delta>0? 0x0800: -0x0800) * (state.shift? 4: 1);
        leftV  += (delta>0? 0x0800: -0x0800) * (state.shift? 4: 1);
        rightV = CLAMP(0x0000, rightV, 0xFFFF);
        leftV  = CLAMP(0x0000, leftV, 0xFFFF);
        Volume = ( ((DWORD)leftV) << 16 ) | ( (DWORD)rightV );
        mywaveOutSetVolume(NULL, Volume);
    }
    return -1;
}
/////////////////////////////////////////////////////////////////////////////
// Windows 2000+ Only
static int ActionTransparency(POINT pt, int delta)
{
    static int alpha=255;
    HWND hwnd = WindowFromPoint(pt);
    if (hwnd == NULL) {
        return 0;
    }
    hwnd = GetAncestor(hwnd, GA_ROOT);
    if ( blacklisted(hwnd, &BlkLst.Windows) || blacklistedP(hwnd, &BlkLst.Processes) )
        return 0;

    LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (delta < 0 && !(exstyle&WS_EX_LAYERED)) {
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle|WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    }

    BYTE old_alpha;
    if (GetLayeredWindowAttributes(hwnd, NULL, &old_alpha, NULL)) {
         alpha = old_alpha; // If possible start from the actual aplha.
    }

    int alpha_delta = (state.shift)?8:64;
    if (delta > 0) {
        alpha += alpha_delta;
        if (alpha >= 255) {
            alpha = 255;
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle & ~WS_EX_LAYERED);
            return -1;
        }
    } else {
        alpha -= alpha_delta;
        if (alpha < conf.MinAlpha)
            alpha = conf.MinAlpha;
    }
    SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);

    return -1;
}
/////////////////////////////////////////////////////////////////////////////
static int ActionLower(POINT pt, int delta)
{
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return 0;

    hwnd = GetAncestor(hwnd, GA_ROOT);

    if (delta > 0) {
        if (state.shift) {
            Maximize_Restore_atpt(hwnd, &pt, SW_TOGGLE_MAX_RESTORE, NULL);
        } else {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
            if(conf.AutoFocus) SetForegroundWindow(hwnd);
        }
    } else {
        if (state.shift) {
            PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        } else {
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
        }
    }
    return -1;
}
/////////////////////////////////////////////////////////////////////////////
static HWND MDIorNOT(HWND hwnd, HWND *mdiclient_)
{
    HWND mdiclient = NULL;
    HWND root = GetAncestor(hwnd, GA_ROOT);

    if (conf.MDI && !blacklisted(root, &BlkLst.MDIs)) {
        while (hwnd != root) {
            HWND parent = GetParent(hwnd);
            LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            if ((exstyle&WS_EX_MDICHILD)) {
                // Found MDI child, parent is now MDIClient window
                mdiclient = parent;
                break;
            }
            hwnd = parent;
        }
    } else {
        hwnd = root;
    }
    if(mdiclient_) *mdiclient_ = mdiclient;
    return hwnd;
}
/////////////////////////////////////////////////////////////////////////////
static int ActionMaxRestMin(POINT pt, int delta)
{
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return 0;
    hwnd = MDIorNOT(hwnd, NULL);
    int maximized = IsZoomed(hwnd);

    if (delta > 0) {
        if(!maximized)
            Maximize_Restore_atpt(hwnd, &pt, SW_MAXIMIZE, NULL);
    } else {
        if(maximized)
            Maximize_Restore_atpt(hwnd, &pt, SW_RESTORE, NULL);
        else
            PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    }
    if(conf.AutoFocus) SetForegroundWindow(hwnd);
    return -1;
}
/////////////////////////////////////////////////////////////////////////////
static HCURSOR CursorToDraw()
{
    HCURSOR cursor;

    if(state.action == AC_MOVE || conf.UseCursor == 3)
        return cursors[HAND];

    if ((state.resize.y == RZ_TOP && state.resize.x == RZ_LEFT)
     || (state.resize.y == RZ_BOTTOM && state.resize.x == RZ_RIGHT)) {
        cursor = cursors[SIZENWSE];
    } else if ((state.resize.y == RZ_TOP && state.resize.x == RZ_RIGHT)
     || (state.resize.y == RZ_BOTTOM && state.resize.x == RZ_LEFT)) {
        cursor = cursors[SIZENESW];
    } else if ((state.resize.y == RZ_TOP && state.resize.x == RZ_CENTER)
     || (state.resize.y == RZ_BOTTOM && state.resize.x == RZ_CENTER)) {
        cursor = cursors[SIZENS];
    } else if ((state.resize.y == RZ_CENTER && state.resize.x == RZ_LEFT)
     || (state.resize.y == RZ_CENTER && state.resize.x == RZ_RIGHT)) {
        cursor = cursors[SIZEWE];
    } else {
        cursor = cursors[SIZEALL];
    }
    return cursor;
}
/////////////////////////////////////////////////////////////////////////////
static int ActionMove(HMONITOR monitor, DWORD button)
{
    if (!monitor) {
        POINT pt;
        GetCursorPos(&pt);
        monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    }
    // Toggle Maximize window if this is a double-click

    if (GetTickCount()-state.clicktime <= conf.dbclktime && IsResizable(state.hwnd)) {
        state.action = AC_NONE; // Stop move action
        state.clicktime = 0; // Reset double-click time

        if (!conf.PearceDBClick) {
            state.blockmouseup = 1; // Block the mouseup, otherwise it can trigger a context menu
            Maximize_Restore_atpt(state.hwnd, NULL, SW_TOGGLE_MAX_RESTORE, monitor);
        } else {
            Send_DoubleClick(button);
        }
        // Prevent mousedown from propagating
        return 1;
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////
static int ActionResize(POINT pt, POINT mdiclientpt, RECT *wnd, RECT mon)
{
    if(!IsResizable(state.hwnd)) {
        state.blockmouseup = 1;
        state.action = AC_NONE;
        return 1;
    }
    // Set edge and offset
    // Think of the window as nine boxes (corner regions get 38%, middle only 24%)
    // Does not use state.origin.width/height since that is based on wndpl.rcNormalPosition
    // which is not what you see when resizing a window that Windows Aero resized
    int wndwidth  = wnd->right  - wnd->left;
    int wndheight = wnd->bottom - wnd->top;
    int SideS = (100-conf.CenterFraction)/2;
    int CenteR = 100-SideS;

    if (pt.x-wnd->left < (wndwidth*SideS)/100) {
        state.resize.x = RZ_LEFT;
        state.offset.x = pt.x-wnd->left;
    } else if (pt.x-wnd->left < (wndwidth*CenteR)/100) {
        state.resize.x = RZ_CENTER;
        state.offset.x = pt.x-mdiclientpt.x; // Used only if both x and y are CENTER
    } else {
        state.resize.x = RZ_RIGHT;
        state.offset.x = wnd->right-pt.x;
    }
    if (pt.y-wnd->top < (wndheight*SideS)/100) {
        state.resize.y = RZ_TOP;
        state.offset.y = pt.y-wnd->top;
    } else if (pt.y-wnd->top < (wndheight*CenteR)/100) {
        state.resize.y = RZ_CENTER;
        state.offset.y = pt.y-mdiclientpt.y;
    } else {
        state.resize.y = RZ_BOTTOM;
        state.offset.y = wnd->bottom-pt.y;
    }
    // Set window right/bottom origin
    state.origin.right = wnd->right-mdiclientpt.x;
    state.origin.bottom = wnd->bottom-mdiclientpt.y;

    // Aero-move this window if this is a double-click
    if (GetTickCount() - state.clicktime <= conf.dbclktime) {
        state.action = AC_NONE; // Stop resize action
        state.clicktime = 0;    // Reset double-click time
        state.blockmouseup = 1; // Block the mouseup

        // Get and set new position
        int posx, posy, wndwidth, wndheight;
        // Set for Top Left corner
        wndwidth =  CLAMPW( (mon.right-mon.left)*conf.AHoff/100 );
        wndheight = CLAMPH( (mon.bottom-mon.top)*conf.AVoff/100 );
        posx = mon.left;
        posy = mon.top;

        RECT bd;
        FixDWMRect(state.hwnd, NULL, NULL, NULL, NULL, &bd);

        if (state.resize.y == RZ_CENTER) {
            wndheight = CLAMPH(mon.bottom - mon.top);
            posy += (mon.bottom - mon.top)/2 - wndheight/2;
        } else if (state.resize.y == RZ_BOTTOM) {
            wndheight = CLAMPH( (mon.bottom-mon.top)*(100-conf.AVoff)/100 );
            posy = mon.bottom-wndheight;
        }

        if (state.resize.x == RZ_CENTER && state.resize.y != RZ_CENTER) {
            if (state.resize.y == RZ_TOP && state.shift) {
                Maximize_Restore_atpt(state.hwnd, &pt, SW_TOGGLE_MAX_RESTORE, NULL);
                return 1;
            } else {
                wndwidth = CLAMPW( (mon.right-mon.left) );
                posx += (mon.right - mon.left)/2 - wndwidth/2;
            }
        } else if (state.resize.x == RZ_CENTER) {
            if (state.shift) {
                // center-center mode and shift => max width.
                wndwidth = CLAMPW(mon.right - mon.left);
                wndheight= wnd->bottom - wnd->top - bd.top - bd.bottom ;
                posx = mon.left;
                posy = wnd->top - mdiclientpt.y + bd.top ;
            } else {
                wndwidth = wnd->right - wnd->left - bd.left - bd.right;
                posx = wnd->left - mdiclientpt.x + bd.left;
            }
        } else if (state.resize.x == RZ_RIGHT) {
            wndwidth =  CLAMPW( (mon.right-mon.left)*(100-conf.AHoff)/100 );
            posx = mon.right - wndwidth;
        }
        // FixDWMRect
        posx -= bd.left; posy -= bd.top;
        wndwidth += bd.left+bd.right; wndheight += bd.top+bd.bottom;
        MoveWindow(state.hwnd, posx, posy, wndwidth, wndheight, TRUE);

        if (!state.wndentry->restore) {
            state.wndentry->width = state.origin.width;
            state.wndentry->height = state.origin.height;
            state.wndentry->restore = 1;
        }

        // Prevent mousedown from propagating
        return 1;
    }
    if (state.resize.y == RZ_CENTER && state.resize.x == RZ_CENTER) {
        if (conf.ResizeCenter == 0) {
            state.resize.x = RZ_RIGHT;
            state.resize.y = RZ_BOTTOM;
            state.offset.y = wnd->bottom-pt.y;
            state.offset.x = wnd->right-pt.x;
        } else if (conf.ResizeCenter == 2) {
            state.action = AC_MOVE;
        }
    }

    return -1;
}
/////////////////////////////////////////////////////////////////////////////
static void AddWindowToDB(HWND hwndd)
{
    // Check if window is already in the wnddb database
    // And set it in the current state
    state.wndentry = NULL;
    int i;
    for (i=0; i < NUMWNDDB; i++) {
        if (wnddb.items[i].hwnd == hwndd) {
            state.wndentry = &wnddb.items[i];
            break;
        }
    }

    // Find a nice place in wnddb if not already present
    if (state.wndentry == NULL) {
        for (i=0; i < NUMWNDDB+1 && wnddb.pos->restore == 1; i++) {
            wnddb.pos = (wnddb.pos == &wnddb.items[NUMWNDDB-1])?&wnddb.items[0]:wnddb.pos+1;
        }
        state.wndentry = wnddb.pos;
        state.wndentry->hwnd = hwndd;
        state.wndentry->restore = 0;
    }
}
/////////////////////////////////////////////////////////////////////////////
static void GetMinMaxInfo_glob(HWND hwnd)
{
    MINMAXINFO mmi = { {0, 0}, {0, 0}, {0, 0}
                     , {GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK)}
                     , {GetSystemMetrics(SM_CXMAXTRACK), GetSystemMetrics(SM_CXMAXTRACK)} };
    SendMessage(hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&mmi);
    state.mmi.Min = mmi.ptMinTrackSize;
    state.mmi.Max = mmi.ptMaxTrackSize;
}
/////////////////////////////////////////////////////////////////////////////
static int init_movement_and_actions(POINT pt, enum action action
          , int nCode, WPARAM wParam, LPARAM lParam, DWORD button)
{
    RECT wnd;

    // Get monitor info
    HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(monitor, &mi);
    RECT mon = mi.rcWork;
    RECT fmon = mi.rcMonitor;

    // Double check if any of the hotkeys are being pressed
    if (!state.activated) {
        // Don't check if we've activated, because keyups would be blocked
        // and GetAsyncKeyState() won't return the correct state
        int i;
        for (i=0; i < conf.Hotkeys.length; i++) {
            if (GetAsyncKeyState(conf.Hotkeys.keys[i])&0x8000) {
                break;
            } else if (i+1 == conf.Hotkeys.length) {
                state.alt = 0;
                UnhookMouse();
                return 0;
            }
        }
    }

    // Okay, at least one trigger key is being pressed
    HCURSOR cursor = NULL;

    // Make sure cursorwnd isn't in the way
    ShowWindow(cursorwnd, SW_HIDE);

    // Get window
    state.mdiclient = NULL;
    state.hwnd = WindowFromPoint(pt);
    DWORD lpdwResult;
    if (state.hwnd == NULL || state.hwnd == LastWin.hwnd) {
        return 0;
    } else if (!SendMessageTimeout(state.hwnd, 0, 0, 0, SMTO_NORMAL, 50, &lpdwResult)) {
        state.blockmouseup = 1;
        return 1;
    }
    // Hide if tooltip
    wchar_t classname[20] = L"";
    state.hwnd = GetClass_HideIfTooltip(pt, state.hwnd, classname, ARR_SZ(classname));

    // MDI or not
    state.hwnd = MDIorNOT(state.hwnd, &state.mdiclient);
    POINT mdiclientpt = {0,0};
    if (state.mdiclient) {
        if (!GetClientRect(state.mdiclient, &fmon)
         || !ClientToScreen(state.mdiclient, &mdiclientpt) ) {
            return 0; // CallNextHookEx
        }
        mon = fmon;
    }

    LONG_PTR style = GetWindowLongPtr(state.hwnd, GWL_STYLE);
    WINDOWPLACEMENT wndpl = { sizeof(WINDOWPLACEMENT) };

    // Return if window is blacklisted,
    // if we can't get information about it,
    // or if the window is fullscreen and hans no sysmenu nor caption.
    if (state.hwnd == progman
     || blacklistedP(state.hwnd, &BlkLst.Processes)
     || blacklisted(state.hwnd, &BlkLst.Windows)
     || GetWindowPlacement(state.hwnd,&wndpl) == 0
     || GetWindowRect(state.hwnd,&wnd) == 0
     ||( (style&WS_SYSMENU) != WS_SYSMENU && (style&WS_CAPTION) != WS_CAPTION
         && wnd.left == fmon.left && wnd.top == fmon.top
         && wnd.right == fmon.right && wnd.bottom == fmon.bottom)) {
        return 0;
    }

    // Update state
    state.action = action;
    if (!state.snap) {
        state.snap = conf.AutoSnap;
    }
    state.activated = 1;
    state.blockaltup = 1;
    state.origin.maximized = IsZoomed(state.hwnd);
    state.origin.width = wndpl.rcNormalPosition.right-wndpl.rcNormalPosition.left;
    state.origin.height = wndpl.rcNormalPosition.bottom-wndpl.rcNormalPosition.top;
    state.origin.monitor = MonitorFromWindow(state.hwnd, MONITOR_DEFAULTTONEAREST);

    AddWindowToDB(state.hwnd);

    // AutoFocus
    if (conf.AutoFocus) { SetForegroundWindow(state.hwnd); }

    // Do things depending on what button was pressed
    if (action == AC_MOVE) { ////////////////
        GetMinMaxInfo_glob(state.hwnd); // for CLAMPH/W functions
        int ret = ActionMove(monitor, button);
        if (ret == 0)
            return 0;
        else if (ret == 1)
            return 1;
        cursor = cursors[HAND];

    } else if (action == AC_RESIZE) { ///////////////
        GetMinMaxInfo_glob(state.hwnd); // for CLAMPH/W functions

        int ret = ActionResize(pt, mdiclientpt, &wnd, mon);
        action = state.action;
        if(ret==1) return 1;
        else if (ret==0) CallNextHookEx(NULL, nCode, wParam, lParam);

        cursor = CursorToDraw();

    } else if (action == AC_MINIMIZE) {
        PostMessage(state.hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    } else if (action == AC_MAXIMIZE) {
        if(state.shift) {
            PostMessage(state.hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        } else if (IsResizable(state.hwnd)) {
            Maximize_Restore_atpt(state.hwnd, NULL, SW_TOGGLE_MAX_RESTORE, monitor);
        }
    } else if (action == AC_CENTER) {
        MoveWindow(state.hwnd, mon.left+(mon.right-mon.left)/2-state.origin.width/2
                , mon.top+(mon.bottom-mon.top)/2-state.origin.height/2
                , state.origin.width, state.origin.height, TRUE);
    } else if (action == AC_ALWAYSONTOP) {
        LONG_PTR topmost = GetWindowLongPtr(state.hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST;
        SetWindowPos(state.hwnd, (topmost?HWND_NOTOPMOST:HWND_TOPMOST), 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);

    } else if (action == AC_BORDERLESS) {
        long style = GetWindowLongPtr(state.hwnd, GWL_STYLE);

        if(style&WS_BORDER) style &= state.shift? ~WS_CAPTION: ~(WS_CAPTION|WS_THICKFRAME);
        else style |= WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU;

        SetWindowLongPtr(state.hwnd, GWL_STYLE, style);

        // Under Windows 10, with DWM we HAVE to resize the windows twice
        // to have proper drawing. this is a bug...
        if(HaveDWM()) {
            RECT rc;
            GetWindowRect(state.hwnd, &rc);
            SetWindowPos(state.hwnd, NULL, rc.left, rc.top, rc.right-rc.left+1, rc.bottom-rc.top
                       , SWP_NOMOVE|SWP_FRAMECHANGED|SWP_NOZORDER);
            SetWindowPos(state.hwnd, NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top
                       , SWP_NOMOVE|SWP_NOZORDER);
        } else {
            SetWindowPos(state.hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED|SWP_NOZORDER);
        }

    } else if (action == AC_CLOSE) {
        PostMessage(state.hwnd, WM_CLOSE, 0, 0);
    } else if (action == AC_LOWER) {
        if (state.shift) {
            PostMessage(state.hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        } else {
            SetWindowPos(state.hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
        }
    }

    // Send WM_ENTERSIZEMOVE
    SendSizeMove_on(action, 1);

    // We have to send the ctrl keys here too because of IE (and maybe some other program?)
    Send_CTRL();

    // Remember time of this click so we can check for double-click
    state.clicktime = GetTickCount();
    state.clickpt = pt;

    // Update cursor
    if (conf.UseCursor && cursorwnd && cursor) {
        MoveWindow(cursorwnd, pt.x-20, pt.y-20, 41, 41, FALSE);
        ShowWindow(cursorwnd, SW_SHOWNA);
        SetCursor(cursor);
        SetCapture(cursorwnd);
    }

    // Prevent mousedown from propagating
    return 1;
}
/////////////////////////////////////////////////////////////////////////////
// This is somewhat the main function, it is active only when the ALT key is
// pressed, or is always on when InactiveScroll or LowerWithMMB are enabled.
__declspec(dllexport) LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode != HC_ACTION || state.ignoreclick)
        return CallNextHookEx(NULL, nCode, wParam, lParam);

    // Set up some variables
    PMSLLHOOKSTRUCT msg = (PMSLLHOOKSTRUCT)lParam;
    POINT pt = msg->pt;

    // Handle mouse move and scroll
    if (wParam == WM_MOUSEMOVE) {
        // Store prevpt so we can check if the hook goes stale
        state.prevpt = pt;
        // Move the window
        if (state.action == AC_MOVE || state.action == AC_RESIZE) {
            static char updaterate;
            updaterate = (updaterate+1)%(state.action==AC_MOVE
                              ? conf.MoveRate: conf.ResizeRate);
            if (updaterate == 0) {
                MouseMove(pt);
            }
        }
        // Reset double-click time
        if (!IsSamePTT(pt, state.clickpt)) {
            state.clicktime = 0;
        }

    } else if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
        if (state.alt && !state.action && conf.Mouse.Scroll) {
            int delta = GET_WHEEL_DELTA_WPARAM(msg->mouseData);

            if (conf.Mouse.Scroll == AC_ALTTAB && !state.shift) {
                if(!ActionAltTab(pt, delta))
                    CallNextHookEx(NULL, nCode, wParam, lParam);

            } else if (conf.Mouse.Scroll == AC_VOLUME) {
                if(!ActionVolume(delta))
                    return CallNextHookEx(NULL, nCode, wParam, lParam);

            } else if (conf.Mouse.Scroll == AC_TRANSPARENCY) {
                if(!ActionTransparency(pt, delta))
                    return CallNextHookEx(NULL, nCode, wParam, lParam);

            } else if (conf.Mouse.Scroll == AC_LOWER) {
                if(!ActionLower(pt, delta))
                    return CallNextHookEx(NULL, nCode, wParam, lParam);

            } else if (conf.Mouse.Scroll == AC_MAXIMIZE) {
                if(!ActionMaxRestMin(pt, delta))
                    return CallNextHookEx(NULL, nCode, wParam, lParam);
            }
            // Block original scroll event
            state.blockaltup = 1;
            state.activated = 1;
            return 1;
        } else if (!state.action && conf.InactiveScroll) {
            if(!ScrollPointedWindow(pt, msg->mouseData, wParam))
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            else return 1;
        }
    }

    // Get Button state
    int button =
        (wParam==WM_LBUTTONDOWN||wParam==WM_LBUTTONUP)?BT_LMB:
        (wParam==WM_MBUTTONDOWN||wParam==WM_MBUTTONUP)?BT_MMB:
        (wParam==WM_RBUTTONDOWN||wParam==WM_RBUTTONUP)?BT_RMB:
        (HIWORD(msg->mouseData)==XBUTTON1)?BT_MB4:
        (HIWORD(msg->mouseData)==XBUTTON2)?BT_MB5:BT_NONE;

    enum {STATE_NONE, STATE_DOWN, STATE_UP} buttonstate =
          (wParam==WM_LBUTTONDOWN||wParam==WM_MBUTTONDOWN
        || wParam==WM_RBUTTONDOWN||wParam==WM_XBUTTONDOWN)? STATE_DOWN:
          (wParam==WM_LBUTTONUP  ||wParam==WM_MBUTTONUP
        || wParam==WM_RBUTTONUP  ||wParam==WM_XBUTTONUP)?STATE_UP:STATE_NONE;

    // Lower window if middle mouse button is used on the title bar/top of the win
    if (conf.LowerWithMMB && !state.alt && !state.action && buttonstate == STATE_DOWN && button == BT_MMB) {
        HWND hwnd = WindowFromPoint(pt);
        if (!hwnd) return CallNextHookEx(NULL, nCode, wParam, lParam);
        hwnd = GetAncestor(hwnd, GA_ROOT);
        if (blacklisted(hwnd, &BlkLst.Windows))
            return CallNextHookEx(NULL, nCode, wParam, lParam);

        int area = SendMessage(hwnd, WM_NCHITTEST, 0, MAKELPARAM(pt.x,pt.y));
        if (area == HTCAPTION || area == HTTOP || area == HTTOPLEFT || area == HTTOPRIGHT
         || area == HTSYSMENU || area == HTMINBUTTON || area == HTMAXBUTTON || area == HTCLOSE) {
            if (state.shift) {
                PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            } else {
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
            }
            return 1;
        }
    }

    // Return if no mouse action will be started
    enum action action = GetAction(button);
    if (!action) return CallNextHookEx(NULL, nCode, wParam, lParam);

    // Block mousedown if we are busy with another action
    if (state.action && buttonstate == STATE_DOWN) {
        return 1; // Block mousedown so AltDrag.exe does not remove cursorwnd

    } else if (buttonstate == STATE_UP && state.blockmouseup) {
        state.blockmouseup = 0;
        return 1;

    } else if (state.alt && buttonstate == STATE_DOWN) {
        int ret = init_movement_and_actions(pt, action, nCode, wParam, lParam, button);
        action = state.action;
        if(!ret) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        } else {
            return 1;
        }

    // FINISHING THE MOVE
    } else if (buttonstate == STATE_UP && state.action) {
        if (was_moving && LastWin.hwnd) {
             if(!conf.FullWin) // to erase the last rectangle...
                 Rectangle(hdcc, oldRect.left, oldRect.top, oldRect.right, oldRect.bottom);

             DWORD lpThreadId;
             HANDLE thread;
             thread = CreateThread(NULL, 0, EndMoveWindowThread, &LastWin, 0, &lpThreadId);
             CloseHandle(thread);
        }

        // Auto Remaximize if option enabled and conditions are met.
        if(conf.AutoRemaximize && was_moving && state.origin.maximized
          && !state.shift && !state.mdiclient && state.action == AC_MOVE) {
            HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            if(monitor != state.origin.monitor){
                Sleep(10); // Wait a little for moveThread.
                Maximize_Restore_atpt(state.hwnd, NULL, SW_MAXIMIZE, monitor);
            }
            state.snap = conf.AutoSnap;
        }

        state.action = AC_NONE;
        was_moving = 0;

        // Send WM_EXITSIZEMOVE
        SendSizeMove_on(action, 0);

        // Unhook mouse?
        if (!state.alt) {
            UnhookMouse();
        }

        // Hide cursorwnd
        ReleaseCapture();
        ShowWindowAsync(cursorwnd, SW_HIDE);

        // Prevent mouseup from propagating
        return 1;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
} // END OF LL MOUSE PROCK

/////////////////////////////////////////////////////////////////////////////
static int HookMouse()
{
    was_moving = 0; // Used to know the first time we call MouseMove.
    state.ignoreclick = 0;
    if (conf.InactiveScroll || conf.LowerWithMMB) {
        SendMessage(g_hwnd, WM_TIMER, REHOOK_TIMER, 0);
    }

    // Check if mouse is already hooked
    if (mousehook)
        return 1;

    // Set up the mouse hook
    mousehook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hinstDLL, 0);
    if (!mousehook)
        return 1;
    else
        return 0;
}

/////////////////////////////////////////////////////////////////////////////
static void UnhookMouse()
{
    // Stop action
    state.action = AC_NONE;
    state.activated = 0;
    was_moving = 0; // Just in case
    ClipCursor(NULL);  // Release cursor trapping in case...

    if (hdcc) { DeleteDC(hdcc); hdcc = NULL; }
    if (hpenDot_Global) { DeleteObject(hpenDot_Global); hpenDot_Global = NULL; }

    ShowWindowAsync(cursorwnd, SW_HIDE);
    ReleaseCapture();

    // Do not unhook if not hooked or if the hook is still used for something
    if (!mousehook || conf.InactiveScroll || conf.LowerWithMMB)
        return;

    // Remove mouse hook
    UnhookWindowsHookEx(mousehook);
    mousehook = NULL;
}
/////////////////////////////////////////////////////////////////////////////
// Window for timers only...
static LRESULT CALLBACK TimerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TIMER) {
        if (wParam == INIT_TIMER) {
            KillTimer(g_hwnd, wParam);

            // Hook mouse if a permanent hook is needed
            if (conf.InactiveScroll || conf.LowerWithMMB) {
                HookMouse();
                SetTimer(g_hwnd, REHOOK_TIMER, 5000, NULL); // Start rehook timer
            }
        } else if (wParam == REHOOK_TIMER) {
            // Silently rehook hooks if they have been stopped (>= Win7 and LowLevelHooksTimeout)
            // This can often happen if locking or sleeping the computer a lot
            POINT pt;
            GetCursorPos(&pt);
            if (mousehook && (pt.x != state.prevpt.x || pt.y != state.prevpt.y)) {
                UnhookWindowsHookEx(mousehook);
                mousehook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hinstDLL, 0);
            }
        }
    } else if (msg == WM_DESTROY) {
        KillTimer(g_hwnd, REHOOK_TIMER);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////
// To be called before Free Library. Ideally it should free everything
__declspec(dllexport) void Unload()
{
    if (hdcc) { DeleteDC(hdcc); hdcc = NULL; }
    if (hpenDot_Global) { DeleteObject(hpenDot_Global); hpenDot_Global = NULL; }

    if (mousehook) { UnhookWindowsHookEx(mousehook); mousehook = NULL; }
    DestroyWindow(g_hwnd);
}
/////////////////////////////////////////////////////////////////////////////
// blacklist is coma separated ans title and class are | separated.
static void readblacklist(wchar_t *inipath, struct blacklist *blacklist, wchar_t *blacklist_str)
{
    wchar_t txt[2048];

    DWORD ret = GetPrivateProfileString(L"Blacklist", blacklist_str, L"", txt, ARR_SZ(txt), inipath);
    if(!ret || txt[0] == '\0'){
        blacklist->data = NULL;
        blacklist->length = 0;
        blacklist->items = NULL;
        return;
    }
    blacklist->data = malloc((wcslen(txt)+1)*sizeof(wchar_t));
    wcscpy(blacklist->data, txt);
    wchar_t *pos = blacklist->data;

    while (pos) {
        wchar_t *title = pos;
        wchar_t *class = wcsstr(pos, L"|");

        // Move pos to next item (if any)
        pos = wcsstr(pos, L",");
        if (pos) {
            *pos = '\0';
            pos++;
        }

        // Split the item with NULL
        if (class) {
           *class = '\0';
           class++;
        }
        // Add blacklist item
        if (title) {
            if (title[0] == '\0') {
                title = L"";
            } else if (!wcscmp(title,L"*")) {
                title = NULL;
            }
            if (class && !wcscmp(class,L"*")) {
                class = NULL;
            }
            // Allocate space
            blacklist->items = realloc(blacklist->items, (blacklist->length+1)*sizeof(struct blacklistitem));

            // Store item
            blacklist->items[blacklist->length].title = title;
            blacklist->items[blacklist->length].classname = class;
            blacklist->length++;
        }
    } // end while
}
///////////////////////////////////////////////////////////////////////////
// Has to be called at startup, it mainly reads the config.
__declspec(dllexport) void Load(void)
{
    // Load settings
    wchar_t txt[1024];
    wchar_t inipath[MAX_PATH];
    state.action = AC_NONE;
    state.shift = 0;
    LastWin.hwnd = NULL;

    conf.Hotkeys.length = 0;
    conf.dbclktime = GetDoubleClickTime();

    // Get ini path
    GetModuleFileName(NULL, inipath, ARR_SZ(inipath));
    wcscpy(&inipath[wcslen(inipath)-3], L"ini");

    // [General]
    conf.AutoFocus =    GetPrivateProfileInt(L"General", L"AutoFocus", 0, inipath);
    conf.AutoSnap=state.snap=GetPrivateProfileInt(L"General", L"AutoSnap", 0, inipath);
    conf.Aero =         GetPrivateProfileInt(L"General", L"Aero", 1, inipath);
    conf.InactiveScroll=GetPrivateProfileInt(L"General", L"InactiveScroll", 0, inipath);
    conf.MDI =          GetPrivateProfileInt(L"General", L"MDI", 0, inipath);
    conf.ResizeCenter = GetPrivateProfileInt(L"General", L"ResizeCenter", 1, inipath);
    conf.CenterFraction=CLAMP(0, GetPrivateProfileInt(L"General", L"CenterFraction", 24, inipath), 100);
    conf.AHoff        = CLAMP(0, GetPrivateProfileInt(L"General", L"AeroHoffset", 50, inipath),    100);
    conf.AVoff        = CLAMP(0, GetPrivateProfileInt(L"General", L"AeroVoffset", 50, inipath),    100);

    // [Advanced]
    conf.ResizeAll     = GetPrivateProfileInt(L"Advanced", L"ResizeAll",      1, inipath);
    conf.AutoRemaximize= GetPrivateProfileInt(L"Advanced", L"AutoRemaximize", 0, inipath);
    conf.SnapThreshold = GetPrivateProfileInt(L"Advanced", L"SnapThreshold", 20, inipath);
    conf.AeroThreshold = GetPrivateProfileInt(L"Advanced", L"AeroThreshold",  5, inipath);
    conf.AeroTopMaximizes=GetPrivateProfileInt(L"Advanced",L"AeroTopMaximizes",1, inipath);
    conf.UseCursor     = GetPrivateProfileInt(L"Advanced", L"UseCursor", 1, inipath);
    conf.PearceDBClick = GetPrivateProfileInt(L"Advanced", L"PearceDBClick", 0, inipath);
    conf.MinAlpha      = GetPrivateProfileInt(L"Advanced", L"MinAlpha", 8, inipath);

    // CURSOR STUFF
    cursorwnd = FindWindow(APP_NAME, NULL);
    cursors[HAND]     = conf.UseCursor>1? LoadCursor(NULL, IDC_ARROW): LoadCursor(NULL, IDC_HAND);
    cursors[SIZENWSE] = LoadCursor(NULL, IDC_SIZENWSE);
    cursors[SIZENESW] = LoadCursor(NULL, IDC_SIZENESW);
    cursors[SIZENS]   = LoadCursor(NULL, IDC_SIZENS);
    cursors[SIZEWE]   = LoadCursor(NULL, IDC_SIZEWE);
    cursors[SIZEALL]  = LoadCursor(NULL, IDC_SIZEALL);

    // [Performance]
    conf.MoveRate  = GetPrivateProfileInt(L"Performance", L"MoveRate", 2, inipath);
    conf.ResizeRate= GetPrivateProfileInt(L"Performance", L"ResizeRate", 4, inipath);
    conf.FullWin   = GetPrivateProfileInt(L"Performance", L"FullWin", 1, inipath);
    conf.RefreshRate=GetPrivateProfileInt(L"Performance", L"RefreshRate", 5, inipath);

    // [Input]
    struct {
        wchar_t *key;
        wchar_t *def;
        enum action *ptr;
    } buttons[] = {
        {L"LMB",    L"Move",    &conf.Mouse.LMB},
        {L"MMB",    L"Maximize",&conf.Mouse.MMB},
        {L"RMB",    L"Resize",  &conf.Mouse.RMB},
        {L"MB4",    L"Nothing", &conf.Mouse.MB4},
        {L"MB5",    L"Nothing", &conf.Mouse.MB5},
        {L"Scroll", L"Nothing", &conf.Mouse.Scroll},
        {NULL}
    };
    int i;
    for (i=0; buttons[i].key != NULL; i++) {
        GetPrivateProfileString(L"Input", buttons[i].key, buttons[i].def, txt, ARR_SZ(txt), inipath);
        if      (!wcsicmp(txt,L"Move"))         *buttons[i].ptr = AC_MOVE;
        else if (!wcsicmp(txt,L"Resize"))       *buttons[i].ptr = AC_RESIZE;
        else if (!wcsicmp(txt,L"Minimize"))     *buttons[i].ptr = AC_MINIMIZE;
        else if (!wcsicmp(txt,L"Maximize"))     *buttons[i].ptr = AC_MAXIMIZE;
        else if (!wcsicmp(txt,L"Center"))       *buttons[i].ptr = AC_CENTER;
        else if (!wcsicmp(txt,L"AlwaysOnTop"))  *buttons[i].ptr = AC_ALWAYSONTOP;
        else if (!wcsicmp(txt,L"Borderless"))   *buttons[i].ptr = AC_BORDERLESS;
        else if (!wcsicmp(txt,L"Close"))        *buttons[i].ptr = AC_CLOSE;
        else if (!wcsicmp(txt,L"Lower"))        *buttons[i].ptr = AC_LOWER;
        else if (!wcsicmp(txt,L"AltTab"))       *buttons[i].ptr = AC_ALTTAB;
        else if (!wcsicmp(txt,L"Volume"))       *buttons[i].ptr = AC_VOLUME;
        else if (!wcsicmp(txt,L"Transparency")) *buttons[i].ptr = AC_TRANSPARENCY;
        else                                    *buttons[i].ptr = AC_NONE;
    }

    conf.LowerWithMMB    = GetPrivateProfileInt(L"Input", L"LowerWithMMB",    0, inipath);
    conf.AggressivePause = GetPrivateProfileInt(L"Input", L"AggressivePause", 0, inipath);
    unsigned temp;
    int numread;
    GetPrivateProfileString(L"Input", L"Hotkeys", L"A4 A5", txt, ARR_SZ(txt), inipath);
    wchar_t *pos = txt;
    while (*pos != '\0' && swscanf(pos,L"%02X%n",&temp,&numread) != EOF) {
        // Bail if we are out of space
        if (conf.Hotkeys.length == MAXKEYS) {
            break;
        }
        // Store key
        conf.Hotkeys.keys[conf.Hotkeys.length++] = temp;
        pos += numread;
    }

    // Zero-out wnddb hwnds
    for (i=0; i < NUMWNDDB; i++) {
        wnddb.items[i].hwnd = NULL;
    }
    wnddb.pos = &wnddb.items[0];

    // Create window for timers
    WNDCLASSEX wnd = { sizeof(WNDCLASSEX), 0, TimerWindowProc, 0, 0, hinstDLL
                     , NULL, NULL, NULL, NULL, APP_NAME"-hooks", NULL };
    RegisterClassEx(&wnd);
    g_hwnd = CreateWindowEx(0, wnd.lpszClassName, wnd.lpszClassName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT
                     , CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, hinstDLL, NULL);
    // Create a timer to do further initialization
    SetTimer(g_hwnd, INIT_TIMER, 10, NULL);

    readblacklist(inipath, &BlkLst.Processes, L"Processes");
    readblacklist(inipath, &BlkLst.Windows,   L"Windows");
    readblacklist(inipath, &BlkLst.Snaplist,  L"Snaplist");
    readblacklist(inipath, &BlkLst.MDIs,      L"MDIs");
    readblacklist(inipath, &BlkLst.Pause,     L"Pause");

    // Allocate some memory
    monitors_alloc++;
    monitors = realloc(monitors, monitors_alloc*sizeof(RECT));
    wnds_alloc += 8;
    wnds = realloc(wnds, wnds_alloc*sizeof(RECT));
}
/////////////////////////////////////////////////////////////////////////////
// Do not forget the -e_DllMain@12 for gcc...
BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        hinstDLL = hInst;
    }
    return TRUE;
}
