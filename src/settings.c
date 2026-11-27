/*
 * Bảng điều khiển: các ô đổi là áp dụng và lưu ngay. Phần cửa sổ, DPI, sáng/tối nằm ở ui.c.
 *
 * Ô chọn và khung nhóm có theme vẽ chữ màu đen bất kể WM_CTLCOLOR*, nên tự vẽ: ô chọn qua
 * NM_CUSTOMDRAW (vẫn dùng hình ô của theme), khung nhóm vẽ trong WM_PAINT.
 */
#include "gtv.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>

enum {
    ID_CLOSE = IDOK, ID_EXIT = 10, ID_EXCLUDE, ID_MACROS, ID_DEFAULT, ID_UPDATE, ID_UPDATE_TEXT,
    ID_ENABLE = 20, ID_OLD_STYLE, ID_W_ALONE, ID_MACRO, ID_SOUND, ID_AUTOSTART, ID_AUTO_UPDATE, ID_ADMIN,
    ID_HK = 40, /* + chỉ số trong HOTKEY_PRESETS */
};

#define GROUP (WS_GROUP | WS_TABSTOP)
#define CHECK (BS_CHECKBOX | GROUP)
#define PUSH (BS_PUSHBUTTON | GROUP)

/* Thứ tự trong bảng là thứ tự Tab. */
static const UiCtl CTLS[] = {
    {L"BUTTON", L"Đóng", BS_DEFPUSHBUTTON | GROUP, ID_CLOSE, 378, 14, 122, 30},
    {L"BUTTON", L"Thoát", PUSH, ID_EXIT, 378, 52, 122, 30},

    {L"BUTTON", L"Bật gõ tiếng Việt", CHECK, ID_ENABLE, 26, 30, 320, 22},
    {L"STATIC", L"Phím chuyển:", SS_LEFT | SS_CENTERIMAGE | WS_GROUP, -1, 26, 60, 82, 22},
    {L"BUTTON", L"Ctrl+Shift", BS_AUTORADIOBUTTON | GROUP, ID_HK + 0, 110, 60, 84, 22},
    {L"BUTTON", L"Alt+Z", BS_AUTORADIOBUTTON, ID_HK + 1, 196, 60, 62, 22},
    {L"BUTTON", L"Ctrl+Space", BS_AUTORADIOBUTTON, ID_HK + 2, 260, 60, 96, 22},

    /* Ô dài chiếm cả dòng, ô ngắn xếp hai cột. */
    {L"BUTTON", L"Đặt dấu kiểu cũ (hòa, khỏe, thúy)", CHECK, ID_OLD_STYLE, 26, 124, 330, 22},
    {L"BUTTON", L"Gõ w thành ư", CHECK, ID_W_ALONE, 26, 148, 164, 22},
    {L"BUTTON", L"Bật gõ tắt", CHECK, ID_MACRO, 194, 148, 164, 22},
    {L"BUTTON", L"Âm báo khi bật/tắt", CHECK, ID_SOUND, 26, 172, 164, 22},
    {L"BUTTON", L"Tự kiểm tra cập nhật", CHECK, ID_AUTO_UPDATE, 194, 172, 164, 22},
    {L"BUTTON", L"Khởi động cùng Windows", CHECK, ID_AUTOSTART, 26, 196, 164, 22},
    {L"BUTTON", L"Chạy với quyền admin", CHECK, ID_ADMIN, 194, 196, 164, 22},

    {L"STATIC", L"", SS_CENTER | WS_GROUP, ID_UPDATE_TEXT, 372, 152, 134, 38},
    {L"BUTTON", L"Kiểm tra cập nhật", PUSH, ID_UPDATE, 378, 194, 122, 30},

    /* Chữ trên hai nút danh sách có thêm số mục, đặt trong settings_refresh. */
    {L"BUTTON", L"", PUSH, ID_EXCLUDE, 12, 238, 196, 30},
    {L"BUTTON", L"", PUSH, ID_MACROS, 216, 238, 150, 30},
    {L"BUTTON", L"Mặc định", PUSH, ID_DEFAULT, 378, 238, 122, 30},
};
#define NCTLS (int)(sizeof CTLS / sizeof CTLS[0])

typedef struct {
    const wchar_t *title;
    short x, y, w, h;
} Group;

static const Group GROUPS[] = {
    {L"Điều khiển", 12, 6, 354, 88},
    {L"Tùy chọn", 12, 100, 354, 126},
};
#define NGROUPS (int)(sizeof GROUPS / sizeof GROUPS[0])

static HWND g_ctl[NCTLS];
static HTHEME g_theme;
static void open_theme(UiWin *u);
static UiWin g_win = {CTLS, NCTLS, g_ctl, 512, 280, open_theme, open_theme};

