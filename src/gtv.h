#ifndef GTV_H
#define GTV_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "telex.h"

/* dwExtraInfo của phím do bộ gõ phát ra; _END đánh dấu sự kiện cuối của một lần phát. */
#define GTV_MAGIC ((ULONG_PTR)0x47545621)
#define GTV_MAGIC_END ((ULONG_PTR)0x47545622)

#define WM_GTV_STATE (WM_APP + 2)
#define WM_GTV_UPDATE (WM_APP + 3) /* kiểm tra cập nhật xong */

/* Phiên bản: build.bat truyền GTV_VERSION (CI lấy từ tag), build tay là 0.0.0-dev. */
#ifndef GTV_VERSION
#define GTV_VERSION "0.0.0-dev"
#endif
#define GTV_WIDEN2(s) L##s
#define GTV_WIDEN(s) GTV_WIDEN2(s)
#define APP_VERSION GTV_WIDEN(GTV_VERSION)

enum {
    MOD_LSHIFT = 1, MOD_RSHIFT = 2, MOD_LCTRL = 4, MOD_RCTRL = 8,
    MOD_LALT = 16, MOD_RALT = 32, MOD_LWIN = 64, MOD_RWIN = 128,
};

typedef struct {
    DWORD vk, scan, flags, mods, time, eaten;
} KeyEvent;

/* hook.c */
int hook_start(HANDLE log_evt);
void hook_stop(void);
int hook_pop(KeyEvent *e);
LONG hook_dropped(void);
LONG hook_skipped_self(void);

/* emit.c: xóa `bs` ký tự, gửi `text`, phát lại `vk` nếu khác 0, trong một lần SendInput.
 * `pad`: gõ ký tự đệm trước để thay phần gợi ý đang bôi đen. */
int emit_start(void);
void emit_stop(void);
int emit_push(int bs, const wchar_t *text, int len, WORD vk, WORD scan, DWORD kflags, int pad);
int emit_pending(void);
void emit_passed(void);
LONG emit_sent_events(void);

/* Phím tắt gói trong một DWORD: (phím bổ trợ << 16) | vk; vk = 0 là chỉ phím bổ trợ. */
#define HK_CTRL 1
#define HK_SHIFT 2
#define HK_ALT 4
#define HK_WIN 8
#define HOTKEY(mods, vk) (((DWORD)(mods) << 16) | (DWORD)(vk))
#define HOTKEY_MODS(hk) ((hk) >> 16)
#define HOTKEY_VK(hk) ((hk) & 0xFFFF)
#define HOTKEY_DEFAULT HOTKEY(HK_CTRL | HK_SHIFT, 0)

/* engine.c: chạy trên luồng hook, engine_key trả 1 nếu chặn phím. */
void engine_init(void);
int hotkey_valid(DWORD hk);
void engine_set_hotkey(DWORD hk);
DWORD engine_hotkey(void);
int engine_key(const KBDLLHOOKSTRUCT *k, DWORD mods, HWND fg);
void engine_mouse_click(void);
void engine_toggle(void);
int engine_enabled(void);
int engine_is_elevated(void);
void engine_set_notify(HWND w);

/* Tùy chọn dạng bit, lưu chung một giá trị registry: luồng giao diện đặt, luồng hook
 * đọc ở phím kế tiếp. */
#define OPT_OLD_STYLE 1   /* hòa, khỏe, thúy */
#define OPT_W_ALONE 2     /* w đứng riêng thành ư */
#define OPT_MACRO 4       /* gõ tắt */
#define OPT_AUTO_UPDATE 8 /* tự kiểm tra cập nhật mỗi ngày */
#define OPT_ALL (OPT_OLD_STYLE | OPT_W_ALONE | OPT_MACRO | OPT_AUTO_UPDATE)
#define OPT_DEFAULT (OPT_MACRO | OPT_AUTO_UPDATE)
void engine_set_options(DWORD opts);
DWORD engine_options(void);

/* autostart.c */
int autostart_enabled(void);
void autostart_set(int on);
void autostart_on_start(void);
void autostart_before_unelevate(void);
int relaunch_elevated(void);
int relaunch_normal(void);

/* datafile.c: file danh sách trong %APPDATA%\gotiengviet, chỉ dùng trên luồng chính. */
#define DATAFILE_MAX 131072 /* byte (và ký tự) đọc tối đa */
typedef struct {
    wchar_t path[MAX_PATH + 32];
    FILETIME mtime;
} DataFile;
int datafile_init(DataFile *d, const wchar_t *name);
int datafile_changed(DataFile *d); /* 1 nếu thời gian sửa file đổi từ lần trước */
int datafile_read(const DataFile *d, wchar_t *text, int cap);
int datafile_next_line(const wchar_t **s, const wchar_t **a, const wchar_t **b);
int datafile_write(DataFile *d, const char *header, const wchar_t *body, int len);
const wchar_t *path_base(const wchar_t *path);

