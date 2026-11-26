/*
 * App loại trừ: %APPDATA%\gotiengviet\exclude.txt, mỗi dòng một tên exe, # là chú thích.
 * Luồng chính đọc file; luồng hook chỉ tra danh sách trong bộ nhớ.
 */
#include "gtv.h"
#include <shellapi.h>

#define MAX_APPS 64
#define NAME_LEN 64
#define FILE_MAX 8192

static wchar_t g_apps[MAX_APPS][NAME_LEN];
static int g_count;
static SRWLOCK g_lock = SRWLOCK_INIT;
static volatile LONG g_version;
static wchar_t g_dir[MAX_PATH], g_path[MAX_PATH + 16];
static FILETIME g_mtime;

static const char HEADER[] =
    "# Danh sách app không gõ tiếng Việt: mỗi dòng một tên file exe, ví dụ code.exe\r\n"
    "# Dòng bắt đầu bằng # là chú thích. Lưu file là bộ gõ tự cập nhật.\r\n";

const wchar_t *path_base(const wchar_t *path)
{
    const wchar_t *base = path;
    for (; *path; path++) if (*path == '\\' || *path == '/') base = path + 1;
    return base;
}

static int file_mtime(FILETIME *t)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(g_path, GetFileExInfoStandard, &a)) return 0;
    *t = a.ftLastWriteTime;
    return 1;
}

static int is_space(wchar_t c) { return c == ' ' || c == '\t' || c == '\r' || c == 0xFEFF; }

static int parse(const wchar_t *s, wchar_t apps[][NAME_LEN])
{
    int n = 0;
    while (*s && n < MAX_APPS) {
        const wchar_t *a = s, *b;
        while (*s && *s != '\n') s++;
        b = s;
        if (*s) s++;
        while (a < b && is_space(*a)) a++;
        while (b > a && is_space(b[-1])) b--;
        if (a == b || *a == '#' || b - a >= NAME_LEN) continue;
        lstrcpynW(apps[n++], a, (int)(b - a) + 1);
    }
    return n;
}

static void load(void)
{
    static char raw[FILE_MAX];
    static wchar_t text[FILE_MAX];
    static wchar_t apps[MAX_APPS][NAME_LEN];
    HANDLE f;
    DWORD got = 0;
    int len, n, i;

    f = CreateFileW(g_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (f != INVALID_HANDLE_VALUE) {
        ReadFile(f, raw, FILE_MAX - 1, &got, 0);
        CloseHandle(f);
    }
    len = MultiByteToWideChar(CP_UTF8, 0, raw, (int)got, text, FILE_MAX - 1);
    text[len] = 0;
    n = parse(text, apps);

    AcquireSRWLockExclusive(&g_lock);
    for (i = 0; i < n; i++) lstrcpynW(g_apps[i], apps[i], NAME_LEN);
    g_count = n;
    InterlockedIncrement(&g_version);
    ReleaseSRWLockExclusive(&g_lock);
}

void exclude_init(void)
{
    DWORD n = GetEnvironmentVariableW(L"APPDATA", g_dir, MAX_PATH - 16);
    if (!n || n >= MAX_PATH - 16) return;
    lstrcatW(g_dir, L"\\gotiengviet");
    wsprintfW(g_path, L"%s\\exclude.txt", g_dir);
    file_mtime(&g_mtime);
    load();
}

void exclude_reload_if_changed(void)
{
    FILETIME t = {0};
    if (!g_path[0]) return;
    file_mtime(&t);
    if (CompareFileTime(&t, &g_mtime) == 0) return;
    g_mtime = t;
    load();
}

LONG exclude_version(void) { return g_version; }

int exclude_has(const wchar_t *exe)
{
    int i, hit = 0;
    AcquireSRWLockShared(&g_lock);
    for (i = 0; i < g_count && !hit; i++) hit = lstrcmpiW(g_apps[i], exe) == 0;
    ReleaseSRWLockShared(&g_lock);
    return hit;
}

void exclude_open_file(void)
{
    if (!g_path[0]) return;
    if (GetFileAttributesW(g_path) == INVALID_FILE_ATTRIBUTES) {
        HANDLE f;
        DWORD done;
        CreateDirectoryW(g_dir, 0);
        f = CreateFileW(g_path, GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
        if (f != INVALID_HANDLE_VALUE) {
            WriteFile(f, HEADER, sizeof HEADER - 1, &done, 0);
            CloseHandle(f);
        }
    }
    ShellExecuteW(0, L"open", L"notepad.exe", g_path, 0, SW_SHOWNORMAL);
}
