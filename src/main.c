/*
 * gotiengviet.exe — bộ gõ Telex.
 *
 *   gotiengviet.exe              chạy ngầm, icon dưới khay, Ctrl+Shift bật/tắt
 *   gotiengviet.exe --log        in mọi sự kiện phím ra console, Ctrl+C để thoát
 *   gotiengviet.exe --selftest   tự mở cửa sổ thử, giả lập gõ nhanh, kiểm tra kết quả
 */
#include "gtv.h"

static HANDLE g_key_evt;
static HANDLE g_worker;
static volatile LONG g_quit;
static DWORD g_main_tid;

/* Chỉ chạy khi bật --log. */
static DWORD WINAPI worker(void *arg)
{
    KeyEvent e;
    (void)arg;
    while (!g_quit) {
        WaitForSingleObject(g_key_evt, INFINITE);
        while (hook_pop(&e))
            log_w(L"%s vk=0x%02X sc=0x%02X mods=0x%02X inj=%d %s",
                  (e.flags & LLKHF_UP) ? L"up  " : L"down",
                  e.vk, e.scan, e.mods, (e.flags & LLKHF_INJECTED) ? 1 : 0,
                  e.eaten ? L"CHẶN" : L"");
    }
    return 0;
}

static BOOL WINAPI on_ctrl(DWORD type)
{
    (void)type;
    PostThreadMessageW(g_main_tid, WM_QUIT, 0, 0);
    return TRUE;
}

/*
 * Hai bản cùng chạy sẽ thay chữ hai lần. Chờ tối đa 3 giây vì khi chuyển quyền, bản
 * mới khởi động lúc bản cũ đang thoát.
 */
static int acquire_single_instance(void)
{
    int i;
    for (i = 0; i < 30; i++) {
        HANDLE m = CreateMutexW(0, FALSE, L"Local\\gotiengviet");
        if (m && GetLastError() != ERROR_ALREADY_EXISTS) return 1;
        if (m) CloseHandle(m);
        Sleep(100);
    }
    return 0;
}

static int has_arg(const wchar_t *cmd, const wchar_t *arg)
{
    const wchar_t *p;
    for (p = cmd; *p; p++) {
        const wchar_t *a = arg, *q = p;
        while (*a && *q == *a) { a++; q++; }
        if (!*a && (*q == 0 || *q == ' ')) return 1;
    }
    return 0;
}

void start(void)
{
    const wchar_t *cmd = GetCommandLineW();
    int test = has_arg(cmd, L"--selftest");
    int code = 0;
    MSG msg;

    g_main_tid = GetCurrentThreadId();
    log_init(test || has_arg(cmd, L"--log"));
    SetConsoleCtrlHandler(on_ctrl, TRUE);

    if (!acquire_single_instance()) {
        log_w(L"gotiengviet đang chạy rồi (xem icon dưới khay).");
        ExitProcess(4);
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    exclude_init();
    if (log_enabled()) g_key_evt = CreateEventW(0, FALSE, FALSE, 0);
    if (!emit_start() || !hook_start(g_key_evt)) {
        log_w(L"Không khởi động được hook (lỗi %u)", GetLastError());
        ExitProcess(3);
    }
    if (g_key_evt) g_worker = CreateThread(0, 64 * 1024, worker, 0, STACK_SIZE_PARAM_IS_A_RESERVATION, 0);

    if (test) {
        code = selftest();
    } else {
        log_w(L"Đang chạy. Thoát bằng menu icon khay hoặc Ctrl+C.");
        if (!tray_run()) {
            while (GetMessageW(&msg, 0, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    hook_stop();
    emit_stop();
    g_quit = 1;
    if (g_worker) {
        SetEvent(g_key_evt);
        WaitForSingleObject(g_worker, 1000);
    }
    ExitProcess(code);
}
