#include "stdafx.h"
#include "snows.h"
#include "renderer.h"

#define TBCREATED_TEXT  "TaskbarCreated"
#define WNDCLS_NAME     "SNOW"
#define MAINWNDCLS_NAME "SNOWMAIN"
#define WND_EXSIZE(n)   (sizeof(LONG_PTR) * (n))
#define WND_EXOFFSET(n) WND_EXSIZE(n)

#define WM_SNOWNIMSG    (WM_APP + 0)
#define WM_SNOWNIRESET  (WM_APP + 1)
#define WM_SNOWSTART    (WM_APP + 2)
#define WM_SNOWSTOP     (WM_APP + 3)
#define WM_SNOWCLOSE    (WM_APP + 4)

#define SNOW_TIMERID    1
#define SNOW_NIID       1
#define SNOW_NITIPSTR   "Animated Snow Wallpaper"


struct ThreadInfo {
    bool valid;
    bool running;
    HWND hwnd;
};

using ThreadMap = std::unordered_map<HMONITOR, ThreadInfo>;


void safeResize(SnowRenderer& sr, UINT width, UINT height) {
    try {
        sr.resize(width, height);
    }
    catch (ComException&) { //refresh device when error occured
        sr.refreash();
    }
}

HRESULT safeRender(SnowRenderer& sr, SnowList& sl, HRESULT hr, float alpha = 1.0f) {
    if (hr == DXGI_STATUS_OCCLUDED) return sr.presentTest();
    hr = sr.render(sl.list, alpha);
    if (hr != S_OK && hr != DXGI_STATUS_OCCLUDED) { //error occur, refresh device and discard this frame
        sr.refreash();
        hr = S_FALSE;
    }
    return hr;
}

LRESULT CALLBACK SnowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    SnowList* psl = (SnowList*)GetWindowLongPtr(hwnd, WND_EXOFFSET(0));
    SnowRenderer* psr = (SnowRenderer*)GetWindowLongPtr(hwnd, WND_EXOFFSET(1));
    HRESULT* phr = (HRESULT*)GetWindowLongPtr(hwnd, WND_EXOFFSET(2));

    switch (msg) {
    case WM_CREATE: //default state is stopped
        BEGINCASECODE;
        LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lparam;
        LPRECT lprcwork = (LPRECT)lpcs->lpCreateParams;
        std::random_device rd;

        psl = new SnowList(lpcs->cx, lpcs->cy, lprcwork->bottom - lprcwork->top, GetDpiForWindow(hwnd), rd());
        SetWindowLongPtr(hwnd, WND_EXOFFSET(0), (LONG_PTR)psl);
        psr = new SnowRenderer;
        psr->initialize(lpcs->cx, lpcs->cy, hwnd);
        SetWindowLongPtr(hwnd, WND_EXOFFSET(1), (LONG_PTR)psr);
        phr = new HRESULT(S_OK);
        SetWindowLongPtr(hwnd, WND_EXOFFSET(2), (LONG_PTR)phr);

        SetWindowPos(hwnd, HWND_BOTTOM, lpcs->x, lpcs->y, lpcs->cx, lpcs->cy, SWP_NOACTIVATE | SWP_NOREDRAW);
        ENDCASECODE;
        return 0;
    case WM_DESTROY:
        delete psl;
        delete psr;
        delete phr;
        KillTimer(hwnd, SNOW_TIMERID);
        PostQuitMessage(0);
        return 0;
    case WM_SNOWSTART:  //start the animation
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetTimer(hwnd, SNOW_TIMERID, (1000 / SnowList::FPS), nullptr);
        return 0;
    case WM_SNOWSTOP:   //stop the animation
        ShowWindow(hwnd, SW_HIDE);
        KillTimer(hwnd, SNOW_TIMERID);
        return 0;
    case WM_SNOWCLOSE:  //close window and exit thread
        DestroyWindow(hwnd);
        return 0;
    case WM_DPICHANGED: //refresh animation if DPI changed
        BEGINCASECODE;
        UINT dpi = LOWORD(wparam);
        if (dpi != psl->dpi) {
            psl->dpi = dpi;
            psl->refreshList();
        }
        ENDCASECODE;
        return 0;
    case WM_DISPLAYCHANGE:  //refresh animation and device if resolution changed
        BEGINCASECODE;
        UINT xres = LOWORD(lparam), yres = HIWORD(lparam);
        if (xres != psl->xres || yres != psl->yres) {
            psl->xres = xres;
            psl->yres = yres;
            psl->refreshList();
            SetWindowPos(hwnd, 0, 0, 0, xres, yres, SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOMOVE);
            safeResize(*psr, xres, yres);
        }
        ENDCASECODE;
        return 0;
    case WM_PAINT:
        BEGINCASECODE;
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        *phr = safeRender(*psr, *psl, *phr);
        EndPaint(hwnd, &ps);
        ENDCASECODE;
        return 0;
    case WM_TIMER:
        BEGINCASECODE;
        HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
        if (hmon) { //update current ground in each frame
            MONITORINFO monii = { sizeof(MONITORINFO) };
            GetMonitorInfo(hmon, &monii);
            psl->ground = monii.rcWork.bottom - monii.rcWork.top;
        }
        psl->nextFrame();
        *phr = safeRender(*psr, *psl, *phr);
        ENDCASECODE;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void runThread(HMONITOR hmon, HWND* phwnd, HANDLE hevent) {
    SnowRenderer::HR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));    //for WIC
    MONITORINFO monii = { sizeof(MONITORINFO) };
    GetMonitorInfo(hmon, &monii);

    HWND hwnd = CreateWindowEx(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        TEXT(WNDCLS_NAME), nullptr,
        WS_POPUP,
        monii.rcMonitor.left, monii.rcMonitor.top,
        monii.rcMonitor.right - monii.rcMonitor.left, monii.rcMonitor.bottom - monii.rcMonitor.top,
        nullptr, nullptr, GetModuleHandle(nullptr), &monii.rcWork
    );

    *phwnd = hwnd;
    SetEvent(hevent);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    CoUninitialize();
}


