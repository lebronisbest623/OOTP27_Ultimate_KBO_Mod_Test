#include "../internal/intl_established_fa_postscan_internal.h"

void kbo_intl_established_fa_postscan_try_run(void)
{
    if (InterlockedCompareExchange(&g_kbo_intl_established_fa_postscan.pending, 2, 1) != 1) {
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (now < g_kbo_intl_established_fa_postscan.due_tick) {
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);
        return;
    }

    KboIntlEstablishedFaPostscanState batch = g_kbo_intl_established_fa_postscan;
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int vector_ready = find_kbo_global_player_vector(&player_vector, &player_count, NULL);
    if (!vector_ready && batch.attempts < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);
        append_logf(
            "international established FA postscan retry batch=%ld attempt=%d reason=no_player_vector",
            batch.batch_id,
            batch.attempts + 1);
        return;
    }

    if (vector_ready
            && batch.before_count > 0
            && batch.expected_count > 0
            && player_count < batch.before_count + batch.expected_count
            && batch.attempts < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 1);
        append_logf(
            "international established FA postscan retry batch=%ld attempt=%d reason=waiting_for_players before=%d after=%d expected=%d",
            batch.batch_id,
            batch.attempts + 1,
            batch.before_count,
            player_count,
            batch.expected_count);
        return;
    }

    kbo_intl_established_fa_postscan_run(&batch);
    InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, 0);
}

DWORD WINAPI kbo_intl_established_fa_postscan_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("international established FA postscan worker started");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(1000)) {
            break;
        }
        kbo_intl_established_fa_postscan_try_run();
    }
    InterlockedExchange(&g_kbo_intl_established_fa_postscan_worker_started, 0);
    append_log_line("international established FA postscan worker stopped");
    return 0;
}

void start_kbo_intl_established_fa_postscan_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_intl_established_fa_postscan_worker_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_intl_established_fa_postscan_thread, NULL, 0, NULL);
    if (thread != NULL) {
        kbo_register_runtime_thread(thread, "intl established FA postscan");
    } else {
        InterlockedExchange(&g_kbo_intl_established_fa_postscan_worker_started, 0);
        append_logf("international established FA postscan worker failed to start error=%lu", GetLastError());
    }
}

