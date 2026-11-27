/*
 * Phần dùng chung của các cửa sổ (bảng điều khiển, danh sách loại trừ, gõ tắt): tạo control
 * theo bảng, bố cục theo 96 DPI rồi nhân theo DPI màn hình, icon nón lá, màu sáng/tối theo
 * chế độ app của Windows. Tất cả chạy trên luồng giao diện.
 *
 * Tối: thanh tiêu đề qua DWM; nút, danh sách dùng theme DarkMode_Explorer; ô nhập và hộp
 * chọn dùng DarkMode_CFD; chữ và nền còn lại đặt qua WM_CTLCOLOR*.
 */
#include "gtv.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>

/* Theo màu hộp thoại của Windows 11. */
static const UiPalette LIGHT = {RGB(243, 243, 243), RGB(0, 0, 0), RGB(140, 140, 140), RGB(213, 213, 213),
                                RGB(255, 255, 255)};
static const UiPalette DARK = {RGB(32, 32, 32), RGB(255, 255, 255), RGB(120, 120, 120), RGB(70, 70, 70),
                               RGB(45, 45, 45)};

UiTheme g_ui;

int ui_scale(const UiWin *u, int v) { return MulDiv(v, (int)u->dpi, 96); }

void ui_load_theme(void)
{
    DWORD v = 1, n = sizeof v;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, 0, &v, &n);
    if (g_ui.bg && g_ui.dark == (v == 0)) return;
    g_ui.dark = v == 0;
    g_ui.pal = g_ui.dark ? DARK : LIGHT;
    if (g_ui.bg) DeleteObject(g_ui.bg);
    if (g_ui.field) DeleteObject(g_ui.field);
    g_ui.bg = CreateSolidBrush(g_ui.pal.bg);
    g_ui.field = CreateSolidBrush(g_ui.pal.field);
}

/* Tiêu đề cột của danh sách vẽ chữ màu theme (xám đậm trên nền tối): đổi màu chữ qua
 * custom draw. Thông báo của tiêu đề đi tới danh sách chứ không tới cửa sổ, nên phải
 * subclass danh sách. */
