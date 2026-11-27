/*
 * Test engine (engine.c + telex.c) không cần cửa sổ hay focus: gọi thẳng engine_key
 * với sự kiện phím giả, luồng phát được thay bằng hàm giả áp lệnh sửa lên một ô
 * văn bản mô phỏng. Phím engine cho đi qua thì "app" mô phỏng tự gõ ra chữ.
 */
#include <stdio.h>
#include "../src/gtv.h"

/* ---- ô văn bản mô phỏng ---- */

static wchar_t g_field[256];
static int g_n;
static DWORD g_mods; /* phím bổ trợ đang giữ, theo dõi như hook thật */
static int g_caps;   /* Caps Lock của "hệ thống" */

static void field_clear(void) { g_n = 0; }

/* App nhận một phím đi qua: chèn ký tự tương ứng (tối giản). */
static void app_key(DWORD vk)
{
    if (g_mods & (MOD_LCTRL | MOD_RCTRL | MOD_LALT | MOD_RALT)) return; /* phím tắt */
    if (vk == VK_BACK) { if (g_n) g_n--; return; }
    if (g_n >= 255) return;
    if (vk >= 'A' && vk <= 'Z') {
        int upper = ((g_mods & (MOD_LSHIFT | MOD_RSHIFT)) != 0) ^ g_caps;
        g_field[g_n++] = (wchar_t)(upper ? vk : vk + 32);
    } else if (vk == VK_SPACE) g_field[g_n++] = ' ';
    else if (vk == VK_OEM_PERIOD) g_field[g_n++] = '.';
    else if (vk >= '0' && vk <= '9') g_field[g_n++] = (wchar_t)vk;
}

/* ---- thay cho emit.c ---- */

static int g_pending;    /* giả lập "còn lần phát chưa đi qua" */
static int g_replays;    /* số phím được phát lại */

int emit_pending(void) { return g_pending; }
void emit_passed(void) {}

int emit_push(int bs, const wchar_t *text, int len, WORD vk, WORD scan, DWORD kflags, int pad)
{
    int i;
    (void)scan; (void)pad;
    for (i = 0; i < bs && g_n > 0; i++) g_n--;
    for (i = 0; i < len && g_n < 255; i++) g_field[g_n++] = text[i];
    if (vk) {
        g_replays++;
        if (!(kflags & KEYEVENTF_KEYUP)) app_key(vk);
    }
    return 1;
}

/* ---- thay cho exclude.c (test không có cửa sổ app nào để loại trừ) ---- */

int exclude_has(const wchar_t *exe) { (void)exe; return 0; }
LONG exclude_version(void) { return 0; }
const wchar_t *path_base(const wchar_t *path) { return path; }

/* ---- thay cho macro.c: hai chữ tắt, một cái dài hơn một lần phát ---- */

static const wchar_t LONG_VALUE[] = L"Cộng hòa xã hội chủ nghĩa Việt Nam độc lập";

int macro_lookup(const wchar_t *word, int n, wchar_t *out)
{
    const wchar_t *v = 0;
    if (n == 2 && word[0] == 'k' && word[1] == 'o') v = L"không";
    if (n == 2 && word[0] == 'v' && word[1] == 'n') v = LONG_VALUE;
    if (!v) return 0;
    lstrcpyW(out, v);
    return lstrlenW(v);
}

/* ---- gõ phím ---- */

static DWORD mod_bit(DWORD vk)
{
    switch (vk) {
    case VK_LSHIFT: return MOD_LSHIFT;
    case VK_LCONTROL: return MOD_LCTRL;
    case VK_LMENU: return MOD_LALT;
    }
    return 0;
}

/* Một sự kiện phím; trả 1 nếu engine chặn. */
static int key(DWORD vk, int up)
{
    KBDLLHOOKSTRUCT k = {0};
    int eat;
    k.vkCode = vk;
    k.flags = up ? LLKHF_UP : 0;
    eat = engine_key(&k, g_mods, 0);
    if (!eat && !up) app_key(vk);
    if (vk == VK_CAPITAL && !up) g_caps ^= 1;
    if (mod_bit(vk)) {
        if (up) g_mods &= ~mod_bit(vk);
        else g_mods |= mod_bit(vk);
    }
    return eat;
}

static void tap(DWORD vk) { key(vk, 0); key(vk, 1); }

