/*
 * Cửa sổ sửa danh sách app loại trừ và gõ tắt: một ô nhập, nút Thêm, danh sách một cột.
 * Mỗi dòng gõ tắt hiện đúng như trong file ("vn - Việt Nam"), ô nhập đọc theo cùng quy tắc.
 * Danh sách hiển thị chính là dữ liệu: mỗi lần thêm/sửa/xóa là ghi lại cả file rồi nạp lại,
 * như các ô trong bảng điều khiển.
 */
#define UNICODE /* macro ListView_* dùng bản W */
#include "gtv.h"
#include <commctrl.h>

enum { ID_ADD = IDOK, ID_CLOSE = IDCANCEL, ID_DELETE = 10, ID_LIST, ID_ENTRY };

#define GROUP (WS_GROUP | WS_TABSTOP)
#define PUSH (BS_PUSHBUTTON | GROUP)
#define LIST (LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_BORDER | GROUP)
#define ROW_MAX (MACRO_KEY_LEN + MACRO_VALUE_MAX + 4) /* "chữ tắt - nội dung" */

static const UiCtl EXCLUDE_CTLS[] = {
    {L"STATIC", L"Chọn app đang mở hoặc gõ tên file exe:", SS_LEFT, -1, 12, 12, 356, 20},
    {L"COMBOBOX", L"", CBS_DROPDOWN | CBS_SORT | CBS_AUTOHSCROLL | WS_VSCROLL | GROUP, ID_ENTRY, 12, 34, 260, 240},
    {L"BUTTON", L"Thêm", BS_DEFPUSHBUTTON | GROUP, ID_ADD, 280, 33, 88, 28},
    {WC_LISTVIEWW, L"", LIST, ID_LIST, 12, 72, 356, 236},
    {L"BUTTON", L"Xóa", PUSH, ID_DELETE, 12, 320, 88, 30},
    {L"BUTTON", L"Đóng", PUSH, ID_CLOSE, 280, 320, 88, 30},
};

static const UiCtl MACRO_CTLS[] = {
    {L"STATIC", L"Gõ chữ tắt - nội dung, ví dụ: vn - Việt Nam", SS_LEFT, -1, 12, 12, 436, 20},
    {L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | GROUP, ID_ENTRY, 12, 34, 340, 26},
    {L"BUTTON", L"Thêm", BS_DEFPUSHBUTTON | GROUP, ID_ADD, 360, 33, 88, 28},
    {WC_LISTVIEWW, L"", LIST, ID_LIST, 12, 72, 436, 280},
    {L"BUTTON", L"Xóa", PUSH, ID_DELETE, 12, 364, 88, 30},
    {L"BUTTON", L"Đóng", PUSH, ID_CLOSE, 360, 364, 88, 30},
};

#define COUNT(a) (int)(sizeof a / sizeof a[0])

static HWND g_ex_hw[COUNT(EXCLUDE_CTLS)], g_mac_hw[COUNT(MACRO_CTLS)];
static void size_column(UiWin *u);
static UiWin g_wins[2] = {
    {EXCLUDE_CTLS, COUNT(EXCLUDE_CTLS), g_ex_hw, 380, 362, size_column, 0},
    {MACRO_CTLS, COUNT(MACRO_CTLS), g_mac_hw, 460, 406, size_column, 0},
};

static int is_macro(const UiWin *u) { return u == &g_wins[LIST_MACRO]; }
static HWND list_of(const UiWin *u) { return GetDlgItem(u->w, ID_LIST); }
static HWND entry_of(const UiWin *u) { return GetDlgItem(u->w, ID_ENTRY); }

/* Cột duy nhất lấp kín chiều ngang (tính cả khi thanh cuộn hiện/ẩn). */
static void size_column(UiWin *u)
{
    ListView_SetColumnWidth(list_of(u), 0, LVSCW_AUTOSIZE_USEHEADER);
}

static int add_row(HWND lv, const wchar_t *text)
{
    LVITEMW it;
    it.mask = LVIF_TEXT;
    it.iItem = ListView_GetItemCount(lv);
    it.iSubItem = 0;
    it.pszText = (wchar_t *)text;
    return ListView_InsertItem(lv, &it);
}

/* Tách dòng gõ tắt thành chữ tắt và nội dung; app loại trừ thì cả dòng là "chữ tắt". */
static int split(const UiWin *u, const wchar_t *row, wchar_t *key, wchar_t *value)
{
    if (is_macro(u)) return macro_parse(row, key, value);
    lstrcpynW(key, row, MACRO_KEY_LEN + EXCLUDE_NAME_LEN);
    return 1;
}