static void set_check(int id, int on)
{
    SendDlgItemMessageW(g_win.w, id, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
}

/* Hai dòng phía trên nút cập nhật: tình trạng, rồi số phiên bản. Có bản mới thì nút
 * thành nút tải. */
static void refresh_update(void)
{
    wchar_t s[80];
    const wchar_t *what = L"Phiên bản", *ver = APP_VERSION;
    int st = update_state();
    switch (st) {
    case UPD_CHECKING: what = L"Đang kiểm tra"; break;
    case UPD_LATEST: what = L"Đã là bản mới nhất"; break;
    case UPD_NEWER: what = L"Có bản mới"; ver = update_latest(); break;
    case UPD_ERROR: what = L"Không kiểm tra được"; break;
    }
    wsprintfW(s, L"%s\n%s", what, ver);
    SetDlgItemTextW(g_win.w, ID_UPDATE_TEXT, s);
    SetDlgItemTextW(g_win.w, ID_UPDATE, st == UPD_NEWER ? L"Tải bản mới" : L"Kiểm tra cập nhật");
    EnableWindow(GetDlgItem(g_win.w, ID_UPDATE), st != UPD_CHECKING);
}

void settings_refresh(void)
{
    DWORD opts;
    int i;
    wchar_t s[64];
    if (!g_win.w) return;
    opts = engine_options();
    wsprintfW(s, L"Danh sách app loại trừ (%d)", exclude_count());
    SetDlgItemTextW(g_win.w, ID_EXCLUDE, s);
    wsprintfW(s, L"Danh sách gõ tắt (%d)", macro_count());
    SetDlgItemTextW(g_win.w, ID_MACROS, s);
    set_check(ID_ENABLE, engine_enabled());
    for (i = 0; i < N_HOTKEY_PRESETS; i++) set_check(ID_HK + i, HOTKEY_PRESETS[i] == engine_hotkey());
    set_check(ID_OLD_STYLE, opts & OPT_OLD_STYLE);
    set_check(ID_W_ALONE, opts & OPT_W_ALONE);
    set_check(ID_MACRO, opts & OPT_MACRO);
    set_check(ID_SOUND, tray_sound());
    set_check(ID_AUTOSTART, autostart_enabled());
    set_check(ID_AUTO_UPDATE, opts & OPT_AUTO_UPDATE);
    set_check(ID_ADMIN, engine_is_elevated());
    refresh_update();
}

/* Theme ô chọn theo DPI và sáng/tối hiện tại; bản tối có thể không có, khi đó dùng bản thường. */
static void open_theme(UiWin *u)
{
    if (g_theme) CloseThemeData(g_theme);
    g_theme = 0;
    if (g_ui.dark) g_theme = OpenThemeDataForDpi(u->w, L"DarkMode_Explorer::Button", u->dpi);
    if (!g_theme) g_theme = OpenThemeDataForDpi(u->w, L"Button", u->dpi);
}

/* Khung nhóm bo góc, tiêu đề nằm đè lên cạnh trên như groupbox. */
static void paint(HWND w)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(w, &ps);
    HPEN pen = CreatePen(PS_SOLID, 1, g_ui.pal.border);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ of = SelectObject(dc, g_win.bold);
    int r = ui_scale(&g_win, 8), i;
    SetBkMode(dc, OPAQUE);
    SetBkColor(dc, g_ui.pal.bg);
    SetTextColor(dc, g_ui.pal.text);
    for (i = 0; i < NGROUPS; i++) {
        const Group *g = &GROUPS[i];
        int top = ui_scale(&g_win, g->y + 8), len = lstrlenW(g->title), x = ui_scale(&g_win, g->x + 12);
        SIZE sz;
        RECT t;
        RoundRect(dc, ui_scale(&g_win, g->x), top, ui_scale(&g_win, g->x + g->w), ui_scale(&g_win, g->y + g->h), r, r);
        GetTextExtentPoint32W(dc, g->title, len, &sz);
        t.left = x - ui_scale(&g_win, 4);
        t.right = x + sz.cx + ui_scale(&g_win, 4);
        t.top = top - sz.cy / 2;
        t.bottom = t.top + sz.cy;
        ExtTextOutW(dc, x, t.top, ETO_OPAQUE, &t, g->title, (UINT)len, 0);
    }
    SelectObject(dc, of);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
    EndPaint(w, &ps);
}

