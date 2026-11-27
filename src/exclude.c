/*
 * App loại trừ: %APPDATA%\gotiengviet\exclude.txt, mỗi dòng một tên exe.
 * Luồng chính đọc/ghi file; luồng hook chỉ tra danh sách trong bộ nhớ.
 */
#include "gtv.h"

#define FILE_MAX 8192

static wchar_t g_apps[EXCLUDE_MAX][EXCLUDE_NAME_LEN];
static int g_count;
static SRWLOCK g_lock = SRWLOCK_INIT;
static volatile LONG g_version;
static DataFile g_file;

static const char HEADER[] =
    "# Danh sách app không gõ tiếng Việt: mỗi dòng một tên file exe, ví dụ code.exe\r\n"
    "# Dòng bắt đầu bằng # là chú thích. Lưu file là bộ gõ tự cập nhật.\r\n";

static void load(void)
{
    static wchar_t text[FILE_MAX];
    static wchar_t apps[EXCLUDE_MAX][EXCLUDE_NAME_LEN];
    const wchar_t *s = text, *a, *b;
    int n = 0, i;

    datafile_read(&g_file, text, FILE_MAX);
    while (n < EXCLUDE_MAX && datafile_next_line(&s, &a, &b))
        if (b - a < EXCLUDE_NAME_LEN) lstrcpynW(apps[n++], a, (int)(b - a) + 1);

    AcquireSRWLockExclusive(&g_lock);
    for (i = 0; i < n; i++) lstrcpynW(g_apps[i], apps[i], EXCLUDE_NAME_LEN);
    g_count = n;
    InterlockedIncrement(&g_version);
    ReleaseSRWLockExclusive(&g_lock);
}

void exclude_init(void)
{
    if (datafile_init(&g_file, L"exclude.txt")) load();
}

int exclude_reload_if_changed(void)
{
    if (!datafile_changed(&g_file)) return 0;
    load();
    return 1;
}

LONG exclude_version(void) { return g_version; }
int exclude_count(void) { return g_count; }
const wchar_t *exclude_get(int i) { return g_apps[i]; }

int exclude_has(const wchar_t *exe)
{
    int i, hit = 0;
    AcquireSRWLockShared(&g_lock);
    for (i = 0; i < g_count && !hit; i++) hit = lstrcmpiW(g_apps[i], exe) == 0;
    ReleaseSRWLockShared(&g_lock);
    return hit;
}

int exclude_save(const wchar_t *const *names, int n)
{
    static wchar_t body[EXCLUDE_MAX * (EXCLUDE_NAME_LEN + 2)];
    int i, len = 0, ok;
    for (i = 0; i < n && i < EXCLUDE_MAX; i++) len += wsprintfW(body + len, L"%s\r\n", names[i]);
    ok = datafile_write(&g_file, HEADER, body, len);
    load();
    return ok;
}