/* '<' Backspace, ' ' dấu cách, '.' dấu chấm, chữ hoa thì giữ Shift. */
static void type(const char *s)
{
    for (; *s; s++) {
        if (*s == '<') tap(VK_BACK);
        else if (*s == ' ') tap(VK_SPACE);
        else if (*s == '.') tap(VK_OEM_PERIOD);
        else if (*s >= 'A' && *s <= 'Z') { key(VK_LSHIFT, 0); tap((DWORD)*s); key(VK_LSHIFT, 1); }
        else tap((DWORD)(*s - 'a' + 'A'));
    }
}

/* ---- kiểm tra ---- */

static int g_total, g_fail;

static void print_w(const wchar_t *s, int n)
{
    char buf[1024];
    int len = WideCharToMultiByte(CP_UTF8, 0, s, n, buf, sizeof buf - 1, 0, 0);
    buf[len] = 0;
    printf("%s", buf);
}

static void expect_field(const char *name, const wchar_t *want)
{
    g_total++;
    if (g_n == lstrlenW(want) && (g_n == 0 || memcmp(g_field, want, g_n * sizeof(wchar_t)) == 0)) return;
    g_fail++;
    printf("SAI  %-34s -> \"", name);
    print_w(g_field, g_n);
    printf("\", mong đợi \"");
    print_w(want, lstrlenW(want));
    printf("\"\n");
}

static void expect(const char *name, int ok)
{
    g_total++;
    if (ok) return;
    g_fail++;
    printf("SAI  %s\n", name);
}

/* Bắt đầu ca mới: Esc kết thúc từ đang gõ, xóa ô. */
static void fresh(void)
{
    tap(VK_ESCAPE);
    field_clear();
}

static void T(const char *keys, const wchar_t *want)
{
    fresh();
    type(keys);
    expect_field(keys, want);
}

