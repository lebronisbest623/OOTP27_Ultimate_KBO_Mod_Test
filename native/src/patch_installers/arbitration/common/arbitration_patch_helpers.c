#include "arbitration_patch_helpers.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"

uint8_t* allocate_kbo_salary_arbitration_stub(const uint8_t* code, size_t size)
{
    if (code == NULL || size == 0) {
        return NULL;
    }

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }

    memcpy(memory, code, size);
    FlushInstructionCache(GetCurrentProcess(), memory, size);
    return memory;
}
