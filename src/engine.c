/*
 * Quyết định chặn hay cho phím đi qua, chạy ngay trong callback hook: muốn thay chữ
 * thì phím gốc phải bị chặn trước khi tới app. Lõi Telex chỉ tốn vài micro giây.
 *
 * Khi còn lần phát chưa đi qua hook, mọi sự kiện khác (kể cả nhả phím, Shift, Ctrl)
 * được phát lại phía sau qua cùng hàng đợi để app nhận đúng thứ tự.
 */
#include "gtv.h"

static Telex g_tx;
static volatile LONG g_enabled = 1;
static volatile HWND g_notify;
static volatile DWORD g_hotkey = HOTKEY_DEFAULT;
static int g_combo; /* đang giữ đúng Ctrl+Shift, chưa có phím nào khác */
static int g_caps, g_caps_down;
static unsigned char g_swallowed[256]; /* chặn lúc nhấn thì chặn luôn lúc nhả */
static HWND g_fg;
static int g_fg_blocked, g_fg_pad, g_fg_excluded;
static LONG g_ex_version;
static int g_self_elevated;
static int g_mouse_reset;

/* Thanh địa chỉ, ô có gợi ý bôi đen: Backspace đầu chỉ xóa phần bôi đen. */
static const wchar_t *const PAD_APPS[] = {
    L"chrome.exe", L"msedge.exe", L"firefox.exe", L"brave.exe", L"opera.exe",
    L"vivaldi.exe", L"browser.exe", L"whale.exe", L"explorer.exe", L"excel.exe", 0,
};

static int swallow(DWORD vk)
{
    if (vk < 256) g_swallowed[vk] = 1;
    return 1;
}

static int is_elevated_token(HANDLE proc)
{
    HANDLE tok;
    TOKEN_ELEVATION te;
    DWORD n;
    int el = -1;
    if (!OpenProcessToken(proc, TOKEN_QUERY, &tok)) return -1;
    if (GetTokenInformation(tok, TokenElevation, &te, sizeof te, &n)) el = te.TokenIsElevated != 0;
    CloseHandle(tok);
    return el;
}

static int name_in(const wchar_t *exe, const wchar_t *const *list)
{
    int k;
    for (k = 0; list[k]; k++)
        if (lstrcmpiW(exe, list[k]) == 0) return 1;
    return 0;
}

static void classify_fg(HWND fg)
{
    static wchar_t path[MAX_PATH];
    DWORD pid = 0, len = MAX_PATH;
    HANDLE h;
    g_fg_blocked = g_fg_pad = g_fg_excluded = 0;
    if (!fg) return;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId()) return;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) {
        g_fg_blocked = !g_self_elevated;
        return;
    }
    /* UIPI: quyền thường không gửi được phím vào cửa sổ quyền admin. */
    if (!g_self_elevated) g_fg_blocked = is_elevated_token(h) != 0;
    if (QueryFullProcessImageNameW(h, 0, path, &len)) {
        const wchar_t *exe = path_base(path);
        g_fg_pad = name_in(exe, PAD_APPS);
        g_fg_excluded = exclude_has(exe);
    }
    CloseHandle(h);
}

void engine_init(void)
{
    telex_init(&g_tx);
    g_caps = GetKeyState(VK_CAPITAL) & 1;
    g_self_elevated = is_elevated_token(GetCurrentProcess()) == 1;
}

/* Phím tắt chỉ có phím bổ trợ cần từ 2 phím; phím tắt có phím thường cần Ctrl, Alt hoặc Win. */
int hotkey_valid(DWORD hk)
{
    DWORD m = HOTKEY_MODS(hk), n = 0, b;
    for (b = m; b; b &= b - 1) n++;
    if (m & ~(DWORD)(HK_CTRL | HK_SHIFT | HK_ALT | HK_WIN)) return 0;
    return HOTKEY_VK(hk) ? (m & (HK_CTRL | HK_ALT | HK_WIN)) != 0 : n >= 2;
}

void engine_set_hotkey(DWORD hk)
{
    if (hotkey_valid(hk)) InterlockedExchange((volatile LONG *)&g_hotkey, (LONG)hk);
    g_combo = 0;
}

DWORD engine_hotkey(void) { return g_hotkey; }
void engine_mouse_click(void) { g_mouse_reset = 1; }
void engine_set_notify(HWND w) { g_notify = w; }
int engine_is_elevated(void) { return g_self_elevated; }
int engine_enabled(void) { return g_enabled != 0; }

void engine_toggle(void)
{
    HWND w = g_notify;
    InterlockedXor(&g_enabled, 1);
    g_mouse_reset = 1;
    if (w) PostMessageW(w, WM_GTV_STATE, 0, 0);
}

static DWORD hk_bit(DWORD vk)
{
    switch (vk) {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return HK_CTRL;
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return HK_SHIFT;
    case VK_MENU: case VK_LMENU: case VK_RMENU: return HK_ALT;
    case VK_LWIN: case VK_RWIN: return HK_WIN;
    }
    return 0;
}

static DWORD hk_mods(DWORD mods)
{
    return ((mods & (MOD_LCTRL | MOD_RCTRL)) ? HK_CTRL : 0) |
           ((mods & (MOD_LSHIFT | MOD_RSHIFT)) ? HK_SHIFT : 0) |
           ((mods & (MOD_LALT | MOD_RALT)) ? HK_ALT : 0) |
           ((mods & (MOD_LWIN | MOD_RWIN)) ? HK_WIN : 0);
}

