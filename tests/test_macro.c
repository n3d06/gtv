/*
 * Test macro.c: đọc file gõ tắt, tra không phân biệt hoa/thường, viết hoa theo chữ tắt,
 * bỏ dòng sai. APPDATA trỏ sang thư mục tạm để không đụng file thật.
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

/* Tra `word`, so kết quả với `want` (0 là không có chữ tắt). */
static void lookup(const char *name, const wchar_t *word, const wchar_t *want)
{
    wchar_t out[MACRO_VALUE_MAX + 1];
    int len = macro_lookup(word, lstrlenW(word), out);
    out[len] = 0;
    expect(name, want ? lstrcmpW(out, want) == 0 : len == 0);
}

int main(void)
{
    static wchar_t tmp[MAX_PATH], dir[MAX_PATH], file[MAX_PATH];

    SetConsoleOutputCP(CP_UTF8);
    GetTempPathW(MAX_PATH, tmp);
    wsprintfW(tmp + lstrlenW(tmp), L"gtv-macro-test-%u", GetCurrentProcessId());
    CreateDirectoryW(tmp, 0);
    wsprintfW(dir, L"%s\\gotiengviet", tmp);
    wsprintfW(file, L"%s\\macro.txt", dir);
    CreateDirectoryW(dir, 0);
    SetEnvironmentVariableW(L"APPDATA", tmp);

    /* BOM, chú thích, Tab, khoảng trắng thừa, dòng thiếu nội dung */
    write_file(file, "\xEF\xBB\xBF# chu thich\r\n"
                     "ab  m\xE1\xBB\x99t hai  \r\n"
                     "cd\tba b\xE1\xBB\x91n\n"
                     "ef\n"
                     "\xC4\x91g \xC4\x91\xC6\xB0\xE1\xBB\xA3\x63\n"
                     "ko - kh\xC3\xB4ng \r\n"
                     "gh: gi hai\n"
                     "ij=i j\n"
                     "mn -5 do\n"
                     "op -\n");
    macro_init();
    expect("đếm được 7 chữ tắt", macro_count() == 7);
    lookup("ngăn cách bằng -", L"ko", L"không");
    lookup("ngăn cách bằng :", L"gh", L"gi hai");
    lookup("ngăn cách bằng = liền chữ", L"ij", L"i j");
    lookup("dấu - dính chữ là nội dung", L"mn", L"-5 do");
    lookup("bỏ dòng chỉ có dấu ngăn cách", L"op", 0);
    lookup("chữ thường", L"ab", L"một hai");
    lookup("cách bằng Tab", L"cd", L"ba bốn");
    lookup("viết hoa chữ đầu", L"Ab", L"Một hai");
    lookup("viết hoa cả chữ tắt", L"AB", L"MỘT HAI");
    lookup("chữ tắt có dấu", L"đg", L"được");
    lookup("chữ tắt có dấu viết hoa", L"Đg", L"Được");
    lookup("bỏ dòng thiếu nội dung", L"ef", 0);
    lookup("không có chữ tắt", L"xy", 0);
    lookup("không khớp một phần", L"a", 0);

    /* Ô nhập trong cửa sổ gõ tắt đọc theo cùng quy tắc */
    {
        wchar_t key[MACRO_KEY_LEN], value[MACRO_VALUE_MAX + 1];
        expect("ô nhập: chữ tắt - nội dung",
               macro_parse(L"  ab - cd ef  ", key, value) && !lstrcmpW(key, L"ab") && !lstrcmpW(value, L"cd ef"));
        expect("ô nhập: thiếu nội dung", !macro_parse(L"ab -", key, value));
        expect("ô nhập: trống", !macro_parse(L"   ", key, value));
    }

    DeleteFileW(file);
    RemoveDirectoryW(dir);
    RemoveDirectoryW(tmp);
    printf("%d/%d test gõ tắt đạt\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