/* Dòng có cùng chữ tắt / tên exe (không phân biệt hoa/thường), -1 nếu không có. */
static int find_row(const UiWin *u, const wchar_t *key)
{
    wchar_t row[ROW_MAX], k[MACRO_KEY_LEN + EXCLUDE_NAME_LEN], v[MACRO_VALUE_MAX + 1];
    HWND lv = list_of(u);
    int i, n = ListView_GetItemCount(lv);
    for (i = 0; i < n; i++) {
        ListView_GetItemText(lv, i, 0, row, ROW_MAX);
        if (split(u, row, k, v) && lstrcmpiW(k, key) == 0) return i;
    }
    return -1;
}

static void select_row(HWND lv, int i)
{
    ListView_SetItemState(lv, -1, 0, LVIS_SELECTED);
    ListView_SetItemState(lv, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(lv, i, FALSE);
}

/* Ghi cả danh sách đang hiện ra file, rồi cập nhật số mục trên bảng điều khiển. */
static void save(UiWin *u)
{
    static wchar_t keys[MACRO_MAX][MACRO_KEY_LEN + EXCLUDE_NAME_LEN], values[MACRO_MAX][MACRO_VALUE_MAX + 1];
    static const wchar_t *pk[MACRO_MAX], *pv[MACRO_MAX];
    wchar_t row[ROW_MAX];
    HWND lv = list_of(u);
    int i, n = 0, count = ListView_GetItemCount(lv);
    for (i = 0; i < count && n < MACRO_MAX; i++) {
        ListView_GetItemText(lv, i, 0, row, ROW_MAX);
        if (!split(u, row, keys[n], values[n])) continue;
        pk[n] = keys[n];
        pv[n] = values[n];
        n++;
    }
    if (is_macro(u)) macro_save(pk, pv, n);
    else exclude_save(pk, n);
    size_column(u);
    settings_refresh();
}

/* Bỏ khoảng trắng hai đầu chữ trong ô, trả độ dài. */
static int read_trimmed(HWND h, wchar_t *s, int cap)
{
    int n = GetWindowTextW(h, s, cap), i = 0, j;
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    for (j = 0; j < n - i; j++) s[j] = s[i + j];
    s[n - i] = 0;
    return n - i;
}

/* Đọc ô nhập thành một dòng của danh sách; 0 nếu không hợp lệ. */
static int read_entry(const UiWin *u, wchar_t *row, wchar_t *key)
{
    wchar_t value[MACRO_VALUE_MAX + 1];
    int n = read_trimmed(entry_of(u), row, ROW_MAX);
    if (is_macro(u)) {
        if (!macro_parse(row, key, value)) return 0;
        wsprintfW(row, L"%s - %s", key, value); /* viết lại cho đều một kiểu */
        return 1;
    }
    if (!n || n + 4 >= EXCLUDE_NAME_LEN) return 0;
    if (n < 4 || lstrcmpiW(row + n - 4, L".exe") != 0) lstrcatW(row, L".exe"); /* "abc" là abc.exe */
    lstrcpyW(key, row);
    return 1;
}

static void add(UiWin *u)
{
    wchar_t row[ROW_MAX], key[MACRO_KEY_LEN + EXCLUDE_NAME_LEN];
    HWND lv = list_of(u);
    int i, max = is_macro(u) ? MACRO_MAX : EXCLUDE_MAX;
    if (!read_entry(u, row, key)) {
        MessageBeep(MB_ICONWARNING);
        SetFocus(entry_of(u));
        return;
    }
    i = find_row(u, key);
    if (i >= 0) {
        ListView_SetItemText(lv, i, 0, row);
    } else if (ListView_GetItemCount(lv) < max) {
        i = add_row(lv, row);
    } else {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    save(u);
    select_row(lv, i);
    SetWindowTextW(entry_of(u), L"");
    SetFocus(entry_of(u));
}

static void delete_selected(UiWin *u)
{
    HWND lv = list_of(u);
    int i, any = 0;
    for (i = ListView_GetItemCount(lv) - 1; i >= 0; i--)
        if (ListView_GetItemState(lv, i, LVIS_SELECTED)) {
            ListView_DeleteItem(lv, i);
            any = 1;
        }
    if (any) save(u);
    else MessageBeep(MB_ICONWARNING);
}

/* Chữ tắt đã có thì nút thành "Lưu" (sửa nội dung), chưa có thì "Thêm". */
static void update_add_label(UiWin *u)
{
    wchar_t row[ROW_MAX], key[MACRO_KEY_LEN + EXCLUDE_NAME_LEN];
    int exists = read_entry(u, row, key) && find_row(u, key) >= 0;
    SetDlgItemTextW(u->w, ID_ADD, exists ? L"Lưu" : L"Thêm");
}

/* Bấm một dòng gõ tắt thì đưa lên ô nhập để sửa. */
static void edit_selected(UiWin *u)
{
    wchar_t row[ROW_MAX];
    HWND lv = list_of(u);
    int i = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
    if (i < 0 || ListView_GetSelectedCount(lv) != 1) return;
    ListView_GetItemText(lv, i, 0, row, ROW_MAX);
    SetWindowTextW(entry_of(u), row);
}

/* Gợi ý tên exe của các cửa sổ đang mở (trừ app đã có trong danh sách). */
static BOOL CALLBACK add_running_app(HWND w, LPARAM lp)
{
    UiWin *u = (UiWin *)lp;
    wchar_t path[MAX_PATH];
    DWORD pid = 0, len = MAX_PATH;
    HANDLE h;
    if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER) || (GetWindowLongW(w, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ||
        !GetWindowTextLengthW(w))
        return TRUE;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId()) return TRUE;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;
    if (QueryFullProcessImageNameW(h, 0, path, &len)) {
        const wchar_t *exe = path_base(path);
        HWND box = entry_of(u);
        if (find_row(u, exe) < 0 && SendMessageW(box, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)exe) == CB_ERR)
            SendMessageW(box, CB_ADDSTRING, 0, (LPARAM)exe);
    }
    CloseHandle(h);
    return TRUE;
}

