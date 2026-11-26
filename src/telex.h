/* Lõi Telex: C thuần, không phụ thuộc Windows, không dùng CRT. */
#ifndef TELEX_H
#define TELEX_H

#include <wchar.h>

#define TELEX_MAX_CHARS 32

typedef struct {
    unsigned char c;      /* chữ gốc 'a'..'z' */
    unsigned char mod;    /* mũ, trăng, móc, gạch (đ) */
    unsigned char up;
    unsigned char from_w; /* ư sinh từ phím w đứng riêng */
} TelexChar;

typedef struct {
    TelexChar ch[TELEX_MAX_CHARS];
    int n;
    int tone;      /* 0 không dấu, 1 sắc, 2 huyền, 3 hỏi, 4 ngã, 5 nặng */
    int dead;      /* tràn bộ đệm: bỏ qua đến hết từ */
    int old_style; /* 1: hòa, khỏe, thúy; 0: hoà, khoẻ, thuý */
    int w_alone;   /* 1: w đứng riêng thành ư */

    /* Từ vừa kết thúc, để xóa hết dấu cách phía sau thì sửa tiếp được. */
    TelexChar prev[TELEX_MAX_CHARS];
    int prev_n, prev_tone;
    int gap; /* số ký tự đã gõ sau từ trước, 0 là không còn nhớ */
} Telex;

/* Lệnh sửa: xóa `backspaces` ký tự cuối rồi gửi text[0..len). */
typedef struct {
    int backspaces, len;
    wchar_t text[TELEX_MAX_CHARS];
} TelexEdit;

void telex_init(Telex *t);
void telex_reset(Telex *t); /* bỏ cả từ đang gõ và từ trước */
void telex_break(Telex *t); /* kết thúc từ bằng một ký tự vừa chèn (dấu cách, dấu câu) */

/* Trả 1: chặn phím gốc và thực hiện `e`. Trả 0: cho phím đi qua. */
int telex_key(Telex *t, wchar_t wc, TelexEdit *e);
int telex_backspace(Telex *t, TelexEdit *e);

int telex_render(const Telex *t, wchar_t *out);

#endif
