#include "..\amateur_assignment_ortools_internal.h"

static PROCESS_INFORMATION g_kbo_amateur_ortools_worker_pi;
static HANDLE g_kbo_amateur_ortools_worker_stdin = NULL;
static HANDLE g_kbo_amateur_ortools_worker_stdout = NULL;
static LONG g_kbo_amateur_ortools_worker_lock = 0;

static void kbo_amateur_ortools_close_worker(void)
{
    if (g_kbo_amateur_ortools_worker_stdin != NULL) {
        CloseHandle(g_kbo_amateur_ortools_worker_stdin);
        g_kbo_amateur_ortools_worker_stdin = NULL;
    }
    if (g_kbo_amateur_ortools_worker_stdout != NULL) {
        CloseHandle(g_kbo_amateur_ortools_worker_stdout);
        g_kbo_amateur_ortools_worker_stdout = NULL;
    }
    if (g_kbo_amateur_ortools_worker_pi.hProcess != NULL) {
        TerminateProcess(g_kbo_amateur_ortools_worker_pi.hProcess, 1);
        CloseHandle(g_kbo_amateur_ortools_worker_pi.hProcess);
        g_kbo_amateur_ortools_worker_pi.hProcess = NULL;
    }
    if (g_kbo_amateur_ortools_worker_pi.hThread != NULL) {
        CloseHandle(g_kbo_amateur_ortools_worker_pi.hThread);
        g_kbo_amateur_ortools_worker_pi.hThread = NULL;
    }
}

static int kbo_amateur_ortools_worker_running(void)
{
    if (g_kbo_amateur_ortools_worker_pi.hProcess == NULL
            || g_kbo_amateur_ortools_worker_stdin == NULL
            || g_kbo_amateur_ortools_worker_stdout == NULL) {
        return 0;
    }
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(g_kbo_amateur_ortools_worker_pi.hProcess, &exit_code)) {
        return 0;
    }
    return exit_code == STILL_ACTIVE;
}

static int kbo_amateur_ortools_start_worker(const char* tool_path)
{
    if (kbo_amateur_ortools_worker_running()) {
        return 1;
    }
    kbo_amateur_ortools_close_worker();

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdin_read = NULL;
    HANDLE stdin_write = NULL;
    HANDLE stdout_read = NULL;
    HANDLE stdout_write = NULL;
    HANDLE stderr_nul = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        return 0;
    }
    if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return 0;
    }
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return 0;
    }
    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        return 0;
    }

    char command[MAX_PATH * 4] = {0};
    snprintf(command, sizeof(command), "\"%s\" --server", tool_path);

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    memset(&g_kbo_amateur_ortools_worker_pi, 0, sizeof(g_kbo_amateur_ortools_worker_pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    stderr_nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    si.hStdError = stderr_nul != INVALID_HANDLE_VALUE ? stderr_nul : stdout_write;
    si.wShowWindow = SW_HIDE;

    int ok = CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_kbo_amateur_ortools_worker_pi);
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    if (stderr_nul != INVALID_HANDLE_VALUE) {
        CloseHandle(stderr_nul);
    }
    if (!ok) {
        static volatile LONG create_fail_count = 0;
        if (InterlockedIncrement(&create_fail_count) <= 5) {
            kbo_log_runtimef("amateur OR-Tools worker launch failed gle=%lu tool=%s", GetLastError(), tool_path);
        }
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        return 0;
    }

    g_kbo_amateur_ortools_worker_stdin = stdin_write;
    g_kbo_amateur_ortools_worker_stdout = stdout_read;
    kbo_log_runtimef("amateur OR-Tools worker started tool=%s", tool_path);
    return 1;
}