static void fill(UiWin *u)
{
    wchar_t row[ROW_MAX];
    LVCOLUMNW col;
    HWND lv = list_of(u), hdr = ListView_GetHeader(lv);
    int i;
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    /* Không cho kéo đổi độ rộng cột: kéo hẹp thì phần trống trông như thêm một cột. */
    SetWindowLongW(hdr, GWL_STYLE, GetWindowLongW(hdr, GWL_STYLE) | HDS_NOSIZING);
    col.mask = LVCF_TEXT;
    col.pszText = is_macro(u) ? L"Chữ tắt - nội dung" : L"Tên file exe";
    ListView_InsertColumn(lv, 0, &col);
    if (is_macro(u)) {
        for (i = 0; i < macro_count(); i++) {
            const wchar_t *k, *v;
            macro_get(i, &k, &v);
            wsprintfW(row, L"%s - %s", k, v);
            add_row(lv, row);
        }
        SendMessageW(entry_of(u), EM_LIMITTEXT, ROW_MAX - 1, 0);
    } else {
        for (i = 0; i < exclude_count(); i++) add_row(lv, exclude_get(i));
        SendMessageW(entry_of(u), CB_LIMITTEXT, EXCLUDE_NAME_LEN - 5, 0);
        EnumWindows(add_running_app, (LPARAM)u);
    }
    size_column(u);
}

static LRESULT proc(UiWin *u, HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;
    if (ui_handle(u, msg, wp, lp, &res)) return res;
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ADD: add(u); return 0;
        case ID_DELETE: delete_selected(u); return 0;
        case ID_CLOSE: DestroyWindow(w); return 0;
        case ID_ENTRY:
            if (is_macro(u) && HIWORD(wp) == EN_CHANGE) update_add_label(u);
            return 0;
        }
        return 0;
    case WM_NOTIFY: {
        const NMHDR *n = (const NMHDR *)lp;
        if (n->idFrom != ID_LIST) return 0;
        if (n->code == LVN_KEYDOWN && ((const NMLVKEYDOWN *)lp)->wVKey == VK_DELETE) delete_selected(u);
        else if (n->code == LVN_ITEMCHANGED && is_macro(u) && (((const NMLISTVIEW *)lp)->uNewState & LVIS_SELECTED))
            edit_selected(u);
        return 0;
    }
    }
    return DefWindowProcW(w, msg, wp, lp);
}

static LRESULT CALLBACK exclude_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    return proc(&g_wins[LIST_EXCLUDE], w, msg, wp, lp);
}

static LRESULT CALLBACK macro_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    return proc(&g_wins[LIST_MACRO], w, msg, wp, lp);
}

void listedit_show(int kind, HWND owner)
{
    UiWin *u = &g_wins[kind];
    int ok = kind == LIST_MACRO
                 ? ui_create(u, L"GotiengvietMacros", macro_proc, L"Danh sách gõ tắt", owner)
                 : ui_create(u, L"GotiengvietExclude", exclude_proc, L"Danh sách app loại trừ", owner);
    if (!ok) return;
    fill(u);
    ui_show(u, ID_ENTRY);
}

int listedit_dialog_message(MSG *msg)
{
    int i;
    for (i = 0; i < 2; i++)
        if (g_wins[i].w && IsDialogMessageW(g_wins[i].w, msg)) return 1;
    return 0;
}