/* Chặn phím thường của tổ hợp thì app thấy Alt/Win nhấn rồi nhả trơn và mở thanh menu
 * hoặc Start; chèn một phím ảo để tránh. */
static void mask_alt_win(void)
{
    emit_push(0, 0, 0, 0xE8, 0, 0, 0);
    emit_push(0, 0, 0, 0xE8, 0, KEYEVENTF_KEYUP, 0);
}

/* Phím tắt chỉ có phím bổ trợ: nhấn đủ rồi nhả, không kèm phím nào khác. */
static void track_mod_hotkey(DWORD vk, DWORD flags, DWORD mods)
{
    DWORD bit = hk_bit(vk);
    if (HOTKEY_VK(g_hotkey)) return;
    if (!(flags & LLKHF_UP)) g_combo = bit && (hk_mods(mods) | bit) == HOTKEY_MODS(g_hotkey);
    else if (bit && g_combo) {
        g_combo = 0;
        engine_toggle();
    }
}

static int is_key_hotkey(DWORD vk, DWORD mods)
{
    return HOTKEY_VK(g_hotkey) && vk == HOTKEY_VK(g_hotkey) && hk_mods(mods) == HOTKEY_MODS(g_hotkey);
}

static int is_modifier(DWORD vk)
{
    switch (vk) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU: case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN: case VK_CAPITAL:
    case VK_NUMLOCK: case VK_SCROLL:
        return 1;
    }
    return vk >= VK_VOLUME_MUTE && vk <= VK_LAUNCH_APP2; /* phím media */
}

/* Phím chèn đúng một ký tự, Backspace một lần là xóa được. */
static int inserts_one_char(DWORD vk)
{
    if (vk == VK_SPACE) return 1;
    if (vk >= '0' && vk <= '9') return 1;
    if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE && vk != VK_SEPARATOR) return 1;
    if (vk >= VK_OEM_1 && vk <= VK_OEM_3) return 1;
    if (vk >= VK_OEM_4 && vk <= VK_OEM_8) return 1;
    return vk == VK_OEM_102;
}

static int replay_if_pending(const KBDLLHOOKSTRUCT *k)
{
    DWORD fl;
    if (!emit_pending() || g_fg_blocked) return 0;
    fl = (k->flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0;
    if (k->flags & LLKHF_UP) fl |= KEYEVENTF_KEYUP;
    return emit_push(0, 0, 0, (WORD)k->vkCode, (WORD)k->scanCode, fl, 0);
}

int engine_key(const KBDLLHOOKSTRUCT *k, DWORD mods, HWND fg)
{
    DWORD vk = k->vkCode;
    TelexEdit e;
    int sw;

    e.backspaces = e.len = 0;
    track_mod_hotkey(vk, k->flags, mods);

    if (k->flags & LLKHF_UP) {
        if (vk == VK_CAPITAL) g_caps_down = 0;
        if (vk < 256 && g_swallowed[vk]) { g_swallowed[vk] = 0; return 1; }
        return replay_if_pending(k);
    }
    if (vk < 256) g_swallowed[vk] = 0; /* lần nhả trước có thể đã bị mất */
    if (is_key_hotkey(vk, mods)) {
        engine_toggle();
        if (HOTKEY_MODS(g_hotkey) & (HK_ALT | HK_WIN)) mask_alt_win();
        return swallow(vk);
    }
    if (vk == VK_CAPITAL) {
        if (!g_caps_down) g_caps ^= 1;
        g_caps_down = 1;
        return replay_if_pending(k);
    }

    if (fg != g_fg || exclude_version() != g_ex_version) {
        g_fg = fg;
        g_ex_version = exclude_version();
        classify_fg(fg);
        telex_reset(&g_tx);
    }
    if (g_mouse_reset) {
        g_mouse_reset = 0;
        telex_reset(&g_tx);
    }
    if (is_modifier(vk)) return replay_if_pending(k);

    if (!g_enabled || g_fg_blocked || g_fg_excluded ||
        (mods & (MOD_LCTRL | MOD_RCTRL | MOD_LALT | MOD_RALT | MOD_LWIN | MOD_RWIN))) {
        telex_reset(&g_tx);
        sw = 0;
    } else if (vk == VK_BACK) {
        sw = telex_backspace(&g_tx, &e);
    } else if (vk >= 'A' && vk <= 'Z') {
        int upper = ((mods & (MOD_LSHIFT | MOD_RSHIFT)) != 0) ^ g_caps;
        sw = telex_key(&g_tx, (wchar_t)(upper ? vk : vk + 32), &e);
    } else if (inserts_one_char(vk)) {
        telex_break(&g_tx);
        sw = 0;
    } else {
        telex_reset(&g_tx);
        sw = 0;
    }

    if (sw) {
        if (emit_push(e.backspaces, e.text, e.len, 0, 0, 0, g_fg_pad && e.backspaces > 0))
            return swallow(vk);
        telex_reset(&g_tx); /* hàng đợi đầy: bỏ từ đang gõ, cho phím gốc đi qua */
        return 0;
    }
    return replay_if_pending(k);
}
