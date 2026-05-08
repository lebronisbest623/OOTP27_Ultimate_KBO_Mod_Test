#include "arbitration_patch_helpers.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../core/core_current_date.h"
#include "../../core/core_save_paths.h"
#include "../../core/core_text_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"

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
