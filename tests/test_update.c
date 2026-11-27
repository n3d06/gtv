/* Test update.c: so phiên bản (không gọi mạng). */
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

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);
    expect("0.2.0 mới hơn 0.1.0", version_newer(L"0.2.0", L"0.1.0"));
    expect("0.10.0 mới hơn 0.9.9", version_newer(L"0.10.0", L"0.9.9"));
    expect("1.0 mới hơn 0.9.9", version_newer(L"1.0", L"0.9.9"));
    expect("chữ v đầu được bỏ qua", version_newer(L"v0.2.0", L"0.1.0"));
    expect("bằng nhau thì không mới hơn", !version_newer(L"0.2.0", L"v0.2.0"));
    expect("cũ hơn thì không mới hơn", !version_newer(L"0.1.0", L"0.1.1"));
    expect("bản dev cũ hơn mọi bản phát hành", version_newer(L"0.0.1", L"0.0.0-dev"));
    expect("beta.2 mới hơn beta.1", version_newer(L"v0.1.0-beta.2", L"0.1.0-beta.1"));
    expect("beta.10 mới hơn beta.9", version_newer(L"0.1.0-beta.10", L"0.1.0-beta.9"));
    expect("bản chính thức mới hơn bản beta", version_newer(L"0.1.0", L"0.1.0-beta.2"));
    expect("bản beta không mới hơn bản chính thức", !version_newer(L"0.1.0-beta.2", L"0.1.0"));
    expect("cùng bản beta thì không mới hơn", !version_newer(L"0.1.0-beta.2", L"0.1.0-beta.2"));
    expect("beta của bản sau mới hơn bản trước", version_newer(L"0.2.0-beta.1", L"0.1.0"));
    printf("%d/%d test cập nhật đạt\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
