#ifndef KBO_CORE_ATOMIC_FILE_H
#define KBO_CORE_ATOMIC_FILE_H

#include <stddef.h>
#include <windows.h>

HANDLE kbo_atomic_open_tmp(const char* dest_path, char* out_tmp, size_t tmp_size);
int kbo_atomic_commit(HANDLE file, const char* tmp_path, const char* dest_path);
void kbo_atomic_abort(HANDLE file, const char* tmp_path);

#endif