/* Ô chọn/ô tròn: hình ô của theme + chữ theo bảng màu. */
static LRESULT draw_check(const NMCUSTOMDRAW *cd)
{
    HWND h = cd->hdr.hwndFrom;
    DWORD type = (DWORD)GetWindowLongW(h, GWL_STYLE) & BS_TYPEMASK;
    int radio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
    int checked = SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
    UINT st = cd->uItemState;
    int part = radio ? BP_RADIOBUTTON : BP_CHECKBOX;
    int state = (checked ? 5 : 1) + ((st & CDIS_DISABLED) ? 3 : (st & CDIS_SELECTED) ? 2 : (st & CDIS_HOT) ? 1 : 0);
    wchar_t text[64];
    RECT rc = cd->rc, box, tr;
    SIZE sz = {0, 0};
    HGDIOBJ of;
    int len;

    if (cd->dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
    FillRect(cd->hdc, &rc, g_ui.bg);
    if (g_theme) GetThemePartSize(g_theme, cd->hdc, part, state, 0, TS_DRAW, &sz);
    if (!sz.cx) sz.cx = sz.cy = ui_scale(&g_win, 13);
    box.left = rc.left;
    box.top = rc.top + (rc.bottom - rc.top - sz.cy) / 2;
    box.right = box.left + sz.cx;
    box.bottom = box.top + sz.cy;
    if (g_theme) DrawThemeBackground(g_theme, cd->hdc, part, state, &box, 0);

    len = GetWindowTextW(h, text, 64);
    of = SelectObject(cd->hdc, g_win.font);
    SetBkMode(cd->hdc, TRANSPARENT);
    SetTextColor(cd->hdc, (st & CDIS_DISABLED) ? g_ui.pal.dim : g_ui.pal.text);
    tr = rc;
    tr.left = box.right + ui_scale(&g_win, 6);
    DrawTextW(cd->hdc, text, len, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    if ((st & CDIS_FOCUS) && !(SendMessageW(h, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        RECT fr = tr;
        DrawTextW(cd->hdc, text, len, &fr, DT_SINGLELINE | DT_CALCRECT);
        OffsetRect(&fr, 0, (tr.top + tr.bottom) / 2 - (fr.top + fr.bottom) / 2);
        InflateRect(&fr, ui_scale(&g_win, 2), 1);
        SetTextColor(cd->hdc, g_ui.pal.text);
        DrawFocusRect(cd->hdc, &fr);
    }
    SelectObject(cd->hdc, of);
    return CDRF_SKIPDEFAULT;
}

static void flip_option(DWORD bit)
{
    tray_set_options(engine_options() ^ bit);
}

static void on_command(int id)
{
    if (id >= ID_HK && id < ID_HK + N_HOTKEY_PRESETS) {
        tray_set_hotkey(HOTKEY_PRESETS[id - ID_HK]);
        return;
    }
    switch (id) {
    case ID_CLOSE:
    case IDCANCEL: DestroyWindow(g_win.w); return;
    case ID_EXIT: tray_quit(); return;
    case ID_ENABLE: engine_toggle(); return; /* WM_GTV_STATE sẽ cập nhật ô */
    case ID_OLD_STYLE: flip_option(OPT_OLD_STYLE); break;
    case ID_W_ALONE: flip_option(OPT_W_ALONE); break;
    case ID_MACRO: flip_option(OPT_MACRO); break;
    case ID_SOUND: tray_set_sound(!tray_sound()); break;
    case ID_AUTOSTART: autostart_set(!autostart_enabled()); break;
    case ID_AUTO_UPDATE: flip_option(OPT_AUTO_UPDATE); break;
    case ID_ADMIN: tray_switch_admin(); return;
    case ID_EXCLUDE: listedit_show(LIST_EXCLUDE, g_win.w); return;
    case ID_MACROS: listedit_show(LIST_MACRO, g_win.w); return;
    case ID_UPDATE:
        if (update_state() == UPD_NEWER) update_open_page();
        else tray_check_update();
        return;
    case ID_DEFAULT:
        tray_set_hotkey(HOTKEY_DEFAULT);
        tray_set_options(OPT_DEFAULT);
        tray_set_sound(1);
        break;
    }
    settings_refresh();
}

static LRESULT CALLBACK settings_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;
    if (ui_handle(&g_win, msg, wp, lp, &res)) return res;
    switch (msg) {
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) on_command(LOWORD(wp));
        return 0;
    case WM_NOTIFY: {
        const NMHDR *n = (const NMHDR *)lp;
        if (n->code == NM_CUSTOMDRAW && n->idFrom >= ID_ENABLE) /* ô chọn và ô tròn */
            return draw_check((const NMCUSTOMDRAW *)lp);
        return 0;
    }
    case WM_PAINT:
        paint(w);
        return 0;
    case WM_DESTROY:
        if (g_theme) CloseThemeData(g_theme);
        g_theme = 0;
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

void settings_show(void)
{
    const wchar_t *title = engine_is_elevated() ? L"gotiengviet (admin)" : L"gotiengviet";
    if (!ui_create(&g_win, L"GotiengvietSettings", settings_proc, title, 0)) return;
    settings_refresh();
    ui_show(&g_win, ID_CLOSE);
}

int settings_dialog_message(MSG *msg)
{
    if (listedit_dialog_message(msg)) return 1;
    return g_win.w && IsDialogMessageW(g_win.w, msg);
}
