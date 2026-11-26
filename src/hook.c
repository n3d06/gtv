/* Luồng hook bàn phím + chuột. Callback không làm I/O, chỉ hỏi engine rồi trả về. */
#include "gtv.h"

#define RING_SIZE 256 /* hàng đợi log, lũy thừa của 2 */

static KeyEvent g_ring[RING_SIZE];
static volatile LONG g_head, g_tail;
static volatile LONG g_dropped;
static volatile LONG g_skipped_self;

static HANDLE g_evt;
static HANDLE g_thread;
static DWORD g_tid;
static HANDLE g_ready;
static HHOOK g_hook;
static HHOOK g_mouse;
static DWORD g_mods;

static DWORD mod_bit(DWORD vk)
{
    switch (vk) {
    case VK_LSHIFT: return MOD_LSHIFT;
    case VK_RSHIFT: return MOD_RSHIFT;
    case VK_LCONTROL: return MOD_LCTRL;
    case VK_RCONTROL: return MOD_RCTRL;
    case VK_LMENU: return MOD_LALT;
    case VK_RMENU: return MOD_RALT;
    case VK_LWIN: return MOD_LWIN;
    case VK_RWIN: return MOD_RWIN;
    }
    return 0;
}

/*
 * Hook có thể không thấy lúc nhả phím (Win+L, UAC), làm Ctrl/Alt/Win bị kẹt.
 * Không đối chiếu khi đang chờ phát lại: trạng thái thật lúc đó chưa cập nhật.
 */
static void resync_mods(void)
{
    static const BYTE vks[8] = { /* cùng thứ tự với bit MOD_* */
        VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN,
    };
    int i;
    for (i = 0; i < 8; i++)
        if ((g_mods & (1u << i)) && !(GetAsyncKeyState(vks[i]) & 0x8000)) g_mods &= ~(1u << i);
}

static LRESULT CALLBACK ll_proc(int code, WPARAM wp, LPARAM lp)
{
    int eat = 0;
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *k = (const KBDLLHOOKSTRUCT *)lp;
        if (k->dwExtraInfo == GTV_MAGIC || k->dwExtraInfo == GTV_MAGIC_END) {
            g_skipped_self++;
            if (k->dwExtraInfo == GTV_MAGIC_END) emit_passed();
        } else {
            LONG h = g_head;
            DWORD bit = mod_bit(k->vkCode);
            if (g_mods && !bit && !(k->flags & LLKHF_UP) && !emit_pending()) resync_mods();
            eat = engine_key(k, g_mods, GetForegroundWindow());
            if (g_evt && h - g_tail >= RING_SIZE) {
                g_dropped++;
            } else if (g_evt) {
                KeyEvent *e = &g_ring[h & (RING_SIZE - 1)];
                e->vk = k->vkCode;
                e->scan = k->scanCode;
                e->flags = k->flags;
                e->mods = g_mods;
                e->time = k->time;
                e->eaten = eat;
                InterlockedExchange(&g_head, h + 1);
                SetEvent(g_evt);
            }
            if (bit) {
                if (k->flags & LLKHF_UP) g_mods &= ~bit;
                else g_mods |= bit;
            }
        }
    }
    if (eat) return 1;
    return CallNextHookEx(0, code, wp, lp);
}

/* Click có thể dời con trỏ văn bản: engine bỏ từ đang gõ ở phím kế tiếp. */
static LRESULT CALLBACK mouse_proc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION &&
        (wp == WM_LBUTTONDOWN || wp == WM_RBUTTONDOWN || wp == WM_MBUTTONDOWN || wp == WM_XBUTTONDOWN))
        engine_mouse_click();
    return CallNextHookEx(0, code, wp, lp);
}

static DWORD WINAPI hook_thread(void *arg)
{
    MSG msg;
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    PeekMessageW(&msg, 0, WM_USER, WM_USER, PM_NOREMOVE); /* tạo hàng đợi message */
    engine_init();
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, ll_proc, GetModuleHandleW(0), 0);
    g_mouse = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, GetModuleHandleW(0), 0);
    SetEvent(g_ready);
    if (!g_hook) return 1;
    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_mouse) UnhookWindowsHookEx(g_mouse);
    UnhookWindowsHookEx(g_hook);
    g_hook = 0;
    return 0;
}

int hook_start(HANDLE log_evt)
{
    g_evt = log_evt;
    g_ready = CreateEventW(0, TRUE, FALSE, 0);
    if (!g_ready) return 0;
    g_thread = CreateThread(0, 64 * 1024, hook_thread, 0, STACK_SIZE_PARAM_IS_A_RESERVATION, &g_tid);
    if (!g_thread) return 0;
    WaitForSingleObject(g_ready, INFINITE);
    CloseHandle(g_ready);
    return g_hook != 0;
}

void hook_stop(void)
{
    if (!g_thread) return;
    PostThreadMessageW(g_tid, WM_QUIT, 0, 0);
    WaitForSingleObject(g_thread, 2000);
    CloseHandle(g_thread);
    g_thread = 0;
}

int hook_pop(KeyEvent *e)
{
    LONG t = g_tail;
    if (t == g_head) return 0;
    *e = g_ring[t & (RING_SIZE - 1)];
    InterlockedExchange(&g_tail, t + 1);
    return 1;
}

LONG hook_dropped(void) { return g_dropped; }
LONG hook_skipped_self(void) { return g_skipped_self; }
