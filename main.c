#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#ifndef AddClipboardFormatListener
WINUSERAPI BOOL WINAPI AddClipboardFormatListener(HWND hwnd);
#endif

#define MAX_FILTERS 64
#define MAX_FILTER_LEN 256

wchar_t filters[MAX_FILTERS][MAX_FILTER_LEN];
int filterCount = 0;

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
        if (OpenClipboard(hwnd)) {

            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t *data = GlobalLock(hData);
                if (data) {

                    size_t len = wcslen(data);
                    wchar_t *out = malloc((len + 1) * sizeof(wchar_t));
                    wcscpy(out, data);

                    applyFilters(out);

                    GlobalUnlock(hData);
                    EmptyClipboard();

                    size_t outSize = (wcslen(out) + 1) * sizeof(wchar_t);
                    HGLOBAL hNew = GlobalAlloc(GMEM_MOVEABLE, outSize);

                    wchar_t *dst = GlobalLock(hNew);
                    memcpy(dst, out, outSize);
                    GlobalUnlock(hNew);

                    SetClipboardData(CF_UNICODETEXT, hNew);
                    free(out);
                }
            }

            CloseClipboard();
        }
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

    AddClipboardFormatListener(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

