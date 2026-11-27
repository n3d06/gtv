/*
 * Gõ tắt: %APPDATA%\gotiengviet\macro.txt, mỗi dòng "chữ tắt nội dung" (cách nhau bằng
 * dấu cách, Tab, hoặc dấu - : =). Luồng chính nạp vào bảng phụ rồi đổi con trỏ; luồng hook
 * tra bảng đang dùng dưới khóa đọc.
 */
#include "gtv.h"

#define FILE_MAX (MACRO_MAX * (MACRO_KEY_LEN + MACRO_VALUE_MAX + 8))

typedef struct {
    wchar_t key[MACRO_KEY_LEN], value[MACRO_VALUE_MAX + 1];
    int klen, vlen;
} Macro;

typedef struct {
    Macro items[MACRO_MAX];
    int n;
} MacroTable;

static MacroTable g_tables[2];
static MacroTable *g_active = &g_tables[0];
static SRWLOCK g_lock = SRWLOCK_INIT;
static DataFile g_file;

static const char HEADER[] =
    "# Gõ tắt: mỗi dòng là chữ tắt rồi nội dung, ví dụ: vn Việt Nam hoặc vn - Việt Nam\r\n"
    "# Gõ chữ tắt rồi dấu cách, dấu câu hoặc Enter là bộ gõ thay bằng nội dung.\r\n"
    "# Gõ chữ tắt viết hoa thì nội dung viết hoa theo. Dòng bắt đầu bằng # là chú thích.\r\n";

static int is_blank(wchar_t c) { return c == ' ' || c == '\t'; }

/* Tách [a, b) đã bỏ khoảng trắng hai đầu. "ko không", "ko - không", "ko: không",
 * "ko = không" đều được. Dấu - chỉ là dấu ngăn cách khi đứng riêng, để nội dung như
 * "-5 độ" vẫn giữ nguyên. */
static int parse_range(const wchar_t *a, const wchar_t *b, Macro *m)
{
    const wchar_t *k = a, *v;
    while (a < b && !is_blank(*a) && *a != ':' && *a != '=') a++;
    v = a;
    while (v < b && is_blank(*v)) v++;
    if (v < b && (*v == ':' || *v == '=' || (*v == '-' && (v + 1 == b || is_blank(v[1]))))) v++;
    while (v < b && is_blank(*v)) v++;
    if (a == k || a - k >= MACRO_KEY_LEN || v == b) return 0;
    m->klen = (int)(a - k);
    m->vlen = (int)(b - v) < MACRO_VALUE_MAX ? (int)(b - v) : MACRO_VALUE_MAX;
    lstrcpynW(m->key, k, m->klen + 1);
    lstrcpynW(m->value, v, m->vlen + 1);
    return 1;
}

static void parse_line(MacroTable *t, const wchar_t *a, const wchar_t *b)
{
    t->n += parse_range(a, b, &t->items[t->n]);
}

int macro_parse(const wchar_t *s, wchar_t *key, wchar_t *value)
{
    static Macro m;
    const wchar_t *b = s + lstrlenW(s);
    while (is_blank(*s)) s++;
    while (b > s && is_blank(b[-1])) b--;
    if (!parse_range(s, b, &m)) return 0;
    lstrcpyW(key, m.key);
    lstrcpyW(value, m.value);
    return 1;
}

static void load(void)
{
    static wchar_t text[FILE_MAX];
    MacroTable *t = g_active == &g_tables[0] ? &g_tables[1] : &g_tables[0];
    const wchar_t *s = text, *a, *b;

    datafile_read(&g_file, text, FILE_MAX);
    t->n = 0;
    while (t->n < MACRO_MAX && datafile_next_line(&s, &a, &b)) parse_line(t, a, b);

    AcquireSRWLockExclusive(&g_lock);
    g_active = t;
    ReleaseSRWLockExclusive(&g_lock);
}

void macro_init(void)
{
    if (datafile_init(&g_file, L"macro.txt")) load();
}

int macro_reload_if_changed(void)
{
    if (!datafile_changed(&g_file)) return 0;
    load();
    return 1;
}

int macro_count(void) { return g_active->n; }

void macro_get(int i, const wchar_t **key, const wchar_t **value)
{
    *key = g_active->items[i].key;
    *value = g_active->items[i].value;
}

/* Ghi dạng "chữ tắt - nội dung": dấu - đứng riêng nên nội dung bắt đầu bằng - : = vẫn giữ nguyên. */
int macro_save(const wchar_t *const *keys, const wchar_t *const *values, int n)
{
    static wchar_t body[FILE_MAX];
    int i, len = 0, ok;
    for (i = 0; i < n && i < MACRO_MAX; i++) len += wsprintfW(body + len, L"%s - %s\r\n", keys[i], values[i]);
    ok = datafile_write(&g_file, HEADER, body, len);
    load();
    return ok;
}

/* Viết hoa cả chữ tắt (từ 2 chữ) thì viết hoa cả nội dung; viết hoa chữ đầu thì hoa chữ đầu. */
static void match_case(const wchar_t *word, int n, wchar_t *out, int len)
{
    int i, upper = n > 1;
    for (i = 0; i < n && upper; i++) upper = !IsCharLowerW(word[i]);
    if (upper) CharUpperBuffW(out, (DWORD)len);
    else if (IsCharUpperW(word[0])) CharUpperBuffW(out, 1);
}

int macro_lookup(const wchar_t *word, int n, wchar_t *out)
{
    int i, len = 0;
    if (n <= 0) return 0;
    AcquireSRWLockShared(&g_lock);
    for (i = 0; i < g_active->n; i++) {
        const Macro *m = &g_active->items[i];
        if (CompareStringOrdinal(word, n, m->key, m->klen, TRUE) != CSTR_EQUAL) continue;
        len = m->vlen;
        CopyMemory(out, m->value, (SIZE_T)len * sizeof(wchar_t));
        break;
    }
    ReleaseSRWLockShared(&g_lock);
    if (len) match_case(word, n, out, len);
    return len;
}
