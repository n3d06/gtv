#ifndef GTV_H
#define GTV_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "telex.h"

/* dwExtraInfo của phím do bộ gõ phát ra; _END đánh dấu sự kiện cuối của một lần phát. */
#define GTV_MAGIC ((ULONG_PTR)0x47545621)
#define GTV_MAGIC_END ((ULONG_PTR)0x47545622)

#define WM_GTV_STATE (WM_APP + 2)

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

/* autostart.c */
int autostart_enabled(void);
void autostart_set(int on);
void autostart_on_start(void);
void autostart_before_unelevate(void);
int relaunch_elevated(void);
int relaunch_normal(void);

/* exclude.c */
void exclude_init(void);
void exclude_reload_if_changed(void);
int exclude_has(const wchar_t *exe);
LONG exclude_version(void);
void exclude_open_file(void);
const wchar_t *path_base(const wchar_t *path);

int selftest(void);
int tray_run(void);

/* log.c: tắt mặc định, không gọi từ callback hook. */
void log_init(int enabled);
int log_enabled(void);
void log_w(const wchar_t *fmt, ...);

#endif