int kbo_amateur_ortools_run_worker(const char* tool_path, const char* request_path, const char* result_path)
{
    while (InterlockedCompareExchange(&g_kbo_amateur_ortools_worker_lock, 1, 0) != 0) {
        Sleep(0);
    }

    int ok = 0;
    do {
        DeleteFileA(result_path);
        if (!kbo_amateur_ortools_start_worker(tool_path)) {
            break;
        }

        for (int attempt = 0; attempt < 2 && !ok; attempt++) {
            char line[MAX_PATH * 6] = {0};
            snprintf(line, sizeof(line), "%s\t%s\n", request_path, result_path);
            DWORD written = 0;
            if (!WriteFile(g_kbo_amateur_ortools_worker_stdin, line, (DWORD)strlen(line), &written, NULL)) {
                kbo_amateur_ortools_close_worker();
                break;
            }
            FlushFileBuffers(g_kbo_amateur_ortools_worker_stdin);

            char response[128] = {0};
            DWORD used = 0;
            DWORD start = GetTickCount();
            while (used + 1 < sizeof(response)) {
                DWORD available = 0;
                if (!PeekNamedPipe(g_kbo_amateur_ortools_worker_stdout, NULL, 0, NULL, &available, NULL)) {
                    kbo_amateur_ortools_close_worker();
                    break;
                }
                if (available == 0) {
                    if (GetTickCount() - start > 5000u) {
                        kbo_log_runtime_line("amateur OR-Tools worker timed out");
                        kbo_amateur_ortools_close_worker();
                        break;
                    }
                    Sleep(1);
                    continue;
                }
                char ch = '\0';
                DWORD read = 0;
                if (!ReadFile(g_kbo_amateur_ortools_worker_stdout, &ch, 1, &read, NULL) || read != 1) {
                    kbo_amateur_ortools_close_worker();
                    break;
                }
                if (ch == '\n') {
                    ok = strncmp(response, "OK 0", 4) == 0 && GetFileAttributesA(result_path) != INVALID_FILE_ATTRIBUTES;
                    break;
                }
                if (ch != '\r') {
                    response[used++] = ch;
                }
            }
            if (!ok && response[0] != '\0') {
                static volatile LONG fail_log_count = 0;
                if (InterlockedIncrement(&fail_log_count) <= 5) {
                    kbo_log_runtimef("amateur OR-Tools worker response=%s", response);
                }
            }
        }
    } while (0);

    InterlockedExchange(&g_kbo_amateur_ortools_worker_lock, 0);
    return ok;
}

int kbo_amateur_ortools_run(const char* tool_path, int is_python_script, const char* request_path, const char* result_path)
{
    DeleteFileA(result_path);

    if (!is_python_script) {
        return kbo_amateur_ortools_run_worker(tool_path, request_path, result_path);
    }

    char command[MAX_PATH * 9] = {0};
    snprintf(
        command,
        sizeof(command),
        "python \"%s\" \"%s\" \"%s\"",
        tool_path,
        request_path,
        result_path);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        static volatile LONG create_fail_count = 0;
        if (InterlockedIncrement(&create_fail_count) <= 5) {
            kbo_log_runtimef("amateur OR-Tools optimizer launch failed gle=%lu tool=%s", GetLastError(), tool_path);
        }
        return 0;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, 5000);
    DWORD exit_code = 1;
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        kbo_log_runtime_line("amateur OR-Tools optimizer timed out");
    } else {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return wait != WAIT_TIMEOUT && exit_code == 0 && GetFileAttributesA(result_path) != INVALID_FILE_ATTRIBUTES;
}

uint32_t kbo_amateur_ortools_read_result(const char* result_path)
{
    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0u;
    }
    char line[512] = {0};
    if (fgets(line, sizeof(line), file) == NULL || fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0u;
    }
    fclose(file);
    return (uint32_t)strtoul(line, NULL, 10);
}

int kbo_amateur_ortools_read_batch_result(const char* result_path, uint32_t league_id)
{
    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0;
    }

    char line[512] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    KboAmateurBatchAssignment assignments[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    int count = 0;
    while (count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX && fgets(line, sizeof(line), file) != NULL) {
        char* cursor = line;
        uint32_t player_id = (uint32_t)strtoul(cursor, &cursor, 10);
        if (*cursor == ',') {
            cursor++;
        }
        uint32_t target_team_id = (uint32_t)strtoul(cursor, NULL, 10);
        if (player_id != 0u && target_team_id != 0u) {
            assignments[count].player_id = player_id;
            assignments[count].league_id = league_id;
            assignments[count].target_team_id = target_team_id;
            count++;
        }
    }
    fclose(file);

    kbo_amateur_batch_lock();
    memset(g_kbo_amateur_batch_assignments, 0, sizeof(g_kbo_amateur_batch_assignments));
    memcpy(g_kbo_amateur_batch_assignments, assignments, (size_t)count * sizeof(assignments[0]));
    InterlockedExchange(&g_kbo_amateur_batch_assignment_count, count);
    kbo_amateur_batch_unlock();
    return count;
}

