/* Log ra stdout nếu là file/pipe (UTF-8), không thì ra console. */
#include <stdarg.h>
#include "gtv.h"

static int g_on;
static HANDLE g_out;
static int g_is_console;
static SRWLOCK g_lock = SRWLOCK_INIT;

void log_init(int enabled)
{
    HANDLE h;
    g_on = enabled;
    if (!enabled) return;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_CHAR) {
        g_out = h;
        return;
    }
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
    g_out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (g_out == INVALID_HANDLE_VALUE) { g_out = 0; g_on = 0; return; }
    g_is_console = 1;
}

int log_enabled(void) { return g_on; }

void log_w(const wchar_t *fmt, ...)
{
    static wchar_t buf[1024];
    static char utf8[3 * 1024];
    va_list ap;
    int n;
    DWORD done;
    if (!g_on) return;
    AcquireSRWLockExclusive(&g_lock);
    va_start(ap, fmt);
    n = wvsprintfW(buf, fmt, ap);
    va_end(ap);
    if (n > 1021) n = 1021;
    buf[n++] = '\r';
    buf[n++] = '\n';
    if (g_is_console) {
        WriteConsoleW(g_out, buf, n, &done, 0);
    } else {
        int m = WideCharToMultiByte(CP_UTF8, 0, buf, n, utf8, sizeof utf8, 0, 0);
        WriteFile(g_out, utf8, m, &done, 0);
    }
    ReleaseSRWLockExclusive(&g_lock);
}
