/*
 * Khởi động cùng Windows: quyền thường dùng khóa HKCU\...\Run; quyền admin dùng task
 * "highest privileges" vì khóa Run không nâng quyền được. Task tạo từ XML để tắt các
 * mặc định không hợp: không chạy khi dùng pin, tự dừng sau 3 ngày, ưu tiên thấp.
 */
#include "gtv.h"
#include <shellapi.h>

static const wchar_t RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t APP_KEY[] = L"Software\\gotiengviet";
static const wchar_t NAME[] = L"gotiengviet";

static int exe_path(wchar_t *buf)
{
    DWORD n = GetModuleFileNameW(0, buf, MAX_PATH);
    return n > 0 && n < MAX_PATH;
}

static int run_key_exists(void)
{
    return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, NAME, RRF_RT_REG_SZ, 0, 0, 0) == ERROR_SUCCESS;
}

static void run_key_set(int on)
{
    static wchar_t path[MAX_PATH], cmd[MAX_PATH + 3];
    int n;
    if (!on) {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, NAME);
        return;
    }
    if (!exe_path(path)) return;
    n = wsprintfW(cmd, L"\"%s\"", path);
    RegSetKeyValueW(HKEY_CURRENT_USER, RUN_KEY, NAME, REG_SZ, cmd, (DWORD)(n + 1) * sizeof(wchar_t));
}

/* Tự ghi cờ trong HKCU: quyền thường không đọc tin cậy được danh sách task. */
static int task_exists(void)
{
    DWORD v = 0, n = sizeof v;
    return RegGetValueW(HKEY_CURRENT_USER, APP_KEY, L"AdminTask", RRF_RT_REG_DWORD, 0, &v, &n) == ERROR_SUCCESS && v;
}

static void task_mark(int on)
{
    DWORD v = 1;
    if (on) RegSetKeyValueW(HKEY_CURRENT_USER, APP_KEY, L"AdminTask", REG_DWORD, &v, sizeof v);
    else RegDeleteKeyValueW(HKEY_CURRENT_USER, APP_KEY, L"AdminTask");
}

/* "runas": đang admin thì không hiện UAC, đang quyền thường thì Windows hỏi. */
static int run_schtasks(const wchar_t *args)
{
    static wchar_t app[MAX_PATH + 16];
    SHELLEXECUTEINFOW sei = {0};
    DWORD code = 1;
    UINT n = GetSystemDirectoryW(app, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    lstrcatW(app, L"\\schtasks.exe");
    sei.cbSize = sizeof sei;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = app;
    sei.lpParameters = args;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return 0;
    if (WaitForSingleObject(sei.hProcess, 15000) == WAIT_OBJECT_0) GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return code == 0;
}

static void xml_escape(wchar_t *dst, const wchar_t *src, int cap)
{
    int n = 0;
    for (; *src && n < cap - 6; src++) {
        if (*src == '&') n += wsprintfW(dst + n, L"&amp;");
        else if (*src == '<') n += wsprintfW(dst + n, L"&lt;");
        else if (*src == '>') n += wsprintfW(dst + n, L"&gt;");
        else dst[n++] = *src;
    }
    dst[n] = 0;
}

static void write_w(HANDLE f, const wchar_t *s)
{
    DWORD done;
    WriteFile(f, s, (DWORD)lstrlenW(s) * sizeof(wchar_t), &done, 0);
}

static int write_task_xml(const wchar_t *file)
{
    static wchar_t path[MAX_PATH], raw[256], user[1024], cmd[5 * MAX_PATH];
    HANDLE f;
    DWORD n;

    if (!exe_path(path)) return 0;
    xml_escape(cmd, path, 5 * MAX_PATH);
    n = GetEnvironmentVariableW(L"USERDOMAIN", raw, 128);
    if (n && n < 128) lstrcatW(raw, L"\\");
    else raw[0] = 0;
    n = GetEnvironmentVariableW(L"USERNAME", raw + lstrlenW(raw), 128);
    if (!n || n >= 128) return 0;
    xml_escape(user, raw, 1024);

    f = CreateFileW(file, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, 0);
    if (f == INVALID_HANDLE_VALUE) return 0;
    write_w(f, L"\xFEFF<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
               L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
               L"<RegistrationInfo><Description>Bộ gõ tiếng Việt gotiengviet (quyền admin)</Description></RegistrationInfo>\r\n"
               L"<Triggers><LogonTrigger><Enabled>true</Enabled><UserId>");
    write_w(f, user);
    write_w(f, L"</UserId></LogonTrigger></Triggers>\r\n<Principals><Principal id=\"Author\"><UserId>");
    write_w(f, user);
    write_w(f, L"</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel>"
               L"</Principal></Principals>\r\n"
               L"<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
               L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"
               L"<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
               L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Priority>4</Priority></Settings>\r\n"
               L"<Actions Context=\"Author\"><Exec><Command>");
    write_w(f, cmd);
    write_w(f, L"</Command></Exec></Actions>\r\n</Task>\r\n");
    CloseHandle(f);
    return 1;
}

static int task_create(void)
{
    static wchar_t file[MAX_PATH + 32], args[MAX_PATH + 64];
    int ok;
    DWORD n = GetTempPathW(MAX_PATH, file);
    if (!n || n >= MAX_PATH) return 0;
    lstrcatW(file, L"gotiengviet-task.xml");
    if (!write_task_xml(file)) return 0;
    wsprintfW(args, L"/Create /TN gotiengviet /F /XML \"%s\"", file);
    ok = run_schtasks(args);
    DeleteFileW(file);
    if (ok) task_mark(1);
    return ok;
}

static int task_delete(void)
{
    if (!run_schtasks(L"/Delete /TN gotiengviet /F")) return 0;
    task_mark(0);
    return 1;
}

int autostart_enabled(void) { return run_key_exists() || task_exists(); }

void autostart_set(int on)
{
    if (!on) {
        run_key_set(0);
        if (task_exists()) task_delete();
    } else if (engine_is_elevated() && task_create()) {
        run_key_set(0);
    } else {
        run_key_set(1);
    }
}

void autostart_on_start(void)
{
    if (engine_is_elevated() && run_key_exists() && task_create()) run_key_set(0);
}

void autostart_before_unelevate(void)
{
    if (task_exists() && task_delete()) run_key_set(1);
}

int relaunch_elevated(void)
{
    static wchar_t path[MAX_PATH];
    SHELLEXECUTEINFOW sei = {0};
    if (!exe_path(path)) return 0;
    sei.cbSize = sizeof sei;
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei);
}

/* Nhờ explorer.exe chạy exe để bản mới không thừa hưởng quyền admin. */
int relaunch_normal(void)
{
    static wchar_t path[MAX_PATH], explorer[MAX_PATH + 16], arg[MAX_PATH + 3];
    UINT n = GetWindowsDirectoryW(explorer, MAX_PATH);
    if (!exe_path(path) || !n || n >= MAX_PATH) return 0;
    lstrcatW(explorer, L"\\explorer.exe");
    wsprintfW(arg, L"\"%s\"", path);
    return (INT_PTR)ShellExecuteW(0, L"open", explorer, arg, 0, SW_SHOWNORMAL) > 32;
}
