#include "../flags_api.h"

#include "../../localappdata/localappdata_reader.h"
#include "../../../logging/core_log.h"
#include "../../../sync/spin_lock.h"

#include <stdio.h>
#include <windows.h>

static volatile LONG g_kbo_runtime_threads_stop_requested = 0;
static volatile LONG g_kbo_runtime_thread_registry_lock = 0;

#define KBO_RUNTIME_THREAD_MAX 64

typedef struct KboRuntimeThreadEntry {
    HANDLE handle;
    char label[64];
} KboRuntimeThreadEntry;

static KboRuntimeThreadEntry g_kbo_runtime_threads[KBO_RUNTIME_THREAD_MAX];

static int kbo_should_log_runtime_thread_registration(void)
{
    static LONG runtime_registration_log_count = 0;
    return InterlockedIncrement(&runtime_registration_log_count) <= 512;
}

static void kbo_lock_runtime_thread_registry(void)
{
    kbo_spin_lock(&g_kbo_runtime_thread_registry_lock);
}

static void kbo_unlock_runtime_thread_registry(void)
{
    kbo_spin_unlock(&g_kbo_runtime_thread_registry_lock);
}

void kbo_request_runtime_threads_stop(void)
{
    InterlockedExchange(&g_kbo_runtime_threads_stop_requested, 1);
}

void kbo_register_runtime_thread(HANDLE thread, const char* label)
{
    const char* thread_label = label != NULL ? label : "runtime thread";
    if (thread == NULL) {
        return;
    }

    kbo_lock_runtime_thread_registry();
    for (int i = 0; i < KBO_RUNTIME_THREAD_MAX; ++i) {
        if (g_kbo_runtime_threads[i].handle != NULL
                && WaitForSingleObject(g_kbo_runtime_threads[i].handle, 0) == WAIT_OBJECT_0) {
            CloseHandle(g_kbo_runtime_threads[i].handle);
            g_kbo_runtime_threads[i].handle = NULL;
            g_kbo_runtime_threads[i].label[0] = '\0';
        }
    }

    for (int i = 0; i < KBO_RUNTIME_THREAD_MAX; ++i) {
        if (g_kbo_runtime_threads[i].handle == NULL) {
            g_kbo_runtime_threads[i].handle = thread;
            snprintf(g_kbo_runtime_threads[i].label, sizeof(g_kbo_runtime_threads[i].label), "%s", thread_label);
            kbo_unlock_runtime_thread_registry();
            if (kbo_should_log_runtime_thread_registration()) {
                append_logf("registered KBO runtime thread label=\"%s\" handle=%p", thread_label, thread);
            }
            return;
        }
    }
    kbo_unlock_runtime_thread_registry();

    append_logf("KBO runtime thread registry full; closing untracked thread label=\"%s\" handle=%p", thread_label, thread);
    CloseHandle(thread);
}

int kbo_start_runtime_thread(LPTHREAD_START_ROUTINE start, LPVOID parameter, const char* label)
{
    const char* thread_label = label != NULL ? label : "runtime thread";
    if (start == NULL) {
        append_logf("KBO runtime thread start skipped label=\"%s\" reason=no_start_proc", thread_label);
        return 0;
    }

    HANDLE thread = CreateThread(NULL, 0, start, parameter, 0, NULL);
    if (thread == NULL) {
        append_logf(
            "KBO runtime thread start failed label=\"%s\" error=%lu",
            thread_label,
            (unsigned long)GetLastError());
        return 0;
    }

    kbo_register_runtime_thread(thread, thread_label);
    return 1;
}

void kbo_shutdown_runtime_threads(uint32_t timeout_ms)
{
    HANDLE handles[KBO_RUNTIME_THREAD_MAX];
    char labels[KBO_RUNTIME_THREAD_MAX][64];
    DWORD count = 0;
    DWORD deadline = GetTickCount() + timeout_ms;

    kbo_request_runtime_threads_stop();

    kbo_lock_runtime_thread_registry();
    for (int i = 0; i < KBO_RUNTIME_THREAD_MAX; ++i) {
        if (g_kbo_runtime_threads[i].handle != NULL) {
            handles[count] = g_kbo_runtime_threads[i].handle;
            snprintf(labels[count], sizeof(labels[count]), "%s", g_kbo_runtime_threads[i].label);
            g_kbo_runtime_threads[i].handle = NULL;
            g_kbo_runtime_threads[i].label[0] = '\0';
            ++count;
        }
    }
    kbo_unlock_runtime_thread_registry();

    if (count == 0) {
        append_log_line("KBO runtime shutdown: no registered threads");
        return;
    }

    append_logf("KBO runtime shutdown: waiting for %lu thread(s)", (unsigned long)count);
    for (DWORD i = 0; i < count; ++i) {
        if (GetThreadId(handles[i]) == GetCurrentThreadId()) {
            append_logf("KBO runtime shutdown: skipping current thread label=\"%s\"", labels[i]);
            CloseHandle(handles[i]);
            continue;
        }

        DWORD now = GetTickCount();
        DWORD remaining = now >= deadline ? 0u : deadline - now;
        DWORD wait = WaitForSingleObject(handles[i], remaining);
        if (wait == WAIT_OBJECT_0) {
            append_logf("KBO runtime shutdown: joined label=\"%s\"", labels[i]);
        } else {
            append_logf(
                "KBO runtime shutdown: join timeout/failure label=\"%s\" wait=0x%08lX",
                labels[i],
                (unsigned long)wait);
        }
        CloseHandle(handles[i]);
    }
}

int kbo_runtime_threads_should_continue(void)
{
    if (InterlockedCompareExchange(&g_kbo_runtime_threads_stop_requested, 0, 0) != 0) {
        return 0;
    }

    int configured = 1;
    if (kbo_read_localappdata_json_flag_value("enable_kbo_fix", "enable_kbo_fix.txt", &configured)
            && !configured) {
        return 0;
    }
    return 1;
}

int kbo_runtime_sleep_should_continue(uint32_t total_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < total_ms) {
        if (!kbo_runtime_threads_should_continue()) {
            return 0;
        }

        uint32_t step = total_ms - elapsed;
        if (step > 250u) {
            step = 250u;
        }
        Sleep(step);
        elapsed += step;
    }
    return kbo_runtime_threads_should_continue();
}
