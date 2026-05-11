#ifndef KBOFIX_SRC_CORE_OPTIMIZER_KBO_OPTIMIZER_H_
#define KBOFIX_SRC_CORE_OPTIMIZER_KBO_OPTIMIZER_H_

#include <stddef.h>
#include <windows.h>

int kbo_optimizer_run_mode(
    const char* mode,
    const char* request_path,
    const char* result_path,
    DWORD timeout_ms);

#endif