/* exclude.c */
void exclude_init(void);
int exclude_reload_if_changed(void); /* 1 nếu file đã đổi và vừa nạp lại */
int exclude_count(void);
int exclude_has(const wchar_t *exe);
LONG exclude_version(void);
#define EXCLUDE_MAX 64
#define EXCLUDE_NAME_LEN 64
const wchar_t *exclude_get(int i);
int exclude_save(const wchar_t *const *names, int n); /* ghi file rồi nạp lại */

/* macro.c: gõ tắt; macro_lookup chạy trên luồng hook, trả độ dài nội dung (0 nếu không có). */
#define MACRO_MAX 256
#define MACRO_KEY_LEN 32
#define MACRO_VALUE_MAX 256
void macro_init(void);
int macro_reload_if_changed(void);
int macro_count(void);
int macro_lookup(const wchar_t *word, int n, wchar_t *out);
void macro_get(int i, const wchar_t **key, const wchar_t **value); /* chỉ luồng giao diện */
int macro_parse(const wchar_t *s, wchar_t *key, wchar_t *value); /* một dòng như trong file */
int macro_save(const wchar_t *const *keys, const wchar_t *const *values, int n);

/* update.c: hỏi GitHub bản phát hành mới nhất trên luồng riêng, xong thì gửi WM_GTV_UPDATE. */
enum { UPD_IDLE, UPD_CHECKING, UPD_LATEST, UPD_NEWER, UPD_ERROR };
void update_check(HWND notify);
int update_state(void);
const wchar_t *update_latest(void);
int update_is_dev(void);
int version_newer(const wchar_t *a, const wchar_t *b); /* 1 nếu a mới hơn b */
void update_open_page(void);

int selftest(void);

/* tray.c: đổi cài đặt và lưu registry, dùng chung cho menu khay và bảng điều khiển. */
#define N_HOTKEY_PRESETS 3
extern const DWORD HOTKEY_PRESETS[N_HOTKEY_PRESETS];
extern const wchar_t *const HOTKEY_PRESET_NAMES[N_HOTKEY_PRESETS];
int tray_run(void);
int tray_sound(void);
void tray_set_sound(int on);
void tray_set_hotkey(DWORD hk);
void tray_set_options(DWORD opts);
void tray_switch_admin(void);
void tray_check_update(void);
void tray_quit(void);

#define IDI_APP 1 /* icon nón lá trong gotiengviet.rc */

/* ui.c: phần dùng chung của các cửa sổ, chạy trên luồng giao diện. */
typedef struct {
    COLORREF bg, text, dim, border, field; /* field: nền ô nhập và danh sách */
} UiPalette;

typedef struct {
    int dark;
    UiPalette pal;
    HBRUSH bg, field;
} UiTheme;
extern UiTheme g_ui;

typedef struct {
    const wchar_t *cls, *text;
    DWORD style;
    int id;
    short x, y, w, h; /* theo 96 DPI */
} UiCtl;

typedef struct UiWin UiWin;
struct UiWin {
    const UiCtl *ctls;
    int nctls;
    HWND *hw;   /* nctls phần tử */
    int cw, ch; /* cỡ vùng trong theo 96 DPI */
    void (*on_layout)(UiWin *u); /* sau khi xếp control (đổi DPI), có thể 0 */
    void (*on_theme)(UiWin *u);  /* sau khi đổi sáng/tối, có thể 0 */
    HWND w, focus;
    UINT dpi;
    HFONT font, bold;
    HICON icon_big, icon_small;
};

#define UI_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU)
int ui_create(UiWin *u, const wchar_t *cls, WNDPROC proc, const wchar_t *title, HWND owner); /* 0 nếu đã mở */
void ui_show(UiWin *u, int focus_id); /* hiện sau khi đã điền nội dung */
int ui_handle(UiWin *u, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res);
int ui_scale(const UiWin *u, int v);
void ui_load_theme(void);
void ui_apply_theme(UiWin *u);

/* listedit.c: cửa sổ sửa danh sách app loại trừ và gõ tắt. */
enum { LIST_EXCLUDE, LIST_MACRO };
void listedit_show(int kind, HWND owner);
int listedit_dialog_message(MSG *msg);

/* settings.c: bảng điều khiển, chạy trên luồng giao diện. */
void settings_show(void);
void settings_refresh(void);
int settings_dialog_message(MSG *msg);

/* log.c: tắt mặc định, không gọi từ callback hook. */
void log_init(int enabled);
int log_enabled(void);
void log_w(const wchar_t *fmt, ...);

#endif