static LRESULT CALLBACK list_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR ref)
{
    (void)id;
    (void)ref;
    if (msg == WM_NOTIFY && ((NMHDR *)lp)->code == NM_CUSTOMDRAW && ((NMHDR *)lp)->hwndFrom == ListView_GetHeader(h)) {
        NMCUSTOMDRAW *cd = (NMCUSTOMDRAW *)lp;
        if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (cd->dwDrawStage == CDDS_ITEMPREPAINT) SetTextColor(cd->hdc, g_ui.pal.text);
        return CDRF_DODEFAULT;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

static BOOL CALLBACK theme_child(HWND h, LPARAM lp)
{
    wchar_t cls[32];
    int dark = g_ui.dark;
    (void)lp;
    GetClassNameW(h, cls, 32);
    if (lstrcmpiW(cls, L"Button") == 0) {
        DWORD t = (DWORD)GetWindowLongW(h, GWL_STYLE) & BS_TYPEMASK;
        if (t == BS_PUSHBUTTON || t == BS_DEFPUSHBUTTON) SetWindowTheme(h, dark ? L"DarkMode_Explorer" : L"Explorer", 0);
    } else if (lstrcmpiW(cls, L"Edit") == 0 || lstrcmpiW(cls, L"ComboBox") == 0) {
        SetWindowTheme(h, dark ? L"DarkMode_CFD" : 0, 0);
    } else if (lstrcmpiW(cls, WC_LISTVIEWW) == 0) {
        SetWindowTheme(h, dark ? L"DarkMode_Explorer" : L"Explorer", 0);
        SetWindowTheme(ListView_GetHeader(h), dark ? L"DarkMode_ItemsView" : 0, 0);
        ListView_SetBkColor(h, g_ui.pal.field);
        ListView_SetTextBkColor(h, g_ui.pal.field);
        ListView_SetTextColor(h, g_ui.pal.text);
        SetWindowSubclass(h, list_proc, 1, 0); /* gọi lại nhiều lần vẫn chỉ một subclass */
    }
    return TRUE;
}

void ui_apply_theme(UiWin *u)
{
    BOOL dark = g_ui.dark;
    DwmSetWindowAttribute(u->w, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof dark);
    EnumChildWindows(u->w, theme_child, 0);
    if (u->on_theme) u->on_theme(u);
    RedrawWindow(u->w, 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

/* Font giao diện của Windows (Segoe UI 9pt) theo DPI, thêm bản đậm cho tiêu đề. */
static void layout(UiWin *u)
{
    NONCLIENTMETRICSW ncm;
    HFONT old = u->font, old_bold = u->bold;
    int i;
    ncm.cbSize = sizeof ncm;
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, u->dpi)) {
        u->font = CreateFontIndirectW(&ncm.lfMessageFont);
        ncm.lfMessageFont.lfWeight = FW_SEMIBOLD;
        u->bold = CreateFontIndirectW(&ncm.lfMessageFont);
    }
    for (i = 0; i < u->nctls; i++) {
        const UiCtl *c = &u->ctls[i];
        SetWindowPos(u->hw[i], 0, ui_scale(u, c->x), ui_scale(u, c->y), ui_scale(u, c->w), ui_scale(u, c->h),
                     SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(u->hw[i], WM_SETFONT, (WPARAM)u->font, TRUE);
    }
    if (old && old != u->font) DeleteObject(old);
    if (old_bold && old_bold != u->bold) DeleteObject(old_bold);
    if (u->on_layout) u->on_layout(u);
}

/* Icon nón lá đúng cỡ theo DPI cho thanh tiêu đề và Alt+Tab. */
static void set_icons(UiWin *u)
{
    HINSTANCE inst = GetModuleHandleW(0);
    HICON ib = 0, is = 0;
    int cb = GetSystemMetricsForDpi(SM_CXICON, u->dpi), cs = GetSystemMetricsForDpi(SM_CXSMICON, u->dpi);
    LoadIconWithScaleDown(inst, MAKEINTRESOURCEW(IDI_APP), cb, cb, &ib);
    LoadIconWithScaleDown(inst, MAKEINTRESOURCEW(IDI_APP), cs, cs, &is);
    SendMessageW(u->w, WM_SETICON, ICON_BIG, (LPARAM)ib);
    SendMessageW(u->w, WM_SETICON, ICON_SMALL, (LPARAM)is);
    if (u->icon_big) DestroyIcon(u->icon_big);
    if (u->icon_small) DestroyIcon(u->icon_small);
    u->icon_big = ib;
    u->icon_small = is;
}

int ui_create(UiWin *u, const wchar_t *cls, WNDPROC proc, const wchar_t *title, HWND owner)
{
    static int common_ready;
    HINSTANCE inst = GetModuleHandleW(0);
    WNDCLASSW wc = {0};
    MONITORINFO mi;
    RECT r = {0, 0, 0, 0}, area;
    POINT pt = {0, 0};
    int i, w, h;

    if (u->w) { /* đã mở: đưa lên trước */
        ShowWindow(u->w, SW_RESTORE);
        SetForegroundWindow(u->w);
        return 0;
    }
    if (!common_ready) {
        INITCOMMONCONTROLSEX icc;
        icc.dwSize = sizeof icc;
        icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icc);
        common_ready = 1;
    }
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = cls;
    RegisterClassW(&wc); /* lần mở sau báo lớp đã có, không sao */

    /* Mở trên màn hình có cửa sổ cha, hoặc có con trỏ chuột (nơi vừa bấm icon khay). */
    if (owner) GetWindowRect(owner, &area);
    else {
        GetCursorPos(&pt);
        area.left = area.right = pt.x;
        area.top = area.bottom = pt.y;
    }
    u->w = CreateWindowExW(0, cls, title, UI_STYLE, area.left, area.top, 1, 1, owner, 0, inst, 0);
    if (!u->w) return 0;
    for (i = 0; i < u->nctls; i++) {
        const UiCtl *c = &u->ctls[i];
        u->hw[i] = CreateWindowExW(0, c->cls, c->text, WS_CHILD | WS_VISIBLE | c->style, 0, 0, 0, 0, u->w,
                                   (HMENU)(INT_PTR)c->id, inst, 0);
    }
    u->dpi = GetDpiForWindow(u->w);
    ui_load_theme();
    layout(u);
    set_icons(u);
    ui_apply_theme(u);

    r.right = ui_scale(u, u->cw);
    r.bottom = ui_scale(u, u->ch);
    AdjustWindowRectExForDpi(&r, UI_STYLE, FALSE, 0, u->dpi);
    w = r.right - r.left;
    h = r.bottom - r.top;
    if (!owner) {
        mi.cbSize = sizeof mi;
        GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
        area = mi.rcWork;
    }
    SetWindowPos(u->w, 0, (area.left + area.right - w) / 2, (area.top + area.bottom - h) / 2, w, h, SWP_NOZORDER);
    return 1;
}

void ui_show(UiWin *u, int focus_id)
{
    ShowWindow(u->w, SW_SHOW);
    SetForegroundWindow(u->w);
    SetFocus(GetDlgItem(u->w, focus_id));
}

/* Các message chung; trả 1 nếu đã xử lý (*res là giá trị trả về). */
int ui_handle(UiWin *u, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    *res = 0;
    switch (msg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetTextColor((HDC)wp, g_ui.pal.text);
        SetBkColor((HDC)wp, g_ui.pal.bg);
        *res = (LRESULT)g_ui.bg;
        return 1;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp, g_ui.pal.text);
        SetBkColor((HDC)wp, g_ui.pal.field);
        *res = (LRESULT)g_ui.field;
        return 1;
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(u->w, &r);
        FillRect((HDC)wp, &r, g_ui.bg);
        *res = 1;
        return 1;
    }
    case WM_SETTINGCHANGE:
        if (lp && lstrcmpW((const wchar_t *)lp, L"ImmersiveColorSet") == 0) {
            ui_load_theme();
            ui_apply_theme(u);
        }
        return 1;
    case WM_THEMECHANGED:
        if (u->on_theme) u->on_theme(u);
        return 1;
    case WM_ACTIVATE: /* giữ ô đang chọn khi chuyển qua cửa sổ khác rồi quay lại */
        if (LOWORD(wp) == WA_INACTIVE) u->focus = GetFocus();
        else if (u->focus && IsChild(u->w, u->focus)) SetFocus(u->focus);
        return 1;
    case WM_DPICHANGED: {
        const RECT *r = (const RECT *)lp;
        u->dpi = HIWORD(wp);
        layout(u);
        set_icons(u);
        SetWindowPos(u->w, 0, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(u->w, 0, TRUE);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(u->w);
        return 1;
    case WM_DESTROY:
        if (u->font) DeleteObject(u->font);
        if (u->bold) DeleteObject(u->bold);
        if (u->icon_big) DestroyIcon(u->icon_big);
        if (u->icon_small) DestroyIcon(u->icon_small);
        u->font = u->bold = 0;
        u->icon_big = u->icon_small = 0;
        u->w = u->focus = 0;
        return 0; /* để cửa sổ dọn phần riêng của nó */
    }
    return 0;
}
