#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>
#include <stdbool.h>

#ifndef AddClipboardFormatListener
WINUSERAPI BOOL WINAPI AddClipboardFormatListener(HWND hwnd);
#endif
#ifndef RemoveClipboardFormatListener
WINUSERAPI BOOL WINAPI RemoveClipboardFormatListener(HWND hwnd);
#endif

#define MAX_FILTERS 64
#define MAX_FILTER_LEN 256

wchar_t filters[MAX_FILTERS][MAX_FILTER_LEN];
int filterCount = 0;

// Tray/icon and menu
#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY 100
#define IDM_TOGGLE 40001
#define IDM_EXIT   40002
#define IDM_EDIT   40003

static BOOL g_enabled = TRUE; // filtering enabled by default
static NOTIFYICONDATAW g_nid;
static HICON g_hIconOn = NULL;
static HICON g_hIconOff = NULL;

void loadFilters() {
    FILE *f = _wfopen(L"filter.txt", L"r, ccs=UTF-8");
    if (!f) {
        MessageBox(NULL, L"Could not open filter.txt", L"Error", MB_OK);
        return;
    }

    wchar_t line[MAX_FILTER_LEN];

    while (fgetws(line, MAX_FILTER_LEN, f) && filterCount < MAX_FILTERS) {
        size_t len = wcslen(line);
        while (len > 0 && (line[len - 1] == L'\n' || line[len - 1] == L'\r')) {
            line[--len] = L'\0';
        }

        if (len > 0) {
            wcscpy(filters[filterCount++], line);
        }
    }

    fclose(f);
}

void removeSubstring(wchar_t *source, const wchar_t *pattern) {
    if (!source || !pattern) return;

    size_t patLen = wcslen(pattern);
    if (patLen == 0) return;

    wchar_t *match;
    while ((match = wcsstr(source, pattern))) {
        size_t tailLen = wcslen(match + patLen) + 1;
        // memmove to handle overlapping memory regions safely
        memmove(match, match + patLen, tailLen * sizeof(wchar_t));
    }
}

void applyFilters(wchar_t *text) {
    for (int i = 0; i < filterCount; i++) {
        removeSubstring(text, filters[i]);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLIPBOARDUPDATE) {
        // If filtering is disabled, do nothing
        if (!g_enabled) return 0;

        if (OpenClipboard(hwnd)) {

            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t *data = GlobalLock(hData);
                if (data) {

                    size_t len = wcslen(data);
                    wchar_t *out = malloc((len + 1) * sizeof(wchar_t));
                    if (out) {
                        wcscpy(out, data);

                        applyFilters(out);

                        GlobalUnlock(hData);
                        EmptyClipboard();

                        size_t outSize = (wcslen(out) + 1) * sizeof(wchar_t);
                        HGLOBAL hNew = GlobalAlloc(GMEM_MOVEABLE, outSize);

                        if (hNew) {
                            wchar_t *dst = GlobalLock(hNew);
                            if (dst) {
                                memcpy(dst, out, outSize);
                                GlobalUnlock(hNew);
                                SetClipboardData(CF_UNICODETEXT, hNew);
                            } else {
                                GlobalFree(hNew);
                            }
                        }

                        free(out);
                    }
                }
            }

            CloseClipboard();
        }

        return 0;
    }

    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                // config/edit filters
                AppendMenuW(hMenu, MF_STRING, IDM_EDIT, L"Edit filters");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

                wchar_t label[64];
                if (g_enabled) wcscpy(label, L"Disable"); else wcscpy(label, L"Enable");
                AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE, label);
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

                // See MS docs: set foreground window before TrackPopupMenu
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
        }

        return 0;
    }

    if (msg == WM_COMMAND) {
        switch (LOWORD(wParam)) {
            case IDM_EDIT:
                // Open filter.txt in default editor
                ShellExecuteW(NULL, L"open", L"filter.txt", NULL, NULL, SW_SHOWNORMAL);
                break;
            case IDM_TOGGLE:
                g_enabled = !g_enabled;
                if (g_enabled) {
                    wcscpy(g_nid.szTip, L"Clipboard Filter: On");
                    g_nid.hIcon = g_hIconOn;
                } else {
                    wcscpy(g_nid.szTip, L"Clipboard Filter: Off");
                    g_nid.hIcon = g_hIconOff;
                }
                g_nid.uFlags = NIF_TIP | NIF_ICON;
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
                break;

            case IDM_EXIT:
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                // unregister clipboard listener and quit
                RemoveClipboardFormatListener(hwnd);
                PostQuitMessage(0);
                break;
        }

        return 0;
    }

    if (msg == WM_DESTROY) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        RemoveClipboardFormatListener(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int nCmdShow) {

    loadFilters(); 

    const wchar_t CLASS_NAME[] = L"ClipListenerClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Clipboard Filter",
        0, 0, 0, 0, 0,
        NULL, NULL, hInst, NULL
    );

    // load small icons for tray states
    g_hIconOn = LoadIconW(NULL, IDI_APPLICATION);
    g_hIconOff = LoadIconW(NULL, IDI_WARNING);

    // initialize tray icon data
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = ID_TRAY;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_hIconOn;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    wcscpy(g_nid.szTip, L"Clipboard Filter: On");

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    AddClipboardFormatListener(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

