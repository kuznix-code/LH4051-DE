#include "longhorn.h"

#ifdef G_OS_WIN32
/* ----------------------------------------------------------------------------
 * Windows child processes for the App Store.
 *
 * lh4051-de is a GUI-subsystem program: it has no console, so its C runtime standard
 * descriptors 0, 1 and 2 are invalid. GLib's spawn code asks the CRT for those descriptors
 * whenever a standard stream is not explicitly redirected, and that fails with
 * "Failed to execute child process (Bad file descriptor)". GLib is therefore bypassed:
 * the child is created with CreateProcessW and every standard handle is given explicitly
 * (stdin = NUL, stdout/stderr = an anonymous pipe or NUL), without a console window.
 * ---------------------------------------------------------------------------- */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gio/gwin32inputstream.h>
#include <string.h>

/* Appends one argument quoted for CommandLineToArgvW / the MSVC runtime. */
static void append_quoted(GString *cmd, const char *arg)
{
    gboolean quote = !*arg || strpbrk(arg, " \t\n\v\"") != NULL;
    guint backslashes = 0;

    if (cmd->len) g_string_append_c(cmd, ' ');
    if (!quote) { g_string_append(cmd, arg); return; }
    g_string_append_c(cmd, '"');
    for (const char *p = arg; ; p++) {
        if (*p == '\\') { backslashes++; continue; }
        if (*p == '"' || !*p) {
            /* backslashes in front of a quote (or the closing quote) are doubled */
            for (guint i = 0; i < backslashes * 2; i++) g_string_append_c(cmd, '\\');
            if (*p == '"') g_string_append(cmd, "\\\"");
            backslashes = 0;
            if (!*p) break;
            continue;
        }
        for (guint i = 0; i < backslashes; i++) g_string_append_c(cmd, '\\');
        backslashes = 0;
        g_string_append_c(cmd, *p);
    }
    g_string_append_c(cmd, '"');
}

static void set_win_error(GError **error, const char *what, const char *program, DWORD code)
{
    char *msg = g_win32_error_message(code);
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s \"%s\": %s (error %lu)",
                what, program, msg ? g_strstrip(msg) : "unknown", (unsigned long)code);
    g_free(msg);
}

/* Starts argv (NULL-terminated, UTF-8) with stdin = NUL. Returns a stream that reads the child's
 * stdout (and stderr when merge_stderr, otherwise stderr goes to NUL). *pid receives the process
 * handle; pass it to g_child_watch_add() and release it with g_spawn_close_pid(). */
GInputStream *lh4051_win_spawn(const char *const *argv, gboolean merge_stderr, GPid *pid, GError **error)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE rd = NULL, wr = NULL, nul_in = INVALID_HANDLE_VALUE, nul_err = INVALID_HANDLE_VALUE;
    GString *cmd = g_string_new(NULL);
    wchar_t *wcmd;
    BOOL ok;
    DWORD code;

    if (!argv || !argv[0]) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "No command given.");
        g_string_free(cmd, TRUE);
        return NULL;
    }
    for (int i = 0; argv[i]; i++) append_quoted(cmd, argv[i]);
    wcmd = (wchar_t *)g_utf8_to_utf16(cmd->str, -1, NULL, NULL, error);
    g_string_free(cmd, TRUE);
    if (!wcmd) return NULL;

    ZeroMemory(&sa, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        set_win_error(error, "Could not create a pipe for", argv[0], GetLastError());
        g_free(wcmd);
        return NULL;
    }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0); /* the child must not inherit our read end */

    nul_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!merge_stderr)
        nul_err = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = nul_in;
    si.hStdOutput = wr;
    si.hStdError = merge_stderr ? wr : nul_err;
    ZeroMemory(&pi, sizeof pi);

    ok = CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    code = ok ? 0 : GetLastError();

    g_free(wcmd);
    CloseHandle(wr);
    if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
    if (nul_err != INVALID_HANDLE_VALUE) CloseHandle(nul_err);
    if (!ok) {
        CloseHandle(rd);
        set_win_error(error, "Could not start", argv[0], code);
        return NULL;
    }
    CloseHandle(pi.hThread);
    *pid = (GPid)pi.hProcess;
    return g_win32_input_stream_new(rd, TRUE);
}
#endif /* G_OS_WIN32 */
