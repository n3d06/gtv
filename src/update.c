/*
 * Kiểm tra cập nhật: hỏi GitHub bản phát hành mới nhất (tag_name, tính cả bản beta) trên
 * luồng riêng, so với phiên bản đang chạy rồi báo về cửa sổ bằng WM_GTV_UPDATE. Không tự
 * tải bản cài: có bản mới thì mở trang phát hành để người dùng tải.
 */
#include "gtv.h"
#include <winhttp.h>
#include <shellapi.h>

#define API_HOST L"api.github.com"
#define API_PATH L"/repos/n3d06/gtv/releases?per_page=1" /* /latest bỏ qua bản beta */
#define RELEASE_PAGE L"https://github.com/n3d06/gtv/releases"
#define BODY_MAX 65536

static volatile LONG g_state = UPD_IDLE;
static wchar_t g_latest[32];
static HWND g_notify;

/* Đọc "1.2.3" (bỏ chữ v đầu), trả con trỏ tới hậu tố sau dấu - (chuỗi rỗng nếu không có). */
static const wchar_t *parse_version(const wchar_t *s, int v[3])
{
    int i;
    if (*s == 'v' || *s == 'V') s++;
    for (i = 0; i < 3; i++) {
        v[i] = 0;
        while (*s >= '0' && *s <= '9') v[i] = v[i] * 10 + (*s++ - '0');
        if (*s == '.') s++;
    }
    return *s == '-' ? s + 1 : s + lstrlenW(s);
}

/* So hậu tố như semver: không hậu tố là bản chính thức, mới hơn mọi bản beta; còn lại so
 * từng ký tự, đoạn chữ số so theo giá trị (beta.10 mới hơn beta.9). */
static int compare_suffix(const wchar_t *a, const wchar_t *b)
{
    if (!*a || !*b) return !*a - !*b;
    while (*a && *b) {
        if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
            int x = 0, y = 0;
            while (*a >= '0' && *a <= '9') x = x * 10 + (*a++ - '0');
            while (*b >= '0' && *b <= '9') y = y * 10 + (*b++ - '0');
            if (x != y) return x > y ? 1 : -1;
        } else if (*a != *b) {
            return *a > *b ? 1 : -1;
        } else {
            a++;
            b++;
        }
    }
    return !!*a - !!*b;
}

int version_newer(const wchar_t *a, const wchar_t *b)
{
    int x[3], y[3], i;
    const wchar_t *sa = parse_version(a, x), *sb = parse_version(b, y);
    for (i = 0; i < 3; i++)
        if (x[i] != y[i]) return x[i] > y[i];
    return compare_suffix(sa, sb) > 0;
}

int update_is_dev(void)
{
    int v[3];
    parse_version(APP_VERSION, v);
    return !v[0] && !v[1] && !v[2];
}

int update_state(void) { return g_state; }
const wchar_t *update_latest(void) { return g_latest; }

void update_open_page(void)
{
    ShellExecuteW(0, L"open", RELEASE_PAGE, 0, 0, SW_SHOWNORMAL);
}

/* Lấy giá trị chuỗi của "tag_name" trong JSON trả về. */
static int find_tag(const char *body, wchar_t *out, int cap)
{
    static const char KEY[] = "\"tag_name\"";
    const char *p = body;
    int n = 0;
    for (; *p; p++) {
        const char *k = KEY, *q = p;
        while (*k && *q == *k) { k++; q++; }
        if (*k) continue;
        while (*q == ' ' || *q == ':') q++;
        if (*q++ != '"') return 0;
        if (*q == 'v' || *q == 'V') q++;
        while (*q && *q != '"' && n < cap - 1) out[n++] = (wchar_t)(unsigned char)*q++;
        out[n] = 0;
        return n > 0;
    }
    return 0;
}

static int fetch(char *body, int cap)
{
    HINTERNET s = 0, c = 0, r = 0;
    DWORD status = 0, size = sizeof status, got;
    int n = 0, ok = 0;

    s = WinHttpOpen(L"gotiengviet/" APP_VERSION, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, 0, 0, 0);
    if (s) c = WinHttpConnect(s, API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (c) r = WinHttpOpenRequest(c, L"GET", API_PATH, 0, 0, 0, WINHTTP_FLAG_SECURE);
    if (r) {
        WinHttpSetTimeouts(r, 5000, 5000, 10000, 10000);
        ok = WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, 0, 0, 0, 0) &&
             WinHttpReceiveResponse(r, 0) &&
             WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, 0, &status, &size, 0) &&
             status == 200;
        while (ok && n < cap - 1 && WinHttpReadData(r, body + n, (DWORD)(cap - 1 - n), &got) && got) n += (int)got;
    }
    body[n] = 0;
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return ok && n > 0;
}

static DWORD WINAPI check_thread(void *arg)
{
    static char body[BODY_MAX];
    LONG st = UPD_ERROR;
    (void)arg;
    if (fetch(body, BODY_MAX) && find_tag(body, g_latest, 32))
        st =version_newer(g_latest, APP_VERSION) ? UPD_NEWER : UPD_LATEST;
    InterlockedExchange(&g_state, st);
    PostMessageW(g_notify, WM_GTV_UPDATE, 0, 0);
    return 0;
}

void update_check(HWND notify)
{
    HANDLE t;
    if (InterlockedExchange(&g_state, UPD_CHECKING) == UPD_CHECKING) return;
    g_notify = notify;
    t = CreateThread(0, 64 * 1024, check_thread, 0, STACK_SIZE_PARAM_IS_A_RESERVATION, 0);
    if (t) CloseHandle(t);
    else g_state = UPD_ERROR;
}
