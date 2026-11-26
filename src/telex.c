#include "telex.h"

enum { M_NONE = 0, M_CIRC, M_BREVE, M_HORN, M_STROKE };

/* Thứ tự dấu: không, sắc, huyền, hỏi, ngã, nặng. */
static const wchar_t *const VOW_LO[12] = {
    L"aáàảãạ", L"ăắằẳẵặ", L"âấầẩẫậ", L"eéèẻẽẹ", L"êếềểễệ", L"iíìỉĩị",
    L"oóòỏõọ", L"ôốồổỗộ", L"ơớờởỡợ", L"uúùủũụ", L"ưứừửữự", L"yýỳỷỹỵ",
};
static const wchar_t *const VOW_UP[12] = {
    L"AÁÀẢÃẠ", L"ĂẮẰẲẴẶ", L"ÂẤẦẨẪẬ", L"EÉÈẺẼẸ", L"ÊẾỀỂỄỆ", L"IÍÌỈĨỊ",
    L"OÓÒỎÕỌ", L"ÔỐỒỔỖỘ", L"ƠỚỜỞỠỢ", L"UÚÙỦŨỤ", L"ƯỨỪỬỮỰ", L"YÝỲỶỸỴ",
};

static const char *const INITIALS[] = {
    "", "b", "c", "ch", "d", "g", "gh", "gi", "h", "k", "kh", "l", "m", "n",
    "ng", "ngh", "nh", "p", "ph", "q", "qu", "r", "s", "t", "th", "tr", "v", "x", 0,
};
static const char *const FINALS[] = {
    "", "c", "ch", "m", "n", "ng", "nh", "p", "t", 0,
};
/* Nguyên âm theo chữ gốc, bỏ qua dấu mũ/móc (ie gồm cả iê, ua gồm cả ưa...). */
static const char *const NUCLEI[] = {
    "a", "e", "i", "o", "u", "y",
    "ai", "ao", "au", "ay", "eo", "eu", "ia", "ie", "iu", "oa", "oe", "oi",
    "ua", "ue", "ui", "uo", "uu", "uy", "ye",
    "ieu", "oai", "oay", "oeo", "uay", "uoi", "uou", "uya", "uye", "uyu", "yeu", 0,
};

static int is_vowel(const TelexChar *x)
{
    switch (x->c) {
    case 'a': case 'e': case 'i': case 'o': case 'u': case 'y': return 1;
    }
    return 0;
}

static int vowel_index(const TelexChar *x)
{
    switch (x->c) {
    case 'a': return x->mod == M_BREVE ? 1 : x->mod == M_CIRC ? 2 : 0;
    case 'e': return x->mod == M_CIRC ? 4 : 3;
    case 'i': return 5;
    case 'o': return x->mod == M_CIRC ? 7 : x->mod == M_HORN ? 8 : 6;
    case 'u': return x->mod == M_HORN ? 10 : 9;
    case 'y': return 11;
    }
    return -1;
}

static int in_set(const Telex *t, int a, int b, const char *const *set)
{
    int k;
    for (k = 0; set[k]; k++) {
        const char *s = set[k];
        int i = a;
        while (i < b && *s && (unsigned char)*s == t->ch[i].c) { i++; s++; }
        if (i == b && !*s) return 1;
    }
    return 0;
}

/* Tách âm đầu, nguyên âm [ns, ne), âm cuối. Trả 1 nếu là âm tiết (đang gõ) hợp lệ. */
static int parse(const Telex *t, int *pns, int *pne)
{
    int n = t->n, i = 0, ns, ne;
    while (i < n && !is_vowel(&t->ch[i])) i++;
    if (i == 1 && i + 1 < n && is_vowel(&t->ch[i + 1])) {
        if ((t->ch[0].c == 'q' && t->ch[1].c == 'u') ||
            (t->ch[0].c == 'g' && t->ch[1].c == 'i'))
            i = 2;
    }
    ns = i;
    while (i < n && is_vowel(&t->ch[i])) i++;
    ne = i;
    *pns = ns;
    *pne = ne;
    if (!in_set(t, 0, ns, INITIALS)) return 0;
    if (ne == ns) return ne == n;
    if (ne - ns > 3 || !in_set(t, ns, ne, NUCLEI)) return 0;
    return in_set(t, ne, n, FINALS);
}

