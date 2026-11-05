/* Exercise downlevel startup without changing the test process's DPI mode. */
#include <windows.h>
#include <string.h>
#include "utest.h"

static struct {
    int context_available, shcore_available, awareness_available;
    BOOL context_result;
    DWORD context_error;
    HRESULT awareness_result;
    int context_calls, awareness_calls, legacy_calls, loads, frees;
    HANDLE context;
    int awareness;
} dpi;

static BOOL WINAPI test_set_context(HANDLE context) {
    ++dpi.context_calls;
    dpi.context = context;
    SetLastError(dpi.context_error);
    return dpi.context_result;
}
static HRESULT WINAPI test_set_awareness(int awareness) {
    ++dpi.awareness_calls;
    dpi.awareness = awareness;
    return dpi.awareness_result;
}
static FARPROC WINAPI test_get_proc(HMODULE module, LPCSTR name) {
    (void)module;
    if (strcmp(name, "SetProcessDpiAwarenessContext") == 0)
        return dpi.context_available ? (FARPROC)test_set_context : NULL;
    if (strcmp(name, "SetProcessDpiAwareness") == 0)
        return dpi.awareness_available ? (FARPROC)test_set_awareness : NULL;
    return NULL;
}
static HMODULE WINAPI test_load_library(LPCWSTR name) {
    (void)name;
    ++dpi.loads;
    return dpi.shcore_available ? (HMODULE)(INT_PTR)1 : NULL;
}
static BOOL WINAPI test_free_library(HMODULE module) {
    (void)module;
    ++dpi.frees;
    return TRUE;
}
static BOOL WINAPI test_set_legacy(void) { ++dpi.legacy_calls; return TRUE; }

#define GetProcAddress test_get_proc
#define LoadLibraryW test_load_library
#define FreeLibrary test_free_library
#define SetProcessDPIAware test_set_legacy
#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>
#undef GetProcAddress
#undef LoadLibraryW
#undef FreeLibrary
#undef SetProcessDPIAware

UTEST(dpi, modern_windows_uses_per_monitor_v2) {
    memset(&dpi, 0, sizeof(dpi));
    dpi.context_available = 1;
    dpi.context_result = TRUE;
    sui_enable_dpi_awareness();
    EXPECT_EQ(1, dpi.context_calls);
    EXPECT_EQ((INT_PTR)-4, (INT_PTR)dpi.context);
    EXPECT_EQ(0, dpi.loads);
    EXPECT_EQ(0, dpi.legacy_calls);
}

UTEST(dpi, unsupported_context_falls_back_to_per_monitor_v1) {
    memset(&dpi, 0, sizeof(dpi));
    dpi.context_available = dpi.shcore_available = dpi.awareness_available = 1;
    dpi.context_error = ERROR_INVALID_PARAMETER;
    sui_enable_dpi_awareness();
    EXPECT_EQ(1, dpi.context_calls);
    EXPECT_EQ(1, dpi.awareness_calls);
    EXPECT_EQ(2, dpi.awareness);
    EXPECT_EQ(1, dpi.frees);
    EXPECT_EQ(0, dpi.legacy_calls);
}

UTEST(dpi, windows_81_uses_shcore_without_new_user32_export) {
    memset(&dpi, 0, sizeof(dpi));
    dpi.shcore_available = dpi.awareness_available = 1;
    sui_enable_dpi_awareness();
    EXPECT_EQ(0, dpi.context_calls);
    EXPECT_EQ(1, dpi.awareness_calls);
    EXPECT_EQ(2, dpi.awareness);
    EXPECT_EQ(1, dpi.frees);
    EXPECT_EQ(0, dpi.legacy_calls);
}

UTEST(dpi, windows_7_uses_system_dpi_without_shcore) {
    memset(&dpi, 0, sizeof(dpi));
    sui_enable_dpi_awareness();
    EXPECT_EQ(0, dpi.context_calls);
    EXPECT_EQ(0, dpi.awareness_calls);
    EXPECT_EQ(0, dpi.frees);
    EXPECT_EQ(1, dpi.legacy_calls);
}

UTEST(dpi, missing_shcore_export_releases_library_and_falls_back) {
    memset(&dpi, 0, sizeof(dpi));
    dpi.shcore_available = 1;
    sui_enable_dpi_awareness();
    EXPECT_EQ(0, dpi.awareness_calls);
    EXPECT_EQ(1, dpi.frees);
    EXPECT_EQ(1, dpi.legacy_calls);
}

UTEST(dpi, failed_shcore_call_falls_back_to_system_dpi) {
    memset(&dpi, 0, sizeof(dpi));
    dpi.shcore_available = dpi.awareness_available = 1;
    dpi.awareness_result = E_FAIL;
    sui_enable_dpi_awareness();
    EXPECT_EQ(1, dpi.frees);
    EXPECT_EQ(1, dpi.legacy_calls);
}

UTEST(dpi, existing_modern_awareness_is_preserved) {
    memset(&dpi, 0, sizeof(dpi));
    dpi.context_available = 1;
    dpi.context_error = ERROR_ACCESS_DENIED;
    sui_enable_dpi_awareness();
    EXPECT_EQ(1, dpi.context_calls);
    EXPECT_EQ(0, dpi.loads);
    EXPECT_EQ(0, dpi.legacy_calls);
}

UTEST(dpi, existing_windows_81_awareness_is_preserved) {
    memset(&dpi, 0, sizeof(dpi));
    dpi.shcore_available = dpi.awareness_available = 1;
    dpi.awareness_result = E_ACCESSDENIED;
    sui_enable_dpi_awareness();
    EXPECT_EQ(1, dpi.awareness_calls);
    EXPECT_EQ(1, dpi.frees);
    EXPECT_EQ(0, dpi.legacy_calls);
}

UTEST_MAIN();
