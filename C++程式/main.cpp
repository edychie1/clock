#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cmath>
#include <vector>
#include <set>
#include <algorithm>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

#include "config.h"

using namespace Gdiplus;

// Globals
HINSTANCE g_hInst;
HWND g_hWnd;
Config g_cfg;
ULONG_PTR g_gdiToken;
HFONT g_hFont = NULL;
std::wstring g_displayTime;
std::wstring g_displayDate;
bool g_dragging = false;
POINT g_dragOffset;
HMENU g_hMenu;
bool g_settingsOpen = false;
NOTIFYICONDATAW g_nid;
#define WM_TRAYICON (WM_APP + 1)
#define WM_TRAY_EXIT (WM_APP + 2)
const wchar_t* g_tzList[] = {
    L"Asia/Taipei", L"Asia/Tokyo", L"Asia/Shanghai", L"Asia/Hong_Kong",
    L"Asia/Seoul", L"Asia/Singapore", L"Asia/Kolkata",
    L"America/New_York", L"America/Chicago", L"America/Denver",
    L"America/Los_Angeles", L"America/Sao_Paulo",
    L"Europe/London", L"Europe/Paris", L"Europe/Berlin", L"Europe/Moscow",
    L"Australia/Sydney", L"Australia/Perth",
    L"Pacific/Auckland", L"UTC"
};
const wchar_t* g_tzLabels[] = {
    L"台北 (UTC+8)", L"東京 (UTC+9)", L"上海 (UTC+8)", L"香港 (UTC+8)",
    L"首爾 (UTC+9)", L"新加坡 (UTC+8)", L"加爾各答 (UTC+5:30)",
    L"紐約 (UTC-5)", L"芝加哥 (UTC-6)", L"丹佛 (UTC-7)",
    L"洛杉磯 (UTC-8)", L"聖保羅 (UTC-3)",
    L"倫敦 (UTC+0)", L"巴黎 (UTC+1)", L"柏林 (UTC+1)", L"莫斯科 (UTC+3)",
    L"雪梨 (UTC+11)", L"伯斯 (UTC+8)",
    L"奧克蘭 (UTC+13)", L"UTC"
};
const int g_tzCount = 20;

// UTC offset in hours for each timezone (standard time, no DST)
static int GetTzOffsetHours(int idx) {
    static const int offsets[] = {
        8, 9, 8, 8,       // Taipei, Tokyo, Shanghai, Hong_Kong
        9, 8, 5,           // Seoul, Singapore, Kolkata (truncated from 5.5)
        -5, -6, -7, -5,    // New_York, Chicago, Denver, Los_Angeles
        -3,                // Sao_Paulo
        0, 1, 1, 3,        // London, Paris, Berlin, Moscow
        11, 8,             // Sydney, Perth
        13, 0              // Auckland, UTC
    };
    if (idx >= 0 && idx < g_tzCount) return offsets[idx];
    return 8; // Default to Taipei
}

static int GetTzIndexByName(const std::wstring& tz) {
    for (int i = 0; i < g_tzCount; i++)
        if (tz == g_tzList[i]) return i;
    return 0; // Default to Taipei
}

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SettingsWndProc(HWND, UINT, WPARAM, LPARAM);