BOOL CALLBACK MonitorEnumProc(HMONITOR hmonitor, HDC hdc, LPRECT lprect, LPARAM lparam) {
    ThreadMap* ptm = (ThreadMap*)lparam;
    auto item = ptm->find(hmonitor);
    if (item == ptm->end()) {   //create a new thread if find a new monitor
        ThreadInfo thi = { false, false, nullptr };
        HANDLE hevent = CreateEvent(nullptr, false, false, nullptr);
        std::thread th(runThread, hmonitor, &thi.hwnd, hevent);
        th.detach();
        WaitForSingleObject(hevent, INFINITE);
        CloseHandle(hevent);
        item = ptm->emplace(hmonitor, thi).first;
    }
    (*item).second.valid = true;    //mark existing threads as valid
    return TRUE;
}

LRESULT onCreate(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lparam;
    SetWindowLongPtr(hwnd, WND_EXOFFSET(1), (LONG_PTR)lpcs->lpCreateParams);    //store taskbar created message
    SendMessage(hwnd, WM_SNOWNIRESET, 0, 0);    //insert notify icon

    ThreadMap* ptm = new ThreadMap;
    SetWindowLongPtr(hwnd, WND_EXOFFSET(0), (LONG_PTR)ptm);
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)ptm);
    for (auto& item : *ptm) {   //default program state is show
        PostMessage(item.second.hwnd, WM_SNOWSTART, 0, 0);
        item.second.running = true;
    }

    HMENU hnimenu = GetSubMenu(lpcs->hMenu, 0);
    CheckMenuRadioItem(hnimenu, ID_NIM_SHOW, ID_NIM_HIDE, ID_NIM_SHOW, MF_BYCOMMAND);
    return 0;
}

LRESULT onDestroy(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
    nid.hWnd = hwnd;
    nid.uID = SNOW_NIID;
    Shell_NotifyIcon(NIM_DELETE, &nid);

    ThreadMap* ptm = (ThreadMap*)GetWindowLongPtr(hwnd, WND_EXOFFSET(0));
    for (auto& item : *ptm) {   //kill threads
        PostMessage(item.second.hwnd, WM_SNOWCLOSE, 0, 0);
    }
    delete ptm;

    PostQuitMessage(0);
    return 0;
}

LRESULT onCommand(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    ThreadMap* ptm = (ThreadMap*)GetWindowLongPtr(hwnd, WND_EXOFFSET(0));
    HMENU hnimenu = GetSubMenu(GetMenu(hwnd), 0);
    if (HIWORD(wparam) == 0) {  //menu
        switch (LOWORD(wparam)) {
        case ID_NIM_SHOW:   //start animation
            for (auto& item : *ptm) {
                PostMessage(item.second.hwnd, WM_SNOWSTART, 0, 0);
                item.second.running = true;
            }
            CheckMenuRadioItem(hnimenu, ID_NIM_SHOW, ID_NIM_HIDE, ID_NIM_SHOW, MF_BYCOMMAND);
            break;
        case ID_NIM_HIDE:   //stop animation
            for (auto& item : *ptm) {
                PostMessage(item.second.hwnd, WM_SNOWSTOP, 0, 0);
                item.second.running = false;
            }
            CheckMenuRadioItem(hnimenu, ID_NIM_SHOW, ID_NIM_HIDE, ID_NIM_HIDE, MF_BYCOMMAND);
            break;
        case ID_NIM_REFRESH:    //restart threads
            for (auto& item : *ptm) PostMessage(item.second.hwnd, WM_SNOWCLOSE, 0, 0);
            ptm->clear();
            EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)ptm);
            for (auto& item : *ptm) {
                PostMessage(item.second.hwnd, WM_SNOWSTART, 0, 0);
                item.second.running = true;
            }
            CheckMenuRadioItem(hnimenu, ID_NIM_SHOW, ID_NIM_HIDE, ID_NIM_SHOW, MF_BYCOMMAND);
            break;
        case ID_NIM_EXIT:   //exit program
            DestroyWindow(hwnd);
            break;
        }
    }
    return 0;
}

