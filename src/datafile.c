/*
 * File danh sách trong %APPDATA%\gotiengviet (app loại trừ, gõ tắt): UTF-8, mỗi dòng một
 * mục, # là chú thích. Chỉ luồng chính đọc/ghi. Cửa sổ danh sách ghi file; sửa tay bằng
 * trình soạn thảo khác thì lưu xong bộ gõ tự nạp lại.
 */
#include "gtv.h"

int datafile_init(DataFile *d, const wchar_t *name)
{
    DWORD n = GetEnvironmentVariableW(L"APPDATA", d->path, MAX_PATH);
    if (!n || n >= MAX_PATH) {
        d->path[0] = 0;
        return 0;
    }
    lstrcatW(d->path, L"\\gotiengviet\\");
    lstrcatW(d->path, name);
    d->mtime.dwLowDateTime = d->mtime.dwHighDateTime = 0;
    datafile_changed(d);
    return 1;
}

int datafile_changed(DataFile *d)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    FILETIME t = {0, 0};
    if (!d->path[0]) return 0;
    if (GetFileAttributesExW(d->path, GetFileExInfoStandard, &a)) t = a.ftLastWriteTime;
    if (CompareFileTime(&t, &d->mtime) == 0) return 0;
    d->mtime = t;
    return 1;
}

int datafile_read(const DataFile *d, wchar_t *text, int cap)
{
    static char raw[DATAFILE_MAX];
    HANDLE f;
    DWORD got = 0;
    int len;
    if (cap > DATAFILE_MAX) cap = DATAFILE_MAX;
    f = CreateFileW(d->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (f != INVALID_HANDLE_VALUE) {
        ReadFile(f, raw, (DWORD)cap - 1, &got, 0);
        CloseHandle(f);
    }
    len = MultiByteToWideChar(CP_UTF8, 0, raw, (int)got, text, cap - 1);
    text[len] = 0;
    return len;
}

static int is_space(wchar_t c) { return c == ' ' || c == '\t' || c == '\r' || c == 0xFEFF; }

/* Dòng kế tiếp đã bỏ khoảng trắng hai đầu, bỏ qua dòng trống và chú thích. */
int datafile_next_line(const wchar_t **s, const wchar_t **a, const wchar_t **b)
{
    while (**s) {
        const wchar_t *p = *s, *q;
        while (**s && **s != '\n') (*s)++;
        q = *s;
        if (**s) (*s)++;
        while (p < q && is_space(*p)) p++;
        while (q > p && is_space(q[-1])) q--;
        if (p == q || *p == '#') continue;
        *a = p;
        *b = q;
        return 1;
    }
    return 0;
}

/* Ghi lại cả file: dòng chú thích đầu file rồi nội dung (UTF-16, đổi sang UTF-8). */
int datafile_write(DataFile *d, const char *header, const wchar_t *body, int len)
{
    wchar_t dir[MAX_PATH + 32];
    HANDLE f, heap = GetProcessHeap();
    DWORD done;
    int n, ok = 0;
    char *utf8;

    if (!d->path[0]) return 0;
    lstrcpyW(dir, d->path);
    *(wchar_t *)path_base(dir) = 0;
    CreateDirectoryW(dir, 0);
    n = WideCharToMultiByte(CP_UTF8, 0, body, len, 0, 0, 0, 0);
    utf8 = (char *)HeapAlloc(heap, 0, (SIZE_T)n + 1);
    if (!utf8) return 0;
    WideCharToMultiByte(CP_UTF8, 0, body, len, utf8, n, 0, 0);
    f = CreateFileW(d->path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (f != INVALID_HANDLE_VALUE) {
        ok = WriteFile(f, header, (DWORD)lstrlenA(header), &done, 0) && WriteFile(f, utf8, (DWORD)n, &done, 0);
        CloseHandle(f);
    }
    HeapFree(heap, 0, utf8);
    datafile_changed(d); /* ghi nhận thời gian sửa mới, khỏi nạp lại lần nữa */
    return ok;
}

const wchar_t *path_base(const wchar_t *path)
{
    const wchar_t *base = path;
    for (; *path; path++) if (*path == '\\' || *path == '/') base = path + 1;
    return base;
}
