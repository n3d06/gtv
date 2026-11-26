/*
 * Test lõi Telex: mô phỏng một ô văn bản, áp lệnh sửa của lõi lên ô đó
 * rồi so với kết quả mong đợi. Ký hiệu đầu vào: '<' là Backspace.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../src/telex.h"

#define FIELD_MAX 4096

typedef struct {
    wchar_t s[FIELD_MAX];
    int n;
} Field;

static int field_apply(Field *f, const TelexEdit *e)
{
    int i;
    if (e->backspaces > f->n) return 0;
    f->n -= e->backspaces;
    if (f->n + e->len >= FIELD_MAX) return 0;
    for (i = 0; i < e->len; i++) f->s[f->n++] = e->text[i];
    return 1;
}

/* Gõ một phím vào ô; trả 0 nếu lõi yêu cầu xóa nhiều hơn ô đang có. */
static int type_key(Telex *t, Field *f, wchar_t k)
{
    TelexEdit e;
    if (k == '<') {
        if (telex_backspace(t, &e)) return field_apply(f, &e);
        if (f->n > 0) f->n--;
        return 1;
    }
    if (telex_key(t, k, &e)) return field_apply(f, &e);
    if (f->n + 1 >= FIELD_MAX) return 0;
    f->s[f->n++] = k;
    return 1;
}

static void print_w(const wchar_t *s, int n)
{
    char buf[FIELD_MAX * 4];
    int len = WideCharToMultiByte(CP_UTF8, 0, s, n, buf, sizeof buf - 1, 0, 0);
    buf[len] = 0;
    printf("%s", buf);
}

static int failures, total;

static void check(const char *input, const wchar_t *expect, int old_style, int w_alone)
{
    Telex t;
    Field f;
    const char *p;
    int ok = 1;
    telex_init(&t);
    t.old_style = old_style;
    t.w_alone = w_alone;
    f.n = 0;
    for (p = input; *p && ok; p++) ok = type_key(&t, &f, (wchar_t)*p);
    total++;
    if (ok && f.n == (int)wcslen(expect) && wmemcmp(f.s, expect, f.n) == 0) return;
    failures++;
    printf("SAI  %-20s -> \"", input);
    print_w(f.s, f.n);
    printf("\", mong đợi \"");
    print_w(expect, (int)wcslen(expect));
    printf("\"%s\n", ok ? "" : " (lệnh xóa vượt quá ô)");
}

#define T(in, out) check(in, out, 0, 0)
#define T_OLD(in, out) check(in, out, 1, 0)
#define T_W(in, out) check(in, out, 0, 1)

