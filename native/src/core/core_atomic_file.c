#include "core_atomic_file.h"

#include <stdio.h>

HANDLE kbo_atomic_open_tmp(const char* dest_path, char* out_tmp, size_t tmp_size)
{
    if (dest_path == NULL || out_tmp == NULL || tmp_size == 0) {
        return INVALID_HANDLE_VALUE;
    }

    snprintf(out_tmp, tmp_size, "%s.tmp", dest_path);
    HANDLE file = CreateFileA(out_tmp, GENERIC_WRITE, FILE_SHARE_READ,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        out_tmp[0] = '\0';
    }
    return file;
}

int kbo_atomic_commit(HANDLE file, const char* tmp_path, const char* dest_path)
{
    if (file == INVALID_HANDLE_VALUE || tmp_path == NULL
            || tmp_path[0] == '\0' || dest_path == NULL) {
        return 0;
    }

    CloseHandle(file);
    if (!MoveFileExA(tmp_path, dest_path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp_path);
        return 0;
    }
    return 1;
}
