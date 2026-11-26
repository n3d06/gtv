/* Icon khay (chữ V/E rỗng, màu theo theme taskbar), âm báo bật/tắt, menu, trả bớt bộ nhớ định kỳ. */
#include "gtv.h"
#include <shellapi.h>

#define WM_TRAY (WM_APP + 1)
#define ID_TOGGLE 1
#define ID_AUTOSTART 2
#define ID_ADMIN 3
#define ID_EXCLUDE_LIST 4
#define ID_EXIT 5
#define ID_SOUND 6
#define ID_HOTKEY 100 /* + chỉ số trong PRESETS */
#define TIMER_TRIM_ONCE 1
#define TIMER_TRIM 2
#define TIMER_EXCLUDE 3
#define TRIM_PERIOD_MS (60 * 1000)
#define SS 4 /* vẽ icon to gấp SS lần rồi thu nhỏ để nét mượt */

static HWND g_wnd;
static UINT g_taskbar_created;
static HICON g_icon_v, g_icon_e;
static NOTIFYICONDATAW g_nid;
static int g_sound = 1;

static const wchar_t APP_KEY[] = L"Software\\gotiengviet";
static const DWORD PRESETS[] = {HOTKEY_DEFAULT, HOTKEY(HK_ALT, 'Z'), HOTKEY(HK_CTRL, VK_SPACE)};
static const wchar_t *const PRESET_NAMES[] = {L"Ctrl+Shift", L"Alt+Z", L"Ctrl+Space"};
#define NPRESETS (sizeof PRESETS / sizeof PRESETS[0])

/* Trả các trang không dùng về cho Windows; cần lại thì lấy về rất nhanh. */
static void trim(void)
{
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

static HBITMAP make_dib(HDC dc, int w, DWORD **bits)
{
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -w;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)bits, 0, 0);
}

static HICON make_icon(wchar_t letter, COLORREF col)
{
    static BYTE zeros[8 * 64];
    int sz = GetSystemMetrics(SM_CXSMICON), big, x, y, dx, dy;
    DWORD *bbits = 0, *bits = 0, rgb;
    HDC dc;
    HBITMAP bigbmp, color, mask, old;
    HFONT font, oldf;
    HPEN pen, oldp;
    RECT r;
    ICONINFO ii;
    HICON icon = 0;

    if (sz > 64) sz = 64;
    big = sz * SS;
    dc = CreateCompatibleDC(0);
    if (!dc) return 0;
    bigbmp = make_dib(dc, big, &bbits);
    color = make_dib(dc, sz, &bits);
    mask = CreateBitmap(sz, sz, 1, 1, zeros);
    if (bigbmp && color && mask) {
        old = (HBITMAP)SelectObject(dc, bigbmp);
        r.left = r.top = 0;
        r.right = r.bottom = big;
        font = CreateFontW(-big, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           ANTIALIASED_QUALITY, 0, L"Segoe UI");
        pen = CreatePen(PS_SOLID, SS * sz / 16 > 1 ? SS * sz / 16 : 1, RGB(255, 255, 255));
        oldf = (HFONT)SelectObject(dc, font);
        oldp = (HPEN)SelectObject(dc, pen);
        SetBkMode(dc, TRANSPARENT);
        BeginPath(dc);
        DrawTextW(dc, &letter, 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        EndPath(dc);
        StrokePath(dc);
        GdiFlush();
        SelectObject(dc, oldp);
        SelectObject(dc, oldf);
        DeleteObject(pen);
        DeleteObject(font);
        SelectObject(dc, old);

        /* Độ phủ của mỗi ô SS x SS thành độ trong suốt. */
        rgb = ((DWORD)GetRValue(col) << 16) | ((DWORD)GetGValue(col) << 8) | GetBValue(col);
        for (y = 0; y < sz; y++) {
            for (x = 0; x < sz; x++) {
                DWORD sum = 0;
                for (dy = 0; dy < SS; dy++)
                    for (dx = 0; dx < SS; dx++)
                        sum += bbits[(y * SS + dy) * big + x * SS + dx] & 0xFF;
                bits[y * sz + x] = ((sum / (SS * SS)) << 24) | rgb;
            }
        }

        ii.fIcon = TRUE;
        ii.xHotspot = ii.yHotspot = 0;
        ii.hbmMask = mask;
        ii.hbmColor = color;
        icon = CreateIconIndirect(&ii);
    }
    if (bigbmp) DeleteObject(bigbmp);
    if (color) DeleteObject(color);
    if (mask) DeleteObject(mask);
    DeleteDC(dc);
    return icon;
}

static int taskbar_is_light(void)
{
    DWORD v = 0, n = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, 0, &v, &n) != ERROR_SUCCESS)
        return 0;
    return v != 0;
}