static int tone_pos(const Telex *t, int ns, int ne)
{
    int i, len = ne - ns;
    unsigned char a, b;
    if (len <= 0) return -1;
    for (i = ne - 1; i >= ns; i--)
        if (t->ch[i].mod != M_NONE) return i;
    if (len == 1) return ns;
    if (len >= 3) return ns + 1;
    if (ne < t->n) return ns + 1; /* có âm cuối: hoàn, toán, huỳnh */
    a = t->ch[ns].c;
    b = t->ch[ns + 1].c;
    if (!t->old_style && ((a == 'o' && (b == 'a' || b == 'e')) || (a == 'u' && b == 'y')))
        return ns + 1;
    return ns;
}

int telex_render(const Telex *t, wchar_t *out)
{
    int ns, ne, i, tp;
    parse(t, &ns, &ne);
    tp = t->tone ? tone_pos(t, ns, ne) : -1;
    for (i = 0; i < t->n; i++) {
        const TelexChar *x = &t->ch[i];
        int v = vowel_index(x);
        if (v >= 0)
            out[i] = (x->up ? VOW_UP : VOW_LO)[v][i == tp ? t->tone : 0];
        else if (x->c == 'd' && x->mod == M_STROKE)
            out[i] = x->up ? 0x0110 : 0x0111;
        else
            out[i] = (wchar_t)(x->up ? x->c - 'a' + 'A' : x->c);
    }
    return t->n;
}

void telex_reset(Telex *t)
{
    t->n = 0;
    t->tone = 0;
    t->dead = 0;
    t->gap = 0;
}

void telex_init(Telex *t)
{
    telex_reset(t);
    t->old_style = 0;
    t->w_alone = 0;
}

#define MAX_GAP 8

void telex_break(Telex *t)
{
    int i;
    if (t->dead) {
        telex_reset(t);
        return;
    }
    if (t->n > 0) {
        for (i = 0; i < t->n; i++) t->prev[i] = t->ch[i];
        t->prev_n = t->n;
        t->prev_tone = t->tone;
        t->gap = 1;
    } else if (t->gap > 0) {
        t->gap = t->gap < MAX_GAP ? t->gap + 1 : 0;
    }
    t->n = 0;
    t->tone = 0;
}

static void append(Telex *t, int c, int up)
{
    TelexChar *x = &t->ch[t->n++];
    x->c = (unsigned char)c;
    x->mod = M_NONE;
    x->up = (unsigned char)up;
    x->from_w = 0;
}

static int tone_of(int c)
{
    switch (c) {
    case 's': return 1;
    case 'f': return 2;
    case 'r': return 3;
    case 'x': return 4;
    case 'j': return 5;
    }
    return 0;
}

/* aa → â, ee → ê, oo → ô; gõ lần nữa thì hủy. */
static int do_circ(Telex *t, int ns, int ne, int c, int up)
{
    int i, ok;
    /* Nếu thêm chữ này vào cuối vẫn là vần hợp lệ (oe + o → oeo) thì để là chữ thường. */
    if (ne == t->n && ne - ns < 3) {
        t->n++;
        t->ch[ne].c = (unsigned char)c;
        ok = in_set(t, ns, ne + 1, NUCLEI);
        t->n--;
        if (ok) return 0;
    }
    for (i = ne - 1; i >= ns; i--) {
        TelexChar *x = &t->ch[i];
        if (x->c != c) continue;
        if (x->mod == M_CIRC) {
            x->mod = M_NONE;
            append(t, c, up);
        } else {
            x->mod = M_CIRC;
        }
        return 1;
    }
    return 0;
}

static int w_target(const Telex *t, int ns, int ne)
{
    int i;
    if (ne - ns >= 2 && t->ch[ns].c == 'o' && t->ch[ns + 1].c == 'a') return ns + 1;
    for (i = ns; i < ne; i++) if (t->ch[i].c == 'u') return i;
    for (i = ns; i < ne; i++) if (t->ch[i].c == 'o') return i;
    for (i = ns; i < ne; i++) if (t->ch[i].c == 'a') return i;
    return -1;
}