static void TrayAdd() {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
    wcscpy_s(g_nid.szTip, L"AIClock");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void TrayRemove() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static COLORREF HexToColorRef(const std::wstring& hex) {
    if (hex.empty() || hex[0] != L'#') return RGB(0, 0, 0);
    int r = 0, g = 0, b = 0;
    swscanf_s(hex.c_str() + 1, L"%2x%2x%2x", &r, &g, &b);
    return RGB(r, g, b);
}

static std::wstring ColorRefToHex(COLORREF cr) {
    wchar_t buf[16];
    swprintf_s(buf, L"#%02X%02X%02X", GetRValue(cr), GetGValue(cr), GetBValue(cr));
    return buf;
}

struct ClockLayout {
    int timeTop, timeBottom;
    float lineY;
    int dateTop, dateBottom;
    int secondsSize;
    int height;
};

static ClockLayout ComputeLayout(int fontSize, int secondsPct = 60, int windowH = 0, int hourSize = 0, int minuteSize = 0) {
    ClockLayout L{};
    int dateSize = std::max(12, (int)(fontSize * 0.45));
    L.secondsSize = std::max(8, (int)(fontSize * secondsPct / 100));

    int tlGap = g_cfg.time_line_gap;
    int ldGap = g_cfg.line_date_gap;

    int effectiveHour = std::max(8, (int)(fontSize * hourSize / 100));
    int effectiveMin = std::max(8, (int)(fontSize * minuteSize / 100));
    int maxTimeSize = std::max(effectiveHour, effectiveMin);
    int dateTextRenderHeight = dateSize;

    // Content spans from text edge to line (includes the gap)
    int spanAbove = maxTimeSize + tlGap;
    int spanBelow = ldGap + dateTextRenderHeight;

    // Symmetric padding around each content span
    int padding = std::max(12, (int)(std::max(maxTimeSize, dateTextRenderHeight) * 0.12));

    // Each half (above/below line) must be equally tall for the line to be centered
    int halfHeight = padding + std::max(spanAbove, spanBelow);

    L.height = halfHeight + 1 + halfHeight;
    L.lineY = (float)halfHeight + 0.5f;

    // Time bottom sits tlGap above the line top edge
    L.timeBottom = (int)(L.lineY - 0.5f - tlGap);
    L.timeTop = L.timeBottom - maxTimeSize;

    // Date top sits ldGap below the line bottom edge
    L.dateTop = (int)(L.lineY + 0.5f + ldGap);
    L.dateBottom = L.dateTop + dateTextRenderHeight;

    return L;
}

static int CalcHeight(int fontSize) {
    return ComputeLayout(fontSize, g_cfg.seconds_size, 0, g_cfg.hour_size, g_cfg.minute_size).height;
}

static void UpdateClockText() {
    int tzIdx = GetTzIndexByName(g_cfg.timezone);
    int offsetSec = GetTzOffsetHours(tzIdx) * 3600;

    time_t now = time(NULL);
    now += offsetSec;
    struct tm t;
    gmtime_s(&t, &now);

    wchar_t buf[64];
    wcsftime(buf, 64, L"%H:%M:%S", &t);
    g_displayTime = buf;

    // Day-of-week in English
    static const wchar_t* wdays[] = {
        L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday"
    };
    wchar_t dateBuf[64];
    swprintf_s(dateBuf, L"%d-%d-%d %s", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, wdays[t.tm_wday]);
    g_displayDate = dateBuf;
}

static void RecreateFont() {
    if (g_hFont) DeleteObject(g_hFont);
    g_hFont = CreateFontW(-g_cfg.font_size, 0, 0, 0, g_cfg.font_weight,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, g_cfg.font_family.c_str());
}

static void ApplyWindowStyleFromConfig() {
    double op = g_cfg.opacity;
    if (op < 0.2) op = 0.2;
    if (op > 1.0) op = 1.0;
    if (g_cfg.bg_type == L"transparent") {
        COLORREF key = HexToColorRef(g_cfg.bg_color);
        SetLayeredWindowAttributes(g_hWnd, key, (BYTE)(op * 255), LWA_COLORKEY | LWA_ALPHA);
    } else {
        SetLayeredWindowAttributes(g_hWnd, 0, (BYTE)(op * 255), LWA_ALPHA);
    }
    int w = g_cfg.width;
    int h = CalcHeight(g_cfg.font_size);
    // Use current window position, not g_cfg.x/y (which may be stale)
    RECT wr;
    GetWindowRect(g_hWnd, &wr);
    SetWindowPos(g_hWnd, NULL, wr.left, wr.top, w, h, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREPOSITION);
    InvalidateRect(g_hWnd, NULL, TRUE);
}

static COLORREF ContrastColor(COLORREF fg, COLORREF bg) {
    int dr = GetRValue(fg) - GetRValue(bg);
    int dg = GetGValue(fg) - GetGValue(bg);
    int db = GetBValue(fg) - GetBValue(bg);
    if (dr*dr + dg*dg + db*db < 30000) {
        if (GetRValue(bg) + GetGValue(bg) + GetBValue(bg) > 384)
            return RGB(0, 0, 0);
        else
            return RGB(255, 255, 255);
    }
    return fg;
}

// Settings window control IDs
enum {
    ID_WIDTH_SLIDER = 3001,
    ID_WIDTH_VAL,
    ID_FONTSZ_SLIDER,
    ID_FONTSZ_VAL,
    ID_FONT_FAMILY,
    ID_FONT_COLOR,
    ID_COLOR_PREVIEW,
    ID_WEIGHT_NORMAL,
    ID_WEIGHT_BOLD,
    ID_OPACITY_SLIDER,
    ID_OPACITY_VAL,
    ID_SEC_SLIDER,
    ID_SEC_VAL,
    ID_SEC_GAP_SLIDER,
    ID_SEC_GAP_VAL,
    ID_TL_GAP_SLIDER,
    ID_TL_GAP_VAL,
    ID_LD_GAP_SLIDER,
    ID_LD_GAP_VAL,
    ID_LW_SLIDER,
    ID_LW_VAL,
    ID_LINE_COLOR,
    ID_LINE_COLOR_PREVIEW,
    ID_HOUR_SLIDER,
    ID_HOUR_VAL,
    ID_MINUTE_SLIDER,
    ID_MINUTE_VAL,
    ID_HM_GAP_SLIDER,
    ID_HM_GAP_VAL,
    ID_BG_TRANSPARENT,
    ID_BG_IMAGE,
    ID_BG_IMAGE_PATH,
    ID_BG_IMAGE_BROWSE,
    ID_TZ_COMBO,
    ID_AUTO_START,
    ID_SAVE,
    ID_CANCEL
};

static void ResetGapsToDefaultProportions(int newSize) {
    const int DEF_FONT = 40;
    if (newSize <= 0) return;
    double r = (double)newSize / (double)DEF_FONT;
    auto sc = [&](int defVal) -> int { return (int)round(defVal * r); };
    g_cfg.hour_minute_gap = sc(0);
    g_cfg.seconds_gap = sc(-4);
    // time_line_gap and line_date_gap are fixed pixel values, not scaled
}

static void SyncScaledUI(HWND hDlg) {
    wchar_t b[32];
    SendDlgItemMessageW(hDlg, ID_HOUR_SLIDER, TBM_SETPOS, TRUE, g_cfg.hour_size);
    swprintf_s(b, L"%d", g_cfg.hour_size); SetDlgItemTextW(hDlg, ID_HOUR_VAL, b);
    SendDlgItemMessageW(hDlg, ID_MINUTE_SLIDER, TBM_SETPOS, TRUE, g_cfg.minute_size);
    swprintf_s(b, L"%d", g_cfg.minute_size); SetDlgItemTextW(hDlg, ID_MINUTE_VAL, b);
    SendDlgItemMessageW(hDlg, ID_HM_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.hour_minute_gap);
    swprintf_s(b, L"%d", g_cfg.hour_minute_gap); SetDlgItemTextW(hDlg, ID_HM_GAP_VAL, b);
    SendDlgItemMessageW(hDlg, ID_SEC_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.seconds_gap);
    swprintf_s(b, L"%d", g_cfg.seconds_gap); SetDlgItemTextW(hDlg, ID_SEC_GAP_VAL, b);
    SendDlgItemMessageW(hDlg, ID_TL_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.time_line_gap);
    swprintf_s(b, L"%d", g_cfg.time_line_gap); SetDlgItemTextW(hDlg, ID_TL_GAP_VAL, b);
    SendDlgItemMessageW(hDlg, ID_LD_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.line_date_gap);
    swprintf_s(b, L"%d", g_cfg.line_date_gap); SetDlgItemTextW(hDlg, ID_LD_GAP_VAL, b);
}

static void PreviewUpdate() {
    RecreateFont();
    int w = g_cfg.width;
    int h = CalcHeight(g_cfg.font_size);
    // Use current window position, not g_cfg.x/y
    RECT wr;
    GetWindowRect(g_hWnd, &wr);
    SetWindowPos(g_hWnd, NULL, wr.left, wr.top, w, h, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREPOSITION);
    UpdateClockText();
    InvalidateRect(g_hWnd, NULL, TRUE);
}

static void Settings_ApplyToClock() {
    ApplyWindowStyleFromConfig();
    RecreateFont();
    UpdateClockText();
}

LRESULT CALLBACK SettingsWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_settingsOpen = true;

        auto lbl = [&](int id, const wchar_t* t, int x, int y, int w) {
            return CreateWindowW(L"STATIC", t, WS_VISIBLE | WS_CHILD | SS_LEFT, x, y, w, 20, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto trk = [&](int id, int x, int y, int w) {
            return CreateWindowW(TRACKBAR_CLASSW, NULL, WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS, x, y, w, 24, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto edt = [&](int id, int x, int y, int w) {
            return CreateWindowW(L"EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_READONLY, x, y, w, 22, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto numEdt = [&](int id, int x, int y, int w) {
            return CreateWindowW(L"EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_NUMBER, x, y, w, 22, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto btn = [&](int id, const wchar_t* t, int x, int y, int w, int h) {
            return CreateWindowW(L"BUTTON", t, WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x, y, w, h, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto rad = [&](int id, const wchar_t* t, int x, int y, int w, int h) {
            return CreateWindowW(L"BUTTON", t, WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, x, y, w, h, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto chk = [&](int id, const wchar_t* t, int x, int y, int w, int h) {
            return CreateWindowW(L"BUTTON", t, WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, x, y, w, h, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto cmb = [&](int id, int x, int y, int w, int h, bool sorted) {
            DWORD style = WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL;
            if (sorted) style |= CBS_SORT;
            return CreateWindowW(L"COMBOBOX", NULL, style, x, y, w, h, hDlg, (HMENU)(INT_PTR)id, g_hInst, NULL);
        };
        auto grp = [&](const wchar_t* t, int x, int y, int w, int h) {
            return CreateWindowW(L"BUTTON", t, WS_VISIBLE | WS_CHILD | BS_GROUPBOX, x, y, w, h, hDlg, NULL, g_hInst, NULL);
        };

        const int LX = 16, RX = 370, SLIDER_X = 120, SLIDER_W = 230, INPUT_W = 55;
        int y = 10, step = 28;
        wchar_t buf[32];

        // ===== Group: Font =====
        grp(L"字型", 6, y, 440, 148);
        y += 18;

        lbl(0, L"字型大小", LX, y, 95);
        trk(ID_FONTSZ_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_FONTSZ_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_FONTSZ_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(12, 240));
        SendDlgItemMessageW(hDlg, ID_FONTSZ_SLIDER, TBM_SETPOS, TRUE, g_cfg.font_size);
        swprintf_s(buf, L"%d", g_cfg.font_size); SetDlgItemTextW(hDlg, ID_FONTSZ_VAL, buf);

        y += step; lbl(0, L"字體", LX, y, 95);
        HWND hFam = cmb(ID_FONT_FAMILY, SLIDER_X, y, 305, 200, true);
        LOGFONTW lf = { 0 }; lf.lfCharSet = DEFAULT_CHARSET;
        struct FE { HWND c; std::wstring s; std::set<std::wstring> seen; } fe = { hFam, g_cfg.font_family, {} };
        EnumFontFamiliesExW(GetDC(hDlg), &lf, [](const LOGFONTW* f, const TEXTMETRICW*, DWORD, LPARAM p) -> int {
            if (f->lfFaceName[0] != L'@') {
                FE* fe = (FE*)p;
                std::wstring name = f->lfFaceName;
                if (fe->seen.find(name) == fe->seen.end()) {
                    fe->seen.insert(name);
                    SendMessageW(fe->c, CB_ADDSTRING, 0, (LPARAM)name.c_str());
                }
            }
            return 1; }, (LPARAM)&fe, 0);
        int idx = (int)SendMessageW(hFam, CB_FINDSTRINGEXACT, 0, (LPARAM)g_cfg.font_family.c_str());
        SendMessageW(hFam, CB_SETCURSEL, idx == CB_ERR ? 0 : idx, 0);

        y += step; lbl(0, L"字型顏色", LX, y, 95);
        HWND hClrPv = CreateWindowW(L"STATIC", NULL, WS_VISIBLE | WS_CHILD | SS_OWNERDRAW, SLIDER_X, y, 40, 22, hDlg, (HMENU)(INT_PTR)ID_COLOR_PREVIEW, g_hInst, NULL);
        SetWindowLongPtrW(hClrPv, GWLP_USERDATA, (LONG_PTR)HexToColorRef(g_cfg.font_color));
        btn(ID_FONT_COLOR, L"選擇顏色", SLIDER_X + 46, y, 80, 22);
        CreateWindowW(L"EDIT", g_cfg.font_color.c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_READONLY, SLIDER_X + 132, y, 80, 22, hDlg, (HMENU)(INT_PTR)301, g_hInst, NULL);

        y += step; lbl(0, L"字體粗細", LX, y, 95);
        rad(ID_WEIGHT_NORMAL, L"細體", SLIDER_X, y, 60, 22);
        rad(ID_WEIGHT_BOLD, L"粗體", SLIDER_X + 65, y, 60, 22);
        SendDlgItemMessageW(hDlg, g_cfg.font_weight >= 600 ? ID_WEIGHT_BOLD : ID_WEIGHT_NORMAL, BM_SETCHECK, BST_CHECKED, 0);

        // ===== Group: Appearance =====
        y = 168;
        grp(L"外觀", 6, y, 440, 196);
        y += 18;

        lbl(0, L"透明度", LX, y, 95);
        trk(ID_OPACITY_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_OPACITY_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_OPACITY_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(10, 100));
        SendDlgItemMessageW(hDlg, ID_OPACITY_SLIDER, TBM_SETPOS, TRUE, (int)(g_cfg.opacity * 100));
        swprintf_s(buf, L"%d", (int)(g_cfg.opacity * 100)); SetDlgItemTextW(hDlg, ID_OPACITY_VAL, buf);

        y += step; lbl(0, L"時的大小 %", LX, y, 95);
        trk(ID_HOUR_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_HOUR_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_HOUR_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(10, 300));
        SendDlgItemMessageW(hDlg, ID_HOUR_SLIDER, TBM_SETPOS, TRUE, g_cfg.hour_size);
        swprintf_s(buf, L"%d", g_cfg.hour_size); SetDlgItemTextW(hDlg, ID_HOUR_VAL, buf);

        y += step; lbl(0, L"分的大小 %", LX, y, 95);
        trk(ID_MINUTE_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_MINUTE_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_MINUTE_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(10, 300));
        SendDlgItemMessageW(hDlg, ID_MINUTE_SLIDER, TBM_SETPOS, TRUE, g_cfg.minute_size);
        swprintf_s(buf, L"%d", g_cfg.minute_size); SetDlgItemTextW(hDlg, ID_MINUTE_VAL, buf);

        y += step; lbl(0, L"秒數大小 %", LX, y, 95);
        trk(ID_SEC_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_SEC_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_SEC_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(10, 100));
        SendDlgItemMessageW(hDlg, ID_SEC_SLIDER, TBM_SETPOS, TRUE, g_cfg.seconds_size);
        swprintf_s(buf, L"%d", g_cfg.seconds_size); SetDlgItemTextW(hDlg, ID_SEC_VAL, buf);

        y += step; lbl(0, L"中線寬度", LX, y, 95);
        trk(ID_LW_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_LW_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_LW_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(100, 1500));
        SendDlgItemMessageW(hDlg, ID_LW_SLIDER, TBM_SETPOS, TRUE, g_cfg.line_width);
        swprintf_s(buf, L"%d", g_cfg.line_width); SetDlgItemTextW(hDlg, ID_LW_VAL, buf);

        y += step; lbl(0, L"中線顏色", LX, y, 95);
        HWND hLineClrPv = CreateWindowW(L"STATIC", NULL, WS_VISIBLE | WS_CHILD | SS_OWNERDRAW, SLIDER_X, y, 40, 22, hDlg, (HMENU)(INT_PTR)ID_LINE_COLOR_PREVIEW, g_hInst, NULL);
        SetWindowLongPtrW(hLineClrPv, GWLP_USERDATA, (LONG_PTR)HexToColorRef(g_cfg.line_color));
        btn(ID_LINE_COLOR, L"選擇顏色", SLIDER_X + 46, y, 80, 22);
        CreateWindowW(L"EDIT", g_cfg.line_color.c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_READONLY, SLIDER_X + 132, y, 80, 22, hDlg, (HMENU)(INT_PTR)302, g_hInst, NULL);

        // ===== Group: Spacing =====
        y = 374;
        grp(L"間距", 6, y, 440, 140);
        y += 18;

        lbl(0, L"時/分距離", LX, y, 95);
        trk(ID_HM_GAP_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_HM_GAP_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_HM_GAP_SLIDER, TBM_SETRANGE, TRUE, MAKELONG((WORD)(SHORT)(-90), 100));
        SendDlgItemMessageW(hDlg, ID_HM_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.hour_minute_gap);
        swprintf_s(buf, L"%d", g_cfg.hour_minute_gap); SetDlgItemTextW(hDlg, ID_HM_GAP_VAL, buf);

        y += step; lbl(0, L"秒/分距離", LX, y, 95);
        trk(ID_SEC_GAP_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_SEC_GAP_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_SEC_GAP_SLIDER, TBM_SETRANGE, TRUE, MAKELONG((WORD)(SHORT)(-120), 50));
        SendDlgItemMessageW(hDlg, ID_SEC_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.seconds_gap);
        swprintf_s(buf, L"%d", g_cfg.seconds_gap); SetDlgItemTextW(hDlg, ID_SEC_GAP_VAL, buf);

        y += step; lbl(0, L"時間/中線", LX, y, 95);
        trk(ID_TL_GAP_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_TL_GAP_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_TL_GAP_SLIDER, TBM_SETRANGE, TRUE, MAKELONG((WORD)(SHORT)(-20), 50));
        SendDlgItemMessageW(hDlg, ID_TL_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.time_line_gap);
        swprintf_s(buf, L"%d", g_cfg.time_line_gap); SetDlgItemTextW(hDlg, ID_TL_GAP_VAL, buf);

        y += step; lbl(0, L"中線/日期", LX, y, 95);
        trk(ID_LD_GAP_SLIDER, SLIDER_X, y - 4, SLIDER_W);
        numEdt(ID_LD_GAP_VAL, RX, y, INPUT_W);
        SendDlgItemMessageW(hDlg, ID_LD_GAP_SLIDER, TBM_SETRANGE, TRUE, MAKELONG((WORD)(SHORT)(-20), 50));
        SendDlgItemMessageW(hDlg, ID_LD_GAP_SLIDER, TBM_SETPOS, TRUE, g_cfg.line_date_gap);
        swprintf_s(buf, L"%d", g_cfg.line_date_gap); SetDlgItemTextW(hDlg, ID_LD_GAP_VAL, buf);

        // ===== Group: Background & System =====
        y = 524;
        grp(L"背景與系統", 6, y, 440, 140);
        y += 18;

        lbl(0, L"背景設定", LX, y, 95);
        rad(ID_BG_TRANSPARENT, L"透明背景", SLIDER_X, y, 90, 22);
        rad(ID_BG_IMAGE, L"自訂圖片", SLIDER_X + 95, y, 90, 22);
        bool isImg = g_cfg.bg_type == L"image";
        SendDlgItemMessageW(hDlg, isImg ? ID_BG_IMAGE : ID_BG_TRANSPARENT, BM_SETCHECK, BST_CHECKED, 0);

        y += step;
        HWND hImgPath = CreateWindowW(L"EDIT", g_cfg.bg_image.c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_READONLY, SLIDER_X, y, 250, 22, hDlg, (HMENU)(INT_PTR)ID_BG_IMAGE_PATH, g_hInst, NULL);
        btn(ID_BG_IMAGE_BROWSE, L"瀏覽", SLIDER_X + 256, y, 60, 22);
        EnableWindow(hImgPath, isImg ? TRUE : FALSE);
        EnableWindow(GetDlgItem(hDlg, ID_BG_IMAGE_BROWSE), isImg ? TRUE : FALSE);

        y += step; lbl(0, L"時區", LX, y, 95);
        HWND hTz = cmb(ID_TZ_COMBO, SLIDER_X, y, 305, 200, false);
        for (int i = 0; i < g_tzCount; i++) SendMessageW(hTz, CB_ADDSTRING, 0, (LPARAM)g_tzLabels[i]);
        SendMessageW(hTz, CB_SETCURSEL, GetTzIndexByName(g_cfg.timezone), 0);

        y += step;
        chk(ID_AUTO_START, L"開機自動啟動", SLIDER_X, y, 200, 22);
        bool autoEnabled = g_cfg.IsAutoStartEnabled();
        g_cfg.auto_start = autoEnabled;
        if (autoEnabled) SendDlgItemMessageW(hDlg, ID_AUTO_START, BM_SETCHECK, BST_CHECKED, 0);

        // Buttons
        y = 676;
        btn(ID_SAVE, L"儲存", 150, y, 80, 28);
        btn(ID_CANCEL, L"取消", 250, y, 80, 28);

        // Copyright
        CreateWindowW(L"STATIC", L"\u00A9 2026 by \u674E\u5955\u7948 is licensed under CC BY-NC-ND 4.0",
            WS_VISIBLE | WS_CHILD | SS_CENTER, 0, y + 38, 452, 20, hDlg, NULL, g_hInst, NULL);

        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (code == EN_KILLFOCUS) {
            wchar_t txt[32]; GetDlgItemTextW(hDlg, id, txt, 32);
            int v = _wtoi(txt);
            auto clampAndSet = [&](int minV, int maxV, int& cfgRef, int sliderId) {
                if (v < minV) v = minV; if (v > maxV) v = maxV;
                cfgRef = v;
                SendDlgItemMessageW(hDlg, sliderId, TBM_SETPOS, TRUE, v);
                wchar_t b[32]; swprintf_s(b, L"%d", v); SetDlgItemTextW(hDlg, id, b);
                PreviewUpdate();
            };
            switch (id) {
            case ID_FONTSZ_VAL: {
                if (v < 12) v = 12; if (v > 240) v = 240;
                ResetGapsToDefaultProportions(v);
                g_cfg.font_size = v;
                int newW = v * 6; if (newW < 120) newW = 120; if (newW > 2000) newW = 2000;
                int newLW = v * 5; if (newLW < 100) newLW = 100; if (newLW > 1500) newLW = 1500;
                if (newW < newLW + 20) newW = newLW + 20;
                g_cfg.width = newW; g_cfg.line_width = newLW;
                SendDlgItemMessageW(hDlg, ID_FONTSZ_SLIDER, TBM_SETPOS, TRUE, v);
                SendDlgItemMessageW(hDlg, ID_LW_SLIDER, TBM_SETPOS, TRUE, newLW);
                wchar_t b[32]; swprintf_s(b, L"%d", newLW); SetDlgItemTextW(hDlg, ID_LW_VAL, b);
                SyncScaledUI(hDlg);
                PreviewUpdate(); break;
            }
            case ID_OPACITY_VAL: {
                if (v < 10) v = 10; if (v > 100) v = 100;
                g_cfg.opacity = v / 100.0;
                SendDlgItemMessageW(hDlg, ID_OPACITY_SLIDER, TBM_SETPOS, TRUE, v);
                double opv = v / 100.0; if (opv < 0.2) opv = 0.2; if (opv > 1.0) opv = 1.0;
                if (g_cfg.bg_type == L"transparent")
                    SetLayeredWindowAttributes(g_hWnd, HexToColorRef(g_cfg.bg_color), (BYTE)(opv * 255), LWA_COLORKEY | LWA_ALPHA);
                else
                    SetLayeredWindowAttributes(g_hWnd, 0, (BYTE)(opv * 255), LWA_ALPHA);
                InvalidateRect(g_hWnd, NULL, TRUE); break;
            }
            case ID_SEC_VAL:     clampAndSet(10, 100, g_cfg.seconds_size, ID_SEC_SLIDER); break;
            case ID_SEC_GAP_VAL: clampAndSet(-120, 50, g_cfg.seconds_gap, ID_SEC_GAP_SLIDER); break;
            case ID_TL_GAP_VAL:  clampAndSet(-20, 50, g_cfg.time_line_gap, ID_TL_GAP_SLIDER); break;
            case ID_LD_GAP_VAL:  clampAndSet(-20, 50, g_cfg.line_date_gap, ID_LD_GAP_SLIDER); break;
            case ID_LW_VAL: {
                if (v < 100) v = 100; if (v > 1500) v = 1500;
                g_cfg.line_width = v;
                if (v + 20 > g_cfg.width) g_cfg.width = v + 20;
                SendDlgItemMessageW(hDlg, ID_LW_SLIDER, TBM_SETPOS, TRUE, v);
                wchar_t b[32]; swprintf_s(b, L"%d", v); SetDlgItemTextW(hDlg, id, b);
                PreviewUpdate(); break;
            }
            case ID_HOUR_VAL:    clampAndSet(10, 300, g_cfg.hour_size, ID_HOUR_SLIDER); break;
            case ID_MINUTE_VAL:  clampAndSet(10, 300, g_cfg.minute_size, ID_MINUTE_SLIDER); break;
            case ID_HM_GAP_VAL:  clampAndSet(-90, 100, g_cfg.hour_minute_gap, ID_HM_GAP_SLIDER); break;
            }
            return 0;
        }

        if (id == ID_SAVE) {
            // Read bg_image path from edit control
            wchar_t imgPath[MAX_PATH] = { 0 };
            GetDlgItemTextW(hDlg, ID_BG_IMAGE_PATH, imgPath, MAX_PATH);
            g_cfg.bg_image = imgPath;

            int autoSt = (int)SendDlgItemMessageW(hDlg, ID_AUTO_START, BM_GETCHECK, 0, 0);
            bool autoOk = g_cfg.SetAutoStart(autoSt != 0);
            g_cfg.auto_start = (autoSt != 0);
            if (!autoOk) {
                MessageBoxW(hDlg, L"無法寫入登錄檔，開機自動啟動設定可能沒有生效。",
                    L"AIClock", MB_OK | MB_ICONWARNING);
            }
            g_cfg.Save();
            Settings_ApplyToClock();
            DestroyWindow(hDlg); g_settingsOpen = false;
            return 0;
        }
        if (id == ID_CANCEL) {
            g_cfg.Load();
            Settings_ApplyToClock();
            DestroyWindow(hDlg); g_settingsOpen = false;
            return 0;
        }

        if (id == ID_BG_TRANSPARENT && code == BN_CLICKED) {
            EnableWindow(GetDlgItem(hDlg, ID_BG_IMAGE_PATH), FALSE);
            EnableWindow(GetDlgItem(hDlg, ID_BG_IMAGE_BROWSE), FALSE);
            g_cfg.bg_type = L"transparent";
            ApplyWindowStyleFromConfig();
        }
        if (id == ID_BG_IMAGE && code == BN_CLICKED) {
            EnableWindow(GetDlgItem(hDlg, ID_BG_IMAGE_PATH), TRUE);
            EnableWindow(GetDlgItem(hDlg, ID_BG_IMAGE_BROWSE), TRUE);
            g_cfg.bg_type = L"image";
            ApplyWindowStyleFromConfig();
        }
        if (id == ID_BG_IMAGE_BROWSE && code == BN_CLICKED) {
            wchar_t path[MAX_PATH] = { 0 };
            OPENFILENAMEW ofn = { sizeof(ofn), hDlg };
            ofn.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp\0\0";
            ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
            if (GetOpenFileNameW(&ofn)) {
                SetDlgItemTextW(hDlg, ID_BG_IMAGE_PATH, path);
                g_cfg.bg_image = path;
                ApplyWindowStyleFromConfig();
            }
        }
        if (id == ID_LINE_COLOR && code == BN_CLICKED) {
            wchar_t hex[16]; GetDlgItemTextW(hDlg, 302, hex, 16);
            COLORREF cur = HexToColorRef(hex);
            CHOOSECOLORW cc = { sizeof(cc), hDlg };
            static COLORREF cust[16] = { 0 };
            cc.rgbResult = cur; cc.lpCustColors = cust;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if (ChooseColorW(&cc)) {
                SetDlgItemTextW(hDlg, 302, ColorRefToHex(cc.rgbResult).c_str());
                SetWindowLongPtrW(GetDlgItem(hDlg, ID_LINE_COLOR_PREVIEW), GWLP_USERDATA, (LONG_PTR)cc.rgbResult);
                InvalidateRect(GetDlgItem(hDlg, ID_LINE_COLOR_PREVIEW), NULL, TRUE);
                g_cfg.line_color = ColorRefToHex(cc.rgbResult);
                InvalidateRect(g_hWnd, NULL, TRUE);
            }
        }
        if (id == ID_FONT_COLOR && code == BN_CLICKED) {
            wchar_t hex[16]; GetDlgItemTextW(hDlg, 301, hex, 16);
            COLORREF cur = HexToColorRef(hex);
            CHOOSECOLORW cc = { sizeof(cc), hDlg };
            static COLORREF cust[16] = { 0 };
            cc.rgbResult = cur; cc.lpCustColors = cust;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if (ChooseColorW(&cc)) {
                SetDlgItemTextW(hDlg, 301, ColorRefToHex(cc.rgbResult).c_str());
                SetWindowLongPtrW(GetDlgItem(hDlg, ID_COLOR_PREVIEW), GWLP_USERDATA, (LONG_PTR)cc.rgbResult);
                InvalidateRect(GetDlgItem(hDlg, ID_COLOR_PREVIEW), NULL, TRUE);
                g_cfg.font_color = ColorRefToHex(cc.rgbResult);
                InvalidateRect(g_hWnd, NULL, TRUE);
            }
        }
        if (id == ID_FONT_FAMILY && code == CBN_SELCHANGE) {
            HWND hC = GetDlgItem(hDlg, ID_FONT_FAMILY);
            int s = (int)SendMessageW(hC, CB_GETCURSEL, 0, 0);
            if (s >= 0) {
                wchar_t fam[LF_FACESIZE]; SendMessageW(hC, CB_GETLBTEXT, s, (LPARAM)fam);
                g_cfg.font_family = fam;
                RecreateFont(); InvalidateRect(g_hWnd, NULL, TRUE);
            }
        }
        if ((id == ID_WEIGHT_NORMAL || id == ID_WEIGHT_BOLD) && code == BN_CLICKED) {
            g_cfg.font_weight = (id == ID_WEIGHT_BOLD) ? 700 : 400;
            RecreateFont(); InvalidateRect(g_hWnd, NULL, TRUE);
        }
        if (id == ID_TZ_COMBO && code == CBN_SELCHANGE) {
            int s = (int)SendDlgItemMessageW(hDlg, ID_TZ_COMBO, CB_GETCURSEL, 0, 0);
            if (s >= 0 && s < g_tzCount) {
                g_cfg.timezone = g_tzList[s];
                UpdateClockText(); InvalidateRect(g_hWnd, NULL, TRUE);
            }
        }
        return 0;
    }
    case WM_HSCROLL: {
        HWND hs = (HWND)lParam;
        int id = GetDlgCtrlID(hs), val; wchar_t b[32];
        if (id == ID_FONTSZ_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_FONTSZ_VAL, b);
            int oldSize = g_cfg.font_size;
            int newW = val * 6;
            if (newW < 120) newW = 120;
            if (newW > 2000) newW = 2000;
            int newLW = val * 5;
            if (newLW < 100) newLW = 100;
            if (newLW > 1500) newLW = 1500;
            if (newW < newLW + 20) newW = newLW + 20;
            ResetGapsToDefaultProportions(val);
            g_cfg.font_size = val; g_cfg.width = newW; g_cfg.line_width = newLW;
            SendDlgItemMessageW(hDlg, ID_LW_SLIDER, TBM_SETPOS, TRUE, newLW);
            swprintf_s(b, L"%d", newLW); SetDlgItemTextW(hDlg, ID_LW_VAL, b);
            SyncScaledUI(hDlg);
            PreviewUpdate();
        } else if (id == ID_OPACITY_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_OPACITY_VAL, b);
            g_cfg.opacity = val / 100.0;
            double opv = val / 100.0; if (opv < 0.2) opv = 0.2; if (opv > 1.0) opv = 1.0;
            if (g_cfg.bg_type == L"transparent") {
                SetLayeredWindowAttributes(g_hWnd, HexToColorRef(g_cfg.bg_color), (BYTE)(opv * 255), LWA_COLORKEY | LWA_ALPHA);
            } else {
                SetLayeredWindowAttributes(g_hWnd, 0, (BYTE)(opv * 255), LWA_ALPHA);
            }
            InvalidateRect(g_hWnd, NULL, TRUE);
        } else if (id == ID_SEC_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_SEC_VAL, b);
            g_cfg.seconds_size = val;
            PreviewUpdate();
        } else if (id == ID_SEC_GAP_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_SEC_GAP_VAL, b);
            g_cfg.seconds_gap = val;
            PreviewUpdate();
        } else if (id == ID_TL_GAP_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_TL_GAP_VAL, b);
            g_cfg.time_line_gap = val;
            PreviewUpdate();
        } else if (id == ID_LD_GAP_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_LD_GAP_VAL, b);
            g_cfg.line_date_gap = val;
            PreviewUpdate();
        } else if (id == ID_LW_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_LW_VAL, b);
            g_cfg.line_width = val;
            if (val + 20 > g_cfg.width) g_cfg.width = val + 20;
            PreviewUpdate();
        } else if (id == ID_HOUR_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_HOUR_VAL, b);
            g_cfg.hour_size = val;
            PreviewUpdate();
        } else if (id == ID_MINUTE_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_MINUTE_VAL, b);
            g_cfg.minute_size = val;
            PreviewUpdate();
        } else if (id == ID_HM_GAP_SLIDER) {
            val = (int)SendMessageW(hs, TBM_GETPOS, 0, 0);
            swprintf_s(b, L"%d", val); SetDlgItemTextW(hDlg, ID_HM_GAP_VAL, b);
            g_cfg.hour_minute_gap = val;
            PreviewUpdate();
        }
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lParam;
        if (di->CtlID == ID_COLOR_PREVIEW || di->CtlID == ID_LINE_COLOR_PREVIEW) {
            COLORREF cr = (COLORREF)GetWindowLongPtrW(di->hwndItem, GWLP_USERDATA);
            HBRUSH br = CreateSolidBrush(cr);
            RECT r; GetClientRect(di->hwndItem, &r); InflateRect(&r, -1, -1);
            FillRect(di->hDC, &r, br); DeleteObject(br);
            FrameRect(di->hDC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return TRUE;
        }
        return FALSE;
    }
    case WM_CLOSE:
        DestroyWindow(hDlg); g_settingsOpen = false;
        return 0;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

static void CreateSettingsWindow(HWND hParent) {
    if (g_settingsOpen) {
        HWND hE = FindWindowW(L"AIClockSettings", NULL);
        if (hE) { SetForegroundWindow(hE); return; }
        g_settingsOpen = false;
    }
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"AIClockSettings", L"AIClock Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 468, 750,
        hParent, NULL, g_hInst, NULL);
    if (hDlg) {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        RECT dr; GetWindowRect(hDlg, &dr);
        int dlgW = dr.right - dr.left;
        int dlgH = dr.bottom - dr.top;
        int x = (screenW - dlgW) / 2;
        int y = (screenH - dlgH) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);
    }
}

static int RegSettingsClass() {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"AIClockSettings";
    return RegisterClassW(&wc) ? 1 : (GetLastError() == ERROR_CLASS_ALREADY_EXISTS ? 1 : 0);
}

static void ShowFontDialog(HWND hParent) {
    LOGFONTW lf = { 0 };
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, g_cfg.font_family.c_str());
    lf.lfHeight = -g_cfg.font_size;
    lf.lfWeight = g_cfg.font_weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;

    CHOOSEFONTW cf = { sizeof(cf), hParent, NULL, &lf,
        CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS | CF_FORCEFONTEXIST,
        0, 0, NULL, NULL, NULL };

    if (ChooseFontW(&cf)) {
        g_cfg.font_family = lf.lfFaceName;
        g_cfg.font_size = abs(cf.iPointSize) / 10;
        g_cfg.font_weight = lf.lfWeight;
        COLORREF c = cf.rgbColors;
        g_cfg.font_color = ColorRefToHex(c);
        RecreateFont();
        ApplyWindowStyleFromConfig();
        g_cfg.Save();
    }
}

static void DrawClock(HDC hdc, RECT* rc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;

    // Background image overlay
    if (g_cfg.bg_type == L"image" && !g_cfg.bg_image.empty()) {
        Image* img = Image::FromFile(g_cfg.bg_image.c_str());
        if (img && img->GetLastStatus() == Ok) {
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.DrawImage(img, 0, 0, w, h);
        }
        delete img;
    }

    // Text color with contrast check (skip for transparent backgrounds)
    COLORREF fgCr = HexToColorRef(g_cfg.font_color);
    if (g_cfg.bg_type != L"transparent")
        fgCr = ContrastColor(fgCr, HexToColorRef(g_cfg.bg_color));
    Gdiplus::Color textColor(255, GetRValue(fgCr), GetGValue(fgCr), GetBValue(fgCr));

    int fontSize = g_cfg.font_size;
    int dateSize = std::max(12, (int)(fontSize * 0.45));
    int secPct = (g_cfg.seconds_size > 0) ? g_cfg.seconds_size : 60;
    int secSize = std::max(8, (int)(fontSize * secPct / 100));

    // Use custom font family for both time and date
    FontFamily* ff = new FontFamily(g_cfg.font_family.c_str());
    if (!ff->IsAvailable()) {
        delete ff;
        ff = new FontFamily(L"Microsoft Sans Serif");
    }
    FontStyle style = g_cfg.font_weight >= 700 ? FontStyleBold : FontStyleRegular;

    // Layout
    int effectiveHour = std::max(8, (int)(fontSize * g_cfg.hour_size / 100));
    int effectiveMin = std::max(8, (int)(fontSize * g_cfg.minute_size / 100));
    ClockLayout L = ComputeLayout(fontSize, secPct, 0, g_cfg.hour_size, g_cfg.minute_size);

    SolidBrush brush(textColor);
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentNear);

    StringFormat sfNear;
    sfNear.SetAlignment(StringAlignmentNear);
    sfNear.SetLineAlignment(StringAlignmentNear);
    sfNear.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces);

    // Split time: "HH", ":", "MM", ":", "SS" (colons are separate objects)
    std::wstring hourText = g_displayTime.substr(0, 2);
    std::wstring colon = L":";
    std::wstring minText = g_displayTime.substr(3, 2);
    std::wstring secDigits = g_displayTime.substr(6, 2);

    // Create fonts BEFORE deleting FontFamily (GDI+ Font copies data internally)
    Font hourFont(ff, (REAL)effectiveHour, style, UnitPixel);
    Font minFont(ff, (REAL)effectiveMin, style, UnitPixel);
    Font secFont(ff, (REAL)secSize, style, UnitPixel);
    Font dateFont(ff, (REAL)dateSize, FontStyleRegular, UnitPixel);
    delete ff;

    // Use reference characters for FIXED-width slots (prevents horizontal jitter)
    RectF refBound;
    g.MeasureString(L"88", -1, &hourFont, PointF(0, 0), &sfNear, &refBound);
    float hourSlot = refBound.Width + 4.0f;
    g.MeasureString(L"88", -1, &minFont, PointF(0, 0), &sfNear, &refBound);
    float minSlot = refBound.Width + 4.0f;
    g.MeasureString(L"88", -1, &secFont, PointF(0, 0), &sfNear, &refBound);
    float secSlot = refBound.Width + 4.0f;
    g.MeasureString(L":", -1, &minFont, PointF(0, 0), &sfNear, &refBound);
    float colonSlot = refBound.Width + 1.0f;
    g.MeasureString(L":", -1, &secFont, PointF(0, 0), &sfNear, &refBound);
    float secColonSlot = refBound.Width + 1.0f;

    float gap = (float)g_cfg.seconds_gap;
    float hmGap = (float)g_cfg.hour_minute_gap;
    // Each colon sits in the middle of its gap (時/分 and 秒/分), so the space on
    // either side equals the gap minus the colon's own width, split evenly.
    float halfHmSide = (hmGap - colonSlot) / 2.0f;
    float halfSecSide = (gap - secColonSlot) / 2.0f;
    float totalW = hourSlot + halfHmSide + colonSlot + halfHmSide + minSlot +
                   halfSecSide + secColonSlot + halfSecSide + secSlot;
    float startX = ((float)w - totalW) / 2.0f;
    if (startX < 0) startX = 0;

    // Draw HH centered in its fixed slot
    RectF hourBound;
    g.MeasureString(hourText.c_str(), -1, &hourFont, PointF(0, 0), &sfNear, &hourBound);
    hourBound.Width += 4.0f;
    float hourSlotCenter = startX + (hourSlot - hourBound.Width) / 2.0f;
    float hourTop = (float)(L.timeBottom - effectiveHour);
    g.DrawString(hourText.c_str(), -1, &hourFont, PointF(hourSlotCenter, hourTop), &sfNear, &brush);

    // Draw ":" centered within the hour/minute gap
    float colon1X = startX + hourSlot + halfHmSide;
    float colonTop = (float)L.timeTop;
    g.DrawString(colon.c_str(), -1, &minFont, PointF(colon1X, colonTop), &sfNear, &brush);

    // Draw MM centered in its fixed slot
    RectF minBound;
    g.MeasureString(minText.c_str(), -1, &minFont, PointF(0, 0), &sfNear, &minBound);
    minBound.Width += 4.0f;
    float minSlotX = colon1X + colonSlot + halfHmSide;
    float minSlotCenter = minSlotX + (minSlot - minBound.Width) / 2.0f;
    float minTop = (float)(L.timeBottom - effectiveMin);
    g.DrawString(minText.c_str(), -1, &minFont, PointF(minSlotCenter, minTop), &sfNear, &brush);

    // Draw second ":" (with secFont) centered within the minute/second gap
    float colon2X = minSlotX + minSlot + halfSecSide;
    float colon2Top = (float)(L.timeBottom - secSize);
    g.DrawString(colon.c_str(), -1, &secFont, PointF(colon2X, colon2Top), &sfNear, &brush);

    // Draw SS centered in its fixed slot
    RectF secBound;
    g.MeasureString(secDigits.c_str(), -1, &secFont, PointF(0, 0), &sfNear, &secBound);
    secBound.Width += 4.0f;
    float secSlotX = colon2X + secColonSlot + halfSecSide;
    float secSlotCenter = secSlotX + (secSlot - secBound.Width) / 2.0f;
    float secTop = (float)(L.timeBottom - secSize);
    g.DrawString(secDigits.c_str(), -1, &secFont, PointF(secSlotCenter, secTop), &sfNear, &brush);

    // Separator line (fixed length, centered)
    float lineW = (float)g_cfg.line_width;
    float lineX = ((float)w - lineW) / 2.0f;
    COLORREF lineCr = HexToColorRef(g_cfg.line_color);
    Gdiplus::Color lineGdiColor(255, GetRValue(lineCr), GetGValue(lineCr), GetBValue(lineCr));
    Pen sepPen(lineGdiColor, 2.0f);
    g.DrawLine(&sepPen, lineX, L.lineY, lineX + lineW, L.lineY);

    // Date text (top-aligned, centered horizontally)
    g.DrawString(g_displayDate.c_str(), -1, &dateFont, PointF((REAL)(w / 2), (REAL)L.dateTop), &sfCenter, &brush);
}