LRESULT onSnowNIMsg(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    UINT niid = HIWORD(lparam), msg = LOWORD(lparam);   //NOTIFYICON_VERSION_4
    HMENU hnimenu = GetSubMenu(GetMenu(hwnd), 0);   //context menu for notify icon
    switch (msg) {
    case WM_CONTEXTMENU:
    case NIN_SELECT:
    case NIN_KEYSELECT: //show context menu, see Remarks section for TrackPopupMenu() in MSDN
        SetForegroundWindow(hwnd);
        TrackPopupMenuEx(hnimenu, TPM_NOANIMATION, MAKEPOINTS(wparam).x, MAKEPOINTS(wparam).y, hwnd, nullptr);
        PostMessage(hwnd, WM_NULL, 0, 0);
        break;
    }
    return 0;
}

LRESULT onSnowNIReset(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.hWnd = hwnd;
    nid.uID = SNOW_NIID;
    nid.uCallbackMessage = WM_SNOWNIMSG;
    nid.hIcon = LoadIcon((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), MAKEINTRESOURCE(IDI_ICON));
    nid.uVersion = NOTIFYICON_VERSION_4;
    _tcscpy_s(nid.szTip, TEXT(SNOW_NITIPSTR));
    Shell_NotifyIcon(NIM_ADD, &nid);
    Shell_NotifyIcon(NIM_SETVERSION, &nid);
    return 0;
}

LRESULT onDisplayChange(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    ThreadMap* ptm = (ThreadMap*)GetWindowLongPtr(hwnd, WND_EXOFFSET(0));
    for (auto& item : *ptm) item.second.valid = false;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)ptm);
    ThreadMap tmp = *ptm;
    for (auto& item : tmp) {
        if (!item.second.running) { //start animation for new threads
            PostMessage(item.second.hwnd, WM_SNOWSTART, 0, 0);
            ptm->at(item.first).running = true;
        }
        if (!item.second.valid) {   //remove invalid threads
            PostMessage(item.second.hwnd, WM_SNOWCLOSE, 0, 0);
            ptm->erase(item.first);
        }
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    PUINT ptbc_msg = (PUINT)GetWindowLongPtr(hwnd, WND_EXOFFSET(1));
    if (ptbc_msg && *ptbc_msg == msg)  //redirect taskbar created message
        msg = WM_SNOWNIRESET;

    switch (msg) {
    case WM_CREATE:
        return onCreate(hwnd, wparam, lparam);
    case WM_DESTROY:
        return onDestroy(hwnd, wparam, lparam);
    case WM_COMMAND:
        return onCommand(hwnd, wparam, lparam);
    case WM_SNOWNIMSG:
        return onSnowNIMsg(hwnd, wparam, lparam);
    case WM_SNOWNIRESET:
        return onSnowNIReset(hwnd, wparam, lparam);
    case WM_DISPLAYCHANGE:
        return onDisplayChange(hwnd, wparam, lparam);
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}


ATOM WINAPI RegisterClassExWrapper(LPCTSTR lpClsName, WNDPROC lpfnWndProc, HINSTANCE hInst, HICON hIcon, int cbWndExtra) {
    WNDCLASSEX wndc = { sizeof(WNDCLASSEX) };
    wndc.lpszClassName = lpClsName;
    wndc.lpfnWndProc = lpfnWndProc;
    wndc.hInstance = hInst;
    wndc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndc.hIcon = hIcon;
    wndc.cbWndExtra = cbWndExtra;
    return RegisterClassEx(&wndc);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    RegisterClassExWrapper(TEXT(WNDCLS_NAME), SnowProc, hInstance, nullptr, WND_EXSIZE(3));
    RegisterClassExWrapper(TEXT(MAINWNDCLS_NAME), WndProc, hInstance, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON)), WND_EXSIZE(2));

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    UINT tbc_msg = RegisterWindowMessage(TEXT(TBCREATED_TEXT));
    HWND hwnd = CreateWindowEx(
        0,
        TEXT(MAINWNDCLS_NAME),
        nullptr,
        WS_POPUP,
        0, 0, 0, 0,
        nullptr,
        LoadMenu(hInstance, MAKEINTRESOURCE(IDR_NIMENU)),
        hInstance,
        &tbc_msg
    );

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