/* aw → ă, ow → ơ, uw → ư, uow → ươ, w đứng riêng → ư; gõ lần nữa thì hủy. */
static int do_w(Telex *t, int ns, int ne, int up)
{
    int i, want;
    TelexChar *x;
    if (ne == ns) {
        if (!t->w_alone) return 0;
        append(t, 'u', up);
        t->ch[t->n - 1].mod = M_HORN;
        t->ch[t->n - 1].from_w = 1;
        return 1;
    }
    for (i = ns; i + 1 < ne; i++) {
        TelexChar *u = &t->ch[i], *o = &t->ch[i + 1];
        if (u->c != 'u' || o->c != 'o') continue;
        if (u->mod == M_HORN && o->mod == M_HORN) {
            u->mod = o->mod = M_NONE;
            append(t, 'w', up);
        } else {
            u->mod = o->mod = M_HORN;
        }
        return 1;
    }
    i = w_target(t, ns, ne);
    if (i < 0) return 0;
    x = &t->ch[i];
    want = x->c == 'a' ? M_BREVE : M_HORN;
    if (x->mod != want) {
        x->mod = (unsigned char)want;
    } else if (x->from_w) {
        x->c = 'w';
        x->mod = M_NONE;
        x->from_w = 0;
    } else {
        x->mod = M_NONE;
        append(t, 'w', up);
    }
    return 1;
}

static int apply_key(Telex *t, int c, int up)
{
    int ns, ne, tone;
    if (!parse(t, &ns, &ne)) return 0;
    tone = tone_of(c);
    if (tone) {
        if (ne == ns) return 0;
        if (t->tone == tone) {
            t->tone = 0;
            append(t, c, up);
        } else {
            t->tone = tone;
        }
        return 1;
    }
    switch (c) {
    case 'z':
        if (!t->tone) return 0;
        t->tone = 0;
        return 1;
    case 'a': case 'e': case 'o':
        return do_circ(t, ns, ne, c, up);
    case 'w':
        return do_w(t, ns, ne, up);
    case 'd':
        if (t->n != 1 || t->ch[0].c != 'd') return 0;
        if (t->ch[0].mod == M_STROKE) {
            t->ch[0].mod = M_NONE;
            append(t, 'd', up);
        } else {
            t->ch[0].mod = M_STROKE;
        }
        return 1;
    }
    return 0;
}

static int make_edit(const wchar_t *old, int olen, const wchar_t *nw, int nlen, TelexEdit *e)
{
    int p = 0, i;
    while (p < olen && p < nlen && old[p] == nw[p]) p++;
    e->backspaces = olen - p;
    e->len = nlen - p;
    for (i = 0; i < e->len; i++) e->text[i] = nw[p + i];
    return 1;
}

int telex_key(Telex *t, wchar_t wc, TelexEdit *e)
{
    wchar_t old[TELEX_MAX_CHARS], nw[TELEX_MAX_CHARS];
    int olen, nlen, i, up, c;

    if (wc >= 'A' && wc <= 'Z') { up = 1; c = wc - 'A' + 'a'; }
    else if (wc >= 'a' && wc <= 'z') { up = 0; c = wc; }
    else { telex_break(t); return 0; }

    if (t->dead) return 0;
    if (t->n >= TELEX_MAX_CHARS - 1) { t->dead = 1; t->gap = 0; return 0; }

    olen = telex_render(t, old);
    if (!apply_key(t, c, up)) append(t, c, up);
    nlen = telex_render(t, nw);

    if (nlen == olen + 1 && nw[olen] == wc) {
        for (i = 0; i < olen && old[i] == nw[i]; i++) {}
        if (i == olen) return 0;
    }
    return make_edit(old, olen, nw, nlen, e);
}

int telex_backspace(Telex *t, TelexEdit *e)
{
    wchar_t old[TELEX_MAX_CHARS], nw[TELEX_MAX_CHARS];
    int olen, nlen, i, ns, ne;

    if (t->dead) { telex_reset(t); return 0; }
    if (t->n == 0) {
        /* Đang xóa dấu cách/dấu câu sau từ trước: xóa hết thì quay lại từ đó. */
        if (t->gap > 0 && --t->gap == 0) {
            for (i = 0; i < t->prev_n; i++) t->ch[i] = t->prev[i];
            t->n = t->prev_n;
            t->tone = t->prev_tone;
        }
        return 0;
    }

    olen = telex_render(t, old);
    /* Xóa chữ đang mang dấu thanh thì xóa luôn dấu. */
    parse(t, &ns, &ne);
    if (t->tone && tone_pos(t, ns, ne) == t->n - 1) t->tone = 0;
    t->n--;
    parse(t, &ns, &ne);
    if (ne == ns) t->tone = 0;
    nlen = telex_render(t, nw);

    if (nlen == olen - 1) {
        for (i = 0; i < nlen && old[i] == nw[i]; i++) {}
        if (i == nlen) return 0;
    }
    return make_edit(old, olen, nw, nlen, e);
}
