/*
 * Test exclude.c: đọc file, chú thích, khoảng trắng, chữ hoa/thường, nạp lại khi file
 * đổi. APPDATA trỏ sang thư mục tạm để không đụng file thật.
 */
#include <stdio.h>
#include "../src/gtv.h"

static int g_total, g_fail;

static void expect(const char *name, int ok)
{
    g_total++;
    if (ok) return;
    g_fail++;
    printf("SAI  %s\n", name);
}

static void write_file(const wchar_t *path, const char *s)
{
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    DWORD done;
    WriteFile(f, s, lstrlenA(s), &done, 0);
    CloseHandle(f);
}

int main(void)
{
    static wchar_t tmp[MAX_PATH], dir[MAX_PATH], file[MAX_PATH];
    LONG v;

    GetTempPathW(MAX_PATH, tmp);
    wsprintfW(tmp + lstrlenW(tmp), L"gtv-exclude-test-%u", GetCurrentProcessId());
    CreateDirectoryW(tmp, 0);
    wsprintfW(dir, L"%s\\gotiengviet", tmp);
    wsprintfW(file, L"%s\\exclude.txt", dir);
    CreateDirectoryW(dir, 0);
    SetEnvironmentVariableW(L"APPDATA", tmp);

    /* BOM UTF-8, chú thích, dòng trống, khoảng trắng, CRLF và LF */
    write_file(file, "\xEF\xBB\xBF# chu thich\r\n  Code.exe  \r\n\r\ngame.exe\n#notepad.exe\n");
    exclude_init();
    expect("đọc code.exe (không phân biệt hoa/thường, bỏ khoảng trắng)", exclude_has(L"code.exe"));
    expect("đọc game.exe (dòng LF)", exclude_has(L"GAME.EXE"));
    expect("bỏ qua dòng chú thích", !exclude_has(L"notepad.exe"));
    expect("app không có trong danh sách", !exclude_has(L"chrome.exe"));

    v = exclude_version();
    /* Người dùng sửa file bằng Notepad: nạp lại khi thời gian sửa file đổi */
    Sleep(20);
    write_file(file, "zalo.exe\r\n");
    {
        FILETIME later;
        HANDLE f = CreateFileW(file, FILE_WRITE_ATTRIBUTES, 0, 0, OPEN_EXISTING, 0, 0);
        GetSystemTimeAsFileTime(&later);
        later.dwHighDateTime += 1; /* chắc chắn khác lần lưu trước */
        SetFileTime(f, 0, 0, &later);
        CloseHandle(f);
    }
    exclude_reload_if_changed();
    expect("nạp lại: có zalo.exe", exclude_has(L"zalo.exe"));
    expect("nạp lại: không còn game.exe", !exclude_has(L"game.exe"));
    expect("nạp lại thì đổi phiên bản", exclude_version() != v);

    DeleteFileW(file);
    RemoveDirectoryW(dir);
    RemoveDirectoryW(tmp);
    printf("%d/%d test loại trừ đạt\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