static void test_cases(void)
{
    /* Biến đổi cơ bản */
    T("aa", L"â"); T("aw", L"ă"); T("ow", L"ơ"); T("uw", L"ư");
    T("ee", L"ê"); T("oo", L"ô"); T("dd", L"đ"); T("w", L"w");
    T("as", L"á"); T("af", L"à"); T("ar", L"ả"); T("ax", L"ã"); T("aj", L"ạ");
    T("asz", L"a"); T("asf", L"à"); T("z", L"z"); T("az", L"az");

    /* Gõ lặp để hủy */
    T("ass", L"as"); T("aff", L"af"); T("aaa", L"aa"); T("aww", L"aw");
    T("ooo", L"oo"); T("ddd", L"dd"); T("ww", L"ww"); T("uww", L"uw");
    T("asss", L"ass"); T("awa", L"â");

    /* Đặt dấu */
    T("quoocs", L"quốc"); T("tieengs", L"tiếng"); T("tuooir", L"tuổi");
    T("hoaf", L"hoà"); T("khoer", L"khoẻ"); T("thuys", L"thuý");
    T("hoanf", L"hoàn"); T("toans", L"toán"); T("huynhf", L"huỳnh");
    T("nguowif", L"người"); T("dduowngf", L"đường"); T("dduwowcj", L"được");
    T("vieetj", L"việt"); T("gias", L"giá"); T("gif", L"gì"); T("ginf", L"gìn");
    T("quas", L"quá"); T("quys", L"quý"); T("muaw", L"mưa"); T("muas", L"múa");
    T("mias", L"mía"); T("cuar", L"của"); T("quar", L"quả"); T("giuwax", L"giữa");
    T("hoawcj", L"hoặc"); T("khuyeen", L"khuyên"); T("chuyeenj", L"chuyện");
    T("khuyur", L"khuỷu"); T("ngoaif", L"ngoài"); T("xoays", L"xoáy");
    T("tw", L"tw"); T("tuw", L"tư"); T("www", L"www"); T("windows", L"windows"); T("tooi", L"tôi"); T("toois", L"tối");
    T("muoons", L"muốn"); T("nghieeng", L"nghiêng"); T("ddi", L"đi");
    T("yeeu", L"yêu"); T("yeeus", L"yếu"); T("huwu", L"hưu"); T("huwuu", L"hưuu");
    T("thuee", L"thuê"); T("hueej", L"huệ"); T("dduwowng", L"đương");
    T("duowng", L"dương"); T("buwowms", L"bướm"); T("ruwowuj", L"rượu");
    T("khuyas", L"khuýa"); T("ngoeof", L"ngoèo"); T("xooong", L"xoong"); T("xoong", L"xông");
    T("tooi", L"tôi"); T("tuoon", L"tuôn");
    T("khuaay", L"khuây"); T("quaay", L"quây"); T("tuaan", L"tuân");

    /* Gõ dấu ở cuối hoặc giữa từ */
    T("tiengse", L"tiếng"); T("tieesng", L"tiếng"); T("tana", L"tân");
    T("nguoiwf", L"người"); T("dduongwf", L"đường");

    /* Kiểu cũ */
    T_OLD("hoaf", L"hòa"); T_OLD("khoer", L"khỏe"); T_OLD("thuys", L"thúy");
    T_OLD("hoafn", L"hoàn"); T_OLD("tuys", L"túy");

    /* Chữ hoa */
    T("VIEETJ", L"VIỆT"); T("Vieetj", L"Việt"); T("DDaay", L"Đây");
    T("Ddi", L"Đi"); T("AA", L"Â"); T("Aa", L"Â"); T("NGUOWIF", L"NGƯỜI");
    T("W", L"W");

    /* w đứng riêng thành ư (tùy chọn) */
    T_W("w", L"ư"); T_W("ww", L"w"); T_W("tw", L"tư"); T_W("W", L"Ư"); T_W("tws", L"tứ");

    /* Kết thúc từ */
    T("as as", L"á á"); T("Vieetj Nam", L"Việt Nam"); T("tieengs Vieetj.", L"tiếng Việt.");
    T("a1s", L"a1s");

    /* Tiếng Anh: không biến đổi khi từ không còn hợp lệ */
    T("fix", L"fix"); T("jazz", L"jazz"); T("class", L"class"); T("print", L"print");

    /* Backspace */
    T("tieengs<", L"tiến"); T("as<", L""); T("tas<a", L"ta"); T("tas<", L"t");
    T("vieetj<<t", L"vit"); T("vieetj<t", L"việt"); T("dd<dd", L"đ");
    T("quoocs<", L"quố"); T("quoocs<<", L"qu"); T("quoocs<<oocs", L"quốc");
    T("aa<", L""); T("ass<", L"a"); T("hoaf<", L"ho"); T("tieengs<<<<", L"t");
    T("ab<<<<", L"");

    /* Xóa dấu cách để quay lại sửa từ trước */
    T("bawts <f", L"bằt"); T("vieet <j", L"việt"); T("as  <<s", L"as");
    T("as b<<s", L"as"); T("tieengs Vieetj<<<<<< <j", L"tiện");
    T("hoa. <<f", L"hoà"); T("as <a", L"ấ"); T("as < f", L"á f");
    T("ddi <<<d", L"d"); T("bawts <<", L"bắ");
}

/* Fuzz: chuỗi phím ngẫu nhiên, kiểm tra không tràn và ô luôn khớp chuỗi lõi đang giữ. */
static unsigned int rng = 12345;
static unsigned int rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void fuzz(int iterations)
{
    static const char keys[] = "aeiouydwsfrxjzqgnhtcmpAEOUWDS<<< .1";
    Telex t;
    Field f;
    wchar_t r[TELEX_MAX_CHARS];
    int i, rl, bad = 0;
    telex_init(&t);
    f.n = 0;
    for (i = 0; i < iterations && !bad; i++) {
        wchar_t k = (wchar_t)keys[rnd() % (sizeof keys - 1)];
        if (f.n > FIELD_MAX - 64) { f.n = 0; telex_reset(&t); }
        if (!type_key(&t, &f, k)) { bad = 1; break; }
        if (t.n < 0 || t.n > TELEX_MAX_CHARS) { bad = 2; break; }
        if (!t.dead) {
            rl = telex_render(&t, r);
            if (rl > f.n || wmemcmp(f.s + f.n - rl, r, rl) != 0) { bad = 3; break; }
        }
    }
    total++;
    if (bad) {
        failures++;
        printf("SAI  fuzz lỗi loại %d ở bước %d\n", bad, i);
    } else {
        printf("fuzz: %d phím ngẫu nhiên, không lỗi\n", iterations);
    }
}

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);
    test_cases();
    fuzz(2000000);
    printf("%d/%d test đạt\n", total - failures, total);
    return failures ? 1 : 0;
}