static void ctrl_shift(void)
{
    key(VK_LCONTROL, 0); key(VK_LSHIFT, 0); key(VK_LSHIFT, 1); key(VK_LCONTROL, 1);
}

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);
    engine_init();
    g_caps = GetKeyState(VK_CAPITAL) & 1;
    if (g_caps) tap(VK_CAPITAL); /* test viết cho Caps Lock tắt */

    /* Gõ chữ, Shift, ngắt từ */
    T("tieengs vieetj", L"tiếng việt");
    T("Vieetj Nam", L"Việt Nam");
    T("hoaf binhf.", L"hoà bình.");
    T("class fix www", L"class fix www");
    T("dd vieetj<t", L"đ việt");
    T("bawts <f", L"bằt");
    T("hoa.<f", L"hoà");

    /* Tùy chọn từ bảng điều khiển */
    engine_set_options(OPT_OLD_STYLE);
    T("hoaf", L"hòa");
    engine_set_options(OPT_W_ALONE);
    T("w ww", L"ư w");
    engine_set_options(0);
    T("hoaf w", L"hoà w");

    /* Gõ tắt */
    engine_set_options(OPT_MACRO);
    T("ko ", L"không ");
    T("ko.", L"không.");
    T("toi ko di", L"toi không di");
    T("kos ", L"kó ");
    fresh(); type("ko"); tap('1');
    expect_field("chữ số không bung chữ tắt", L"ko1");
    T("vn ", L"Cộng hòa xã hội chủ nghĩa Việt Nam độc lập ");
    fresh(); type("ko"); tap(VK_RETURN);
    expect_field("ko + Enter", L"không");
    engine_set_options(0);
    T("ko ", L"ko ");

    /* Caps Lock */
    fresh(); tap(VK_CAPITAL); type("vieetj"); tap(VK_CAPITAL);
    expect_field("CapsLock vieetj", L"VIỆT");
    fresh(); tap(VK_CAPITAL); type("Vieetj"); tap(VK_CAPITAL);
    expect_field("CapsLock + Shift Vieetj", L"vIỆT");

    /* Ctrl+phím là phím tắt: đi thẳng tới app và kết thúc từ */
    fresh(); type("a"); key(VK_LCONTROL, 0); tap('C'); key(VK_LCONTROL, 1); type("s");
    expect_field("a, Ctrl+C, s", L"as");
    fresh(); type("a"); key(VK_LMENU, 0); tap('F'); key(VK_LMENU, 1); type("s");
    expect_field("a, Alt+F, s", L"as");

    /* Mũi tên, click chuột: kết thúc từ */
    fresh(); type("a"); tap(VK_LEFT); type("s");
    expect_field("a, mũi tên, s", L"as");
    fresh(); type("a"); engine_mouse_click(); type("s");
    expect_field("a, click chuột, s", L"as");

    /* Ctrl+Shift bật/tắt */
    fresh(); ctrl_shift();
    expect("Ctrl+Shift tắt bộ gõ", !engine_enabled());
    type("as");
    expect_field("đã tắt: as", L"as");
    ctrl_shift();
    expect("Ctrl+Shift bật lại", engine_enabled());
    fresh(); type("as");
    expect_field("bật lại: as", L"á");
    fresh(); key(VK_LCONTROL, 0); key(VK_LSHIFT, 0); tap('T'); key(VK_LSHIFT, 1); key(VK_LCONTROL, 1);
    expect("Ctrl+Shift+T không bật/tắt", engine_enabled());
    fresh(); key(VK_LSHIFT, 0); key(VK_LCONTROL, 0); key(VK_LCONTROL, 1); key(VK_LSHIFT, 1);
    expect("Shift rồi Ctrl cũng bật/tắt", !engine_enabled());
    ctrl_shift();

    /* Phím bị thay lúc nhấn thì nhả cũng bị chặn; phím đi qua thì nhả cũng đi qua */
    fresh(); type("a");
    expect("s nhấn bị chặn", key('S', 0) == 1);
    expect("s nhả bị chặn", key('S', 1) == 1);
    expect("b nhấn đi qua", key('B', 0) == 0);
    expect("b nhả đi qua", key('B', 1) == 0);

    /* Còn lần phát chưa đi qua: mọi phím (kể cả Shift) được phát lại theo thứ tự */
    fresh(); type("a"); g_pending = 1; g_replays = 0;
    expect("b khi đang chờ: bị chặn để phát lại", key('B', 0) == 1);
    expect("b nhả khi đang chờ: phát lại", key('B', 1) == 1);
    expect("Shift khi đang chờ: phát lại", key(VK_LSHIFT, 0) == 1);
    key(VK_LSHIFT, 1);
    g_pending = 0;
    expect("đã phát lại 4 sự kiện", g_replays == 4);
    expect_field("ab qua phát lại", L"ab");

    /* Phím tắt Alt+Z */
    engine_set_hotkey(HOTKEY(HK_ALT, 'Z'));
    fresh(); ctrl_shift();
    expect("Alt+Z: Ctrl+Shift không còn bật/tắt", engine_enabled());
    g_replays = 0;
    key(VK_LMENU, 0);
    expect("Alt+Z: Z nhấn bị chặn", key('Z', 0) == 1);
    expect("Alt+Z: Z nhả bị chặn", key('Z', 1) == 1);
    key(VK_LMENU, 1);
    expect("Alt+Z tắt bộ gõ", !engine_enabled());
    expect("Alt+Z chèn phím ảo để app không nhảy lên thanh menu", g_replays == 2);
    type("as");
    expect_field("Alt+Z đã tắt: as", L"as");
    key(VK_LMENU, 0); tap('Z'); key(VK_LMENU, 1);
    expect("Alt+Z bật lại", engine_enabled());
    T("az", L"az");

    /* Phím tắt Ctrl+Space */
    engine_set_hotkey(HOTKEY(HK_CTRL, VK_SPACE));
    fresh(); key(VK_LCONTROL, 0); tap(VK_SPACE); key(VK_LCONTROL, 1);
    expect("Ctrl+Space tắt bộ gõ", !engine_enabled());
    expect_field("Ctrl+Space không chèn dấu cách", L"");
    key(VK_LCONTROL, 0); tap(VK_SPACE); key(VK_LCONTROL, 1);
    expect("Ctrl+Space bật lại", engine_enabled());
    T("as as", L"á á");

    /* Phím tắt không hợp lệ thì giữ phím tắt cũ */
    engine_set_hotkey(HOTKEY(0, 'A'));
    expect("không nhận phím trơn làm phím tắt", engine_hotkey() == HOTKEY(HK_CTRL, VK_SPACE));
    engine_set_hotkey(HOTKEY(HK_SHIFT, 'A'));
    expect("không nhận Shift+chữ làm phím tắt", engine_hotkey() == HOTKEY(HK_CTRL, VK_SPACE));
    engine_set_hotkey(HOTKEY(HK_CTRL, 0));
    expect("không nhận một phím bổ trợ đơn lẻ", engine_hotkey() == HOTKEY(HK_CTRL, VK_SPACE));

    engine_set_hotkey(HOTKEY_DEFAULT);

    printf("%d/%d test engine đạt\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