static void ShowContextMenu(HWND hWnd, int x, int y) {
    if (!g_hMenu) {
        g_hMenu = CreatePopupMenu();
        AppendMenuW(g_hMenu, MF_STRING, 1001, L"設定");
        AppendMenuW(g_hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(g_hMenu, MF_STRING, 1002, L"退出");
    }

    int cmd = TrackPopupMenu(g_hMenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, hWnd, NULL);
    switch (cmd) {
    case 1001: CreateSettingsWindow(hWnd); break;
    case 1002: PostQuitMessage(0); break;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        srand((unsigned int)time(NULL));
        g_hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        SetTimer(hWnd, 1, 1000, NULL);   // Clock update every 1s
        UpdateClockText();
        return 0;
    }

    case WM_ACTIVATE:
        return 0;

    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONUP) {
            HMENU trayMenu = CreatePopupMenu();
            AppendMenuW(trayMenu, MF_STRING, 1001, L"設定");
            AppendMenuW(trayMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(trayMenu, MF_STRING, 1002, L"退出");
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(trayMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(trayMenu);
            if (cmd == 1001) CreateSettingsWindow(hWnd);
            else if (cmd == 1002) PostQuitMessage(0);
            return 0;
        }
        if (lParam == WM_LBUTTONDBLCLK) {
            CreateSettingsWindow(hWnd);
            return 0;
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1) {
            UpdateClockText();
            InvalidateRect(hWnd, NULL, FALSE);
            // Reassert topmost every tick to stay above all other windows
            // (skip while the settings dialog is open so it can stay usable)
            if (!g_settingsOpen) {
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
        return 0;

    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS* pos = (WINDOWPOS*)lParam;
        pos->hwndInsertAfter = HWND_TOPMOST;
        pos->flags &= ~SWP_NOZORDER;
        return 0;
    }

    case WM_WINDOWPOSCHANGED: {
        // Safety net: if the window was moved to a non-topmost position
        // (e.g. by other apps), pull it straight back to the top
        WINDOWPOS* pos = (WINDOWPOS*)lParam;
        if (pos->hwndInsertAfter != HWND_TOPMOST &&
            (GetWindowLongPtrW(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0) {
            SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        HBRUSH bg = CreateSolidBrush(HexToColorRef(g_cfg.bg_color));
        FillRect(memDC, &rc, bg);
        DeleteObject(bg);

        DrawClock(memDC, &rc);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN: {
        g_dragging = true;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hWnd, &pt);
        RECT wr;
        GetWindowRect(hWnd, &wr);
        g_dragOffset.x = pt.x - wr.left;
        g_dragOffset.y = pt.y - wr.top;
        SetCapture(hWnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_dragging) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hWnd, &pt);
            SetWindowPos(hWnd, NULL, pt.x - g_dragOffset.x, pt.y - g_dragOffset.y,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_dragging) {
            g_dragging = false;
            ReleaseCapture();
            RECT wr;
            GetWindowRect(hWnd, &wr);
            g_cfg.x = wr.left;
            g_cfg.y = wr.top;
            g_cfg.Save();
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
        CreateSettingsWindow(hWnd);
        return 0;

    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hWnd, &pt);
        ShowContextMenu(hWnd, pt.x, pt.y);
        return 0;
    }

    case WM_QUERYENDSESSION:
        PostQuitMessage(0);
        return TRUE;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        TrayRemove();
        KillTimer(hWnd, 1);
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool CheckSingleInstance() {
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"AIClockInstanceMutex");
    return hMutex && GetLastError() != ERROR_ALREADY_EXISTS;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;

    // Standard Windows security init
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Handle command line
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc > 1 && wcscmp(argv[1], L"/about") == 0) {
            MessageBoxW(NULL, L"AIClock Desktop Widget v2.0", L"About AIClock", MB_OK);
            LocalFree(argv);
            CoUninitialize();
            return 0;
        }
        if (argv) LocalFree(argv);
    }

    if (!CheckSingleInstance()) {
        MessageBoxW(NULL, L"AIClock is already running.", L"AIClock", MB_OK | MB_ICONINFORMATION);
        CoUninitialize();
        return 0;
    }

    // Remove Zone.Identifier ADS
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring adsPath = std::wstring(exePath) + L":Zone.Identifier";
        DeleteFileW(adsPath.c_str());
    }

    GdiplusStartupInput gsi;
    if (GdiplusStartup(&g_gdiToken, &gsi, NULL) != Ok) {
        MessageBoxW(NULL, L"Failed to initialize GDI+.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    RegSettingsClass();

    const wchar_t CLASS_NAME[] = L"AIClockWindow";
    WNDCLASSW wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!RegisterClassW(&wc)) {
        GdiplusShutdown(g_gdiToken);
        CoUninitialize();
        return 1;
    }

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    g_hWnd = CreateWindowExW(exStyle, CLASS_NAME, L"AIClock",
        WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 750, 300,
        NULL, NULL, hInstance, NULL);
    if (!g_hWnd) {
        GdiplusShutdown(g_gdiToken);
        CoUninitialize();
        return 1;
    }

    g_cfg.Load();
    RecreateFont();

    double op = g_cfg.opacity;
    if (op < 0.2) op = 0.2;
    if (op > 1.0) op = 1.0;
    if (g_cfg.bg_type == L"transparent") {
        COLORREF key = HexToColorRef(g_cfg.bg_color);
        SetLayeredWindowAttributes(g_hWnd, key, (BYTE)(op * 255), LWA_COLORKEY | LWA_ALPHA);
    } else {
        SetLayeredWindowAttributes(g_hWnd, 0, (BYTE)(op * 255), LWA_ALPHA);
    }

    int w = g_cfg.width;
    int h = CalcHeight(g_cfg.font_size);
    int x = g_cfg.x;
    int y = g_cfg.y;
    {
        POINT testPt = { x + w / 2, y + h / 2 };
        HMONITOR hm = MonitorFromPoint(testPt, MONITOR_DEFAULTTONULL);
        if (!hm) {
            hm = MonitorFromPoint(testPt, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfoW(hm, &mi);
            int mw = mi.rcWork.right - mi.rcWork.left;
            int mh = mi.rcWork.bottom - mi.rcWork.top;
            x = mi.rcWork.left + (mw - w) / 2;
            y = mi.rcWork.top + (mh - h) / 2;
        } else {
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfoW(hm, &mi);
            if (x + w < mi.rcWork.left + 50 || x > mi.rcWork.right - 50 ||
                y + h < mi.rcWork.top + 50 || y > mi.rcWork.bottom - 50) {
                x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
                y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
            }
        }
    }
    SetWindowPos(g_hWnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);
    TrayAdd();

    MSG msg = { };
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(g_gdiToken);
    CoUninitialize();
    return (int)msg.wParam;
}
