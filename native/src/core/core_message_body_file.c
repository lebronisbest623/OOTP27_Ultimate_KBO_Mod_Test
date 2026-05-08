#include "core_message_body_file.h"
#include <stdio.h>
#include <string.h>
#include "../bootstrap/ootp_offsets.h"
#include "core_log.h"
#include "core_current_date.h"
#include "core_save_paths.h"
#include "core_text_date.h"
#include "core_flags/flags_api.h"
#include "../runtime_memory/runtime_memory.h"

/* Core message body file persistence. */

int write_kbo_message_body_file(uint32_t message_id, const char* title, const char* body, const char* source)
{
    if (message_id == 0 || title == NULL || title[0] == '\0') {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        append_logf("league news body file skipped source=%s title=%s reason=no_save_path", source != NULL ? source : "", title);
        return 0;
    }

    char message_dir[MAX_PATH] = {0};
    snprintf(message_dir, sizeof(message_dir), "%s\\messages", save_path);
    CreateDirectoryA(message_dir, NULL);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\message%u.txt", message_dir, message_id);

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf(
            "league news body file skipped source=%s title=%s id=%u reason=create_failed gle=%lu path=%s",
            source != NULL ? source : "",
            title,
            message_id,
            GetLastError(),
            path);
        return 0;
    }

    char text[4096] = {0};
    int len = snprintf(text, sizeof(text), "%s\r\n%s\r\n", title, body != NULL ? body : "");
    DWORD written = 0;
    BOOL ok = FALSE;
    if (len > 0) {
        ok = WriteFile(file, text, (DWORD)len, &written, NULL);
    }
    CloseHandle(file);

    append_logf(
        "league news body file write source=%s title=%s id=%u ok=%u bytes=%lu path=%s",
        source != NULL ? source : "",
        title,
        message_id,
        ok ? 1u : 0u,
        written,
        path);
    return ok && written > 0;
}