static void make_icons(void)
{
    COLORREF col = taskbar_is_light() ? RGB(0, 0, 0) : RGB(255, 255, 255);
    if (g_icon_v) DestroyIcon(g_icon_v);
    if (g_icon_e) DestroyIcon(g_icon_e);
    g_icon_v = make_icon('V', col);
    g_icon_e = make_icon('E', col);
}

static const wchar_t *hotkey_name(void)
{
    UINT i;
    for (i = 0; i < NPRESETS; i++)
        if (PRESETS[i] == engine_hotkey()) return PRESET_NAMES[i];
    return PRESET_NAMES[0];
}

static void load_hotkey(void)
{
    DWORD v = 0, n = sizeof v;
    UINT i;
    if (RegGetValueW(HKEY_CURRENT_USER, APP_KEY, L"Hotkey", RRF_RT_REG_DWORD, 0, &v, &n) != ERROR_SUCCESS) return;
    for (i = 0; i < NPRESETS; i++)
        if (PRESETS[i] == v) engine_set_hotkey(v); /* chỉ nhận phím tắt có trong menu */
}

static void save_hotkey(void)
{
    DWORD v = engine_hotkey();
    RegSetKeyValueW(HKEY_CURRENT_USER, APP_KEY, L"Hotkey", REG_DWORD, &v, sizeof v);
}

/* Beep của kernel32: không thêm DLL, vẫn kêu khi bộ âm thanh Windows là "No sounds".
 * Chạy trên luồng giao diện nên không làm chậm hook. */
static void beep(DWORD freq, DWORD ms)
{
    if (g_sound) Beep(freq, ms);
}

static void load_sound(void)
{
    DWORD v = 1, n = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, APP_KEY, L"Sound", RRF_RT_REG_DWORD, 0, &v, &n) == ERROR_SUCCESS)
        g_sound = v != 0;
}

static void save_sound(void)
{
    DWORD v = (DWORD)g_sound;
    RegSetKeyValueW(HKEY_CURRENT_USER, APP_KEY, L"Sound", REG_DWORD, &v, sizeof v);
}

static void update_icon(DWORD msg)
{
    int on = engine_enabled();
    g_nid.hIcon = on ? g_icon_v : g_icon_e;
    wsprintfW(g_nid.szTip, L"gotiengviet%s: %s (%s để %s)", engine_is_elevated() ? L" (admin)" : L"",
              on ? L"Tiếng Việt" : L"Tiếng Anh", hotkey_name(), on ? L"tắt" : L"bật");
    Shell_NotifyIconW(msg, &g_nid);
}

static void add_icon(void)
{
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_wnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_TRAY;
    update_icon(NIM_ADD);
}

/*
 * Menu tối: Win32 không có API công khai, dùng uxtheme ordinal 135 (SetPreferredAppMode)
 * và 136 (FlushMenuThemes) như Notepad++. Không có thì menu giữ màu sáng.
 */
typedef int(WINAPI *SetPreferredAppModeFn)(int mode); /* 2: tối, 3: sáng */
typedef void(WINAPI *FlushMenuThemesFn)(void);

