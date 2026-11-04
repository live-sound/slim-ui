#ifndef SLIM_UI_WIN32_TEST_SUPPORT_H
#define SLIM_UI_WIN32_TEST_SUPPORT_H

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

static void sui_test_window_shutdown(void);

/* One reusable native shell per test executable. Tests still own a fresh host,
 * renderer and tree, and must detach them (including timers/workers) before
 * releasing the shell. Lifecycle and simultaneous-window tests use their own HWNDs. */
static HWND sui_test_cached_window;
static int sui_test_window_in_use;

static HWND sui_test_window_acquire(const WNDCLASSEXW* wc, const wchar_t* title,
                                    DWORD ex_style, DWORD style, int x, int y, int width, int height) {
    RECT bounds;
    if (sui_test_window_in_use) return NULL;
    if (!sui_test_cached_window) {
        if (!RegisterClassExW(wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return NULL;
        sui_test_cached_window = CreateWindowExW(ex_style, wc->lpszClassName, title, style,
                                                 x, y, width, height, NULL, NULL, wc->hInstance, NULL);
        if (!sui_test_cached_window) return NULL;
        atexit(sui_test_window_shutdown);
    } else {
        /* No fixture is connected while restoring geometry altered by a test. */
        if (IsIconic(sui_test_cached_window) || IsZoomed(sui_test_cached_window))
            ShowWindow(sui_test_cached_window, SW_SHOWNOACTIVATE);
        GetWindowRect(sui_test_cached_window, &bounds);
        if (bounds.left != x || bounds.top != y || bounds.right - bounds.left != width || bounds.bottom - bounds.top != height)
            SetWindowPos(sui_test_cached_window, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    sui_test_window_in_use = 1;
    return sui_test_cached_window;
}

static void sui_test_window_release(HWND hwnd) {
    MSG message;
    if (!hwnd) return;
    /* Detach first: focus/capture changes and PeekMessage can synchronously
     * deliver messages. Never leave a pointer to the departing stack fixture. */
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    if (GetCapture() == hwnd) ReleaseCapture();
    if (GetFocus() == hwnd) SetFocus(NULL);
    ValidateRect(hwnd, NULL);
    while (PeekMessageW(&message, hwnd, 0, 0, PM_REMOVE)) {}
    sui_test_window_in_use = 0;
}

static void sui_test_window_shutdown(void) {
    if (sui_test_cached_window) {
        DestroyWindow(sui_test_cached_window);
        sui_test_cached_window = NULL;
    }
}

/* A whole process must start on the private desktop: moving only the test thread
 * leaves framework/IME threads on the interactive desktop. Real focus and native
 * menus can then run without changing the user's foreground window or keyboard.
 * Keep the desktop handle in the launcher until all child UI threads have exited. */
static int sui_test_main(int argc, const char* const argv[], int (*run)(int, const char* const[])) {
    static const char marker[] = "--sui-private-desktop";
    static const wchar_t suffix[] = L" --sui-private-desktop";
    wchar_t name[64], executable[32768];
    HDESK desktop;
    HANDLE job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    const wchar_t* original = GetCommandLineW();
    const size_t length = wcslen(original);
    wchar_t* command;
    DWORD result = 1;
    DWORD path_length;
    if (argc > 1 && strcmp(argv[argc - 1], marker) == 0) {
        DWORD bytes = 0;
        if (!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, name, sizeof(name), &bytes) ||
            wcsncmp(name, L"SlimUiTests-", 12) != 0) {
            fprintf(stderr, "Tests require a private desktop.\n");
            return 1;
        }
        /* An assert/abort on this private desktop must report and exit, never wait
         * in an invisible CRT error dialog. Prepared before any allocation probe. */
#if defined(_MSC_VER)
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
        setvbuf(stdout, NULL, _IONBF, 0);
        return run(argc - 1, argv);
    }
    path_length = GetModuleFileNameW(NULL, executable, 32768);
    if (!path_length || path_length >= 32768 || length + sizeof(suffix) / sizeof(wchar_t) > 32767) return 1;
    swprintf(name, sizeof(name) / sizeof(name[0]), L"SlimUiTests-%lu", GetCurrentProcessId());
    desktop = CreateDesktopW(name, NULL, NULL, 0, GENERIC_ALL, NULL);
    if (!desktop) { fprintf(stderr, "Cannot create the private test desktop: %lu\n", GetLastError()); return 1; }
    command = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, length * sizeof(wchar_t) + sizeof(suffix));
    job = CreateJobObjectW(NULL, NULL);
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (command && job && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        memcpy(command, original, length * sizeof(wchar_t));
        memcpy(command + length, suffix, sizeof(suffix));
        startup.cb = sizeof(startup); startup.lpDesktop = name;
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE); startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        if (CreateProcessW(executable, command, NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
            if (AssignProcessToJobObject(job, process.hProcess) && ResumeThread(process.hThread) != (DWORD)-1) {
                WaitForSingleObject(process.hProcess, INFINITE);
                GetExitCodeProcess(process.hProcess, &result);
            } else {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, INFINITE);
            }
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
        }
    }
    if (result) fprintf(stderr, "Private desktop test run failed (exit %lu).\n", result);
    if (job) CloseHandle(job);
    if (command) HeapFree(GetProcessHeap(), 0, command);
    if (!CloseDesktop(desktop)) {
        fprintf(stderr, "Cannot close the private test desktop: %lu\n", GetLastError());
        if (!result) result = 1;
    }
    return (int)result;
}

#endif
