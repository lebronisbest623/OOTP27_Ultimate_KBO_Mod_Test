#include "hook_stubs_near_code.h"

uint8_t* kbo_alloc_near_code(void* target, size_t size)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t granularity = (uintptr_t)si.dwAllocationGranularity;
    uintptr_t target_addr = (uintptr_t)target;
    uintptr_t min_addr = target_addr > 0x70000000ull ? target_addr - 0x70000000ull : granularity;
    uintptr_t max_addr = target_addr + 0x70000000ull;

    for (uintptr_t address = target_addr; address >= min_addr + granularity; address -= granularity) {
        uintptr_t aligned = address & ~(granularity - 1u);
        uint8_t* memory = (uint8_t*)VirtualAlloc((void*)aligned, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (memory != NULL) {
            return memory;
        }
    }

    for (uintptr_t address = (target_addr + granularity) & ~(granularity - 1u); address < max_addr; address += granularity) {
        uint8_t* memory = (uint8_t*)VirtualAlloc((void*)address, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (memory != NULL) {
            return memory;
        }
    }

    return NULL;
}