static void apply_menu_theme(void)
{
    static int loaded;
    static SetPreferredAppModeFn set_mode;
    static FlushMenuThemesFn flush;
    if (!loaded) {
        HMODULE ux = LoadLibraryExW(L"uxtheme.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
        loaded = 1;
        if (!ux) return;
        set_mode = (SetPreferredAppModeFn)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        flush = (FlushMenuThemesFn)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    }
    if (!set_mode) return;
    set_mode(taskbar_is_light() ? 3 : 2);
    if (flush) flush();
}

static void show_menu(void)
{
    HMENU m, keys;
    POINT pt;
    UINT cmd, i;
    wchar_t item[64];

    apply_menu_theme();
    m = CreatePopupMenu();
    keys = CreatePopupMenu();
    if (!m || !keys) return;
    for (i = 0; i < NPRESETS; i++)
        AppendMenuW(keys, MF_STRING | (PRESETS[i] == engine_hotkey() ? MF_CHECKED : 0), ID_HOTKEY + i, PRESET_NAMES[i]);
    wsprintfW(item, L"Gõ tiếng Việt\t%s", hotkey_name());
    AppendMenuW(m, MF_STRING | (engine_enabled() ? MF_CHECKED : 0), ID_TOGGLE, item);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)keys, L"Phím tắt bật/tắt");
    AppendMenuW(m, MF_STRING | (g_sound ? MF_CHECKED : 0), ID_SOUND, L"Âm báo");
    AppendMenuW(m, MF_STRING | (autostart_enabled() ? MF_CHECKED : 0), ID_AUTOSTART, L"Khởi động cùng Windows");
    AppendMenuW(m, MF_STRING | (engine_is_elevated() ? MF_CHECKED : 0), ID_ADMIN, L"Chạy với quyền admin");
    AppendMenuW(m, MF_STRING, ID_EXCLUDE_LIST, L"Danh sách app loại trừ");
    AppendMenuW(m, MF_SEPARATOR, 0, 0);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Thoát");
    GetCursorPos(&pt);
    SetForegroundWindow(g_wnd);
    cmd = (UINT)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g_wnd, 0);
    PostMessageW(g_wnd, WM_NULL, 0, 0);
    DestroyMenu(m); /* hủy luôn menu con */
    if (cmd >= ID_HOTKEY && cmd < ID_HOTKEY + NPRESETS) {
        engine_set_hotkey(PRESETS[cmd - ID_HOTKEY]);
        save_hotkey();
        update_icon(NIM_MODIFY);
    }
    switch (cmd) {
    case ID_TOGGLE: engine_toggle(); break;
    case ID_SOUND:
        g_sound = !g_sound;
        save_sound();
        break;
    case ID_AUTOSTART: autostart_set(!autostart_enabled()); break;
    case ID_ADMIN:
        if (engine_is_elevated()) {
            autostart_before_unelevate();
            if (relaunch_normal()) { DestroyWindow(g_wnd); return; }
        } else if (relaunch_elevated()) {
            DestroyWindow(g_wnd);
            return;
        }
        break;
    case ID_EXCLUDE_LIST: exclude_open_file(); break;
    case ID_EXIT: DestroyWindow(g_wnd); return;
    }
    trim();
}

static LRESULT CALLBACK wnd_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TRAY:
        if (lp == WM_LBUTTONUP) engine_toggle();
        else if (lp == WM_RBUTTONUP) show_menu();
        return 0;
    case WM_GTV_STATE:
        update_icon(NIM_MODIFY);
        beep(engine_enabled() ? 1000 : 500, 70);
        return 0;
    case WM_SETTINGCHANGE:
        if (lp && lstrcmpW((const wchar_t *)lp, L"ImmersiveColorSet") == 0) {
            make_icons();
            update_icon(NIM_MODIFY);
        }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_EXCLUDE) {
            exclude_reload_if_changed();
            return 0;
        }
        if (wp == TIMER_TRIM_ONCE) KillTimer(w, TIMER_TRIM_ONCE);
        trim();
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    if (msg == g_taskbar_created && msg) { /* Explorer khởi động lại */
        add_icon();
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int tray_run(void)
{
    WNDCLASSW wc = {0};
    MSG msg;

    autostart_on_start();
    load_hotkey();
    load_sound();
    make_icons();
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW(0);
    wc.lpszClassName = L"GotiengvietTray";
    if (!RegisterClassW(&wc)) return 0;
    /* Cửa sổ ẩn thường, không phải message-only, để nhận được TaskbarCreated. */
    g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"gotiengviet", WS_POPUP,
                            0, 0, 0, 0, 0, 0, wc.hInstance, 0);
    if (!g_wnd) return 0;
    engine_set_notify(g_wnd);
    add_icon();
    SetTimer(g_wnd, TIMER_TRIM_ONCE, 3000, 0);
    SetTimer(g_wnd, TIMER_TRIM, TRIM_PERIOD_MS, 0);
    SetTimer(g_wnd, TIMER_EXCLUDE, 3000, 0);

    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    engine_set_notify(0);
    if (IsWindow(g_wnd)) DestroyWindow(g_wnd); /* thoát bằng Ctrl+C vẫn gỡ icon */
    return 1;
}
