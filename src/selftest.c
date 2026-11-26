/* Giả lập gõ vào ô EDIT và RichEdit của cửa sổ thử; chỉ gõ khi cửa sổ thử có focus. */
#include "gtv.h"

static void pump_for(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    for (;;) {
        DWORD now = GetTickCount();
        if ((LONG)(end - now) <= 0) break;
        MsgWaitForMultipleObjects(0, 0, FALSE, end - now, QS_ALLINPUT);
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

static INPUT g_tin[512];

static void tkey(UINT *n, WORD vk, DWORD flags)
{
    INPUT *in = &g_tin[(*n)++];
    in->type = INPUT_KEYBOARD;
    in->ki.wVk = vk;
    in->ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    in->ki.dwFlags = flags;
    in->ki.time = 0;
    in->ki.dwExtraInfo = 0; /* như người dùng gõ */
}

/*
 * Giả lập người dùng gõ. '<' là Backspace. burst=1: dồn cả chuỗi vào một lần
 * SendInput (gõ nhanh nhất có thể), burst=0: mỗi phím một lần SendInput.
 */
static void type_keys(const char *s, int burst)
{
    UINT n = 0;
    for (; *s && n < 500; s++) {
        WORD vk;
        int shift = 0;
        if (*s == '<') vk = VK_BACK;
        else if (*s == ' ') vk = VK_SPACE;
        else if (*s >= 'A' && *s <= 'Z') { vk = (WORD)*s; shift = 1; }
        else vk = (WORD)(*s - 'a' + 'A');
        if (shift) tkey(&n, VK_LSHIFT, 0);
        tkey(&n, vk, 0);
        tkey(&n, vk, KEYEVENTF_KEYUP);
        if (shift) tkey(&n, VK_LSHIFT, KEYEVENTF_KEYUP);
        if (!burst) { SendInput(n, g_tin, sizeof(INPUT)); n = 0; }
    }
    if (n) SendInput(n, g_tin, sizeof(INPUT));
}

typedef struct {
    const char *keys;
    const wchar_t *expect;
} Case;

static const Case CASES[] = {
    { "tieengs vieetj", L"tiếng việt" },
    { "Vieetj Nam", L"Việt Nam" },
    { "hoaf binhf", L"hoà bình" },
    { "nguowif dduowngf quoocs", L"người đường quốc" },
    { "vieetj<t", L"việt" },
    { "dduwowngf<<", L"đườ" },
    { "tieengs<<<<<<", L"" },
    { "ass aaa ddd", L"as aa dd" },
    { "class fix", L"class fix" },
    { "TIEENGS VIEETJ", L"TIẾNG VIỆT" },
    { "Tooi yeeu tieengs Vieetj Nam lawms",L"Tôi yêu tiếng Việt Nam lắm" },
};

static int run_cases(HWND wnd, HWND ctl, const wchar_t *name, int burst)
{
    static wchar_t got[256];
    int i, fails = 0;
    for (i = 0; i < (int)(sizeof CASES / sizeof CASES[0]); i++) {
        if (GetForegroundWindow() != wnd) {
            log_w(L"SAI: mất focus, dừng để không gõ nhầm vào app khác");
            return 100;
        }
        SetFocus(ctl);
        SetWindowTextW(ctl, L"");
        type_keys(" ", burst); /* kết thúc từ cũ trong engine */
        pump_for(100);
        SetWindowTextW(ctl, L"");
        type_keys(CASES[i].keys, burst);
        pump_for(400);
        GetWindowTextW(ctl, got, 256);
        if (lstrcmpW(got, CASES[i].expect) != 0) {
            fails++;
            log_w(L"SAI  [%s, %s] %S -> \"%s\", mong đợi \"%s\"", name,
                  burst ? L"dồn" : L"từng phím", CASES[i].keys, got, CASES[i].expect);
        }
    }
    log_w(L"%s, %s: %d/%d đạt", name, burst ? L"gõ dồn 1 lần SendInput" : L"từng phím",
          (int)(sizeof CASES / sizeof CASES[0]) - fails, (int)(sizeof CASES / sizeof CASES[0]));
    return fails;
}

int selftest(void)
{
    WNDCLASSW wc = {0};
    HWND wnd, edit, rich;
    int fails = 0;

    LoadLibraryW(L"msftedit.dll");
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(0);
    wc.lpszClassName = L"GtvSelftest";
    RegisterClassW(&wc);
    wnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"gotiengviet selftest",
                          WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 200, 520, 140, 0, 0, wc.hInstance, 0);
    edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
                           10, 10, 480, 28, wnd, 0, wc.hInstance, 0);
    rich = CreateWindowExW(0, L"RICHEDIT50W", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                           10, 50, 480, 28, wnd, 0, wc.hInstance, 0);
    SetForegroundWindow(wnd);
    SetFocus(edit);
    pump_for(300);
    if (GetForegroundWindow() != wnd) {
        log_w(L"SAI: không lấy được focus cho cửa sổ thử, dừng để không gõ nhầm vào app khác");
        return 2;
    }

    fails += run_cases(wnd, edit, L"EDIT", 0);
    fails += run_cases(wnd, edit, L"EDIT", 1);
    if (rich) {
        fails += run_cases(wnd, rich, L"RichEdit", 0);
        fails += run_cases(wnd, rich, L"RichEdit", 1);
    } else {
        log_w(L"bỏ qua RichEdit: không tạo được control");
    }
    DestroyWindow(wnd);

    pump_for(300);
    log_w(L"sự kiện đã phát: %d, hook thấy và bỏ qua: %d, bị rơi do đầy hàng đợi: %d",
          emit_sent_events(), hook_skipped_self(), hook_dropped());
    if (emit_sent_events() != hook_skipped_self()) fails++;
    if (hook_dropped()) fails++;
    log_w(fails ? L"selftest SAI" : L"selftest ĐẠT");
    return fails ? 1 : 0;
}
