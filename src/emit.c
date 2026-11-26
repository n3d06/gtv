/*
 * Luồng phát ký tự. g_pending đếm số lần phát mà hook chưa thấy GTV_MAGIC_END: lúc
 * SendInput trả về, phím người dùng gõ trước có thể vẫn đang xếp hàng phía trước.
 */
#include "gtv.h"

#define QSIZE 64
#define MAX_BS 64
#define PAD_CHAR ' ' /* thanh địa chỉ không bỏ qua dấu cách, và URL không có dấu cách */

typedef struct {
    int bs, len;
    wchar_t text[TELEX_MAX_CHARS];
    WORD vk, scan;
    DWORD kflags;
    int pad;
} EmitReq;

static EmitReq g_q[QSIZE];
static int g_qhead, g_qtail;
static SRWLOCK g_lock = SRWLOCK_INIT;
static HANDLE g_evt;
static HANDLE g_thread;
static volatile LONG g_quit;
static volatile LONG g_pending;
static volatile LONG g_sent_events;
static volatile DWORD g_last_push;

static INPUT g_in[2 * (MAX_BS + TELEX_MAX_CHARS + 2) + 1]; /* tĩnh: tránh __chkstk khi không có CRT */

static void key(INPUT *in, WORD vk, WORD scan, DWORD flags)
{
    in->type = INPUT_KEYBOARD;
    in->ki.wVk = vk;
    in->ki.wScan = scan;
    in->ki.dwFlags = flags;
    in->ki.time = 0;
    in->ki.dwExtraInfo = GTV_MAGIC;
}

static void send_req(const EmitReq *r)
{
    UINT n = 0, sent;
    int i, bs = r->bs;
    if (r->pad) {
        key(&g_in[n++], 0, PAD_CHAR, KEYEVENTF_UNICODE);
        key(&g_in[n++], 0, PAD_CHAR, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP);
        bs++;
    }
    for (i = 0; i < bs; i++) {
        key(&g_in[n++], VK_BACK, 0x0E, 0);
        key(&g_in[n++], VK_BACK, 0x0E, KEYEVENTF_KEYUP);
    }
    for (i = 0; i < r->len; i++) {
        key(&g_in[n++], 0, r->text[i], KEYEVENTF_UNICODE);
        key(&g_in[n++], 0, r->text[i], KEYEVENTF_UNICODE | KEYEVENTF_KEYUP);
    }
    if (r->vk) key(&g_in[n++], r->vk, r->scan, r->kflags);
    if (!n) {
        InterlockedDecrement(&g_pending);
        return;
    }
    g_in[n - 1].ki.dwExtraInfo = GTV_MAGIC_END;
    sent = SendInput(n, g_in, sizeof(INPUT));
    InterlockedExchangeAdd(&g_sent_events, (LONG)sent);
    if (sent != n) InterlockedDecrement(&g_pending); /* hook sẽ không thấy sự kiện cuối */
}

static int pop(EmitReq *r)
{
    int ok = 0;
    AcquireSRWLockExclusive(&g_lock);
    if (g_qtail != g_qhead) {
        *r = g_q[g_qtail];
        g_qtail = (g_qtail + 1) % QSIZE;
        ok = 1;
    }
    ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

static DWORD WINAPI emit_thread(void *arg)
{
    static EmitReq r;
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    while (!g_quit) {
        WaitForSingleObject(g_evt, INFINITE);
        while (pop(&r)) send_req(&r);
    }
    return 0;
}

int emit_start(void)
{
    g_evt = CreateEventW(0, FALSE, FALSE, 0);
    if (!g_evt) return 0;
    g_thread = CreateThread(0, 64 * 1024, emit_thread, 0, STACK_SIZE_PARAM_IS_A_RESERVATION, 0);
    return g_thread != 0;
}

void emit_stop(void)
{
    if (!g_thread) return;
    g_quit = 1;
    SetEvent(g_evt);
    WaitForSingleObject(g_thread, 2000);
    CloseHandle(g_thread);
    g_thread = 0;
}

void emit_passed(void)
{
    if (InterlockedDecrement(&g_pending) < 0) InterlockedExchange(&g_pending, 0);
}

int emit_pending(void)
{
    if (g_pending <= 0) return 0;
    if (GetTickCount() - g_last_push > 1000) { /* phòng hờ kẹt: không giữ phím mãi */
        InterlockedExchange(&g_pending, 0);
        return 0;
    }
    return 1;
}

LONG emit_sent_events(void) { return g_sent_events; }

int emit_push(int bs, const wchar_t *text, int len, WORD vk, WORD scan, DWORD kflags, int pad)
{
    int i, next, ok = 0;
    if (bs < 0 || bs > MAX_BS || len < 0 || len > TELEX_MAX_CHARS) return 0;
    AcquireSRWLockExclusive(&g_lock);
    next = (g_qhead + 1) % QSIZE;
    if (next != g_qtail) {
        EmitReq *r = &g_q[g_qhead];
        r->bs = bs;
        r->len = len;
        for (i = 0; i < len; i++) r->text[i] = text[i];
        r->vk = vk;
        r->scan = scan;
        r->kflags = kflags;
        r->pad = pad;
        g_qhead = next;
        g_last_push = GetTickCount();
        InterlockedIncrement(&g_pending);
        ok = 1;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (ok) SetEvent(g_evt);
    return ok;
}
