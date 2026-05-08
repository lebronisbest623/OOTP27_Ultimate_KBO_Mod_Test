static int __attribute__((unused)) patch_kbo_salary_arbitration_bytes(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t size)
{
    if (exe == NULL || label == NULL || expected == NULL || patch == NULL || size == 0) {
        return 0;
    }

    uint8_t* target = (uint8_t*)((uintptr_t)exe + rva);
    if (!memory_range_readable(target, size)) {
        append_logf("%s target unreadable target=%p rva=0x%x", label, target, rva);
        return 0;
    }

    return patch_static_bytes(label, target, expected, patch, size);
}

static uint8_t* allocate_kbo_salary_arbitration_stub(const uint8_t* code, size_t size)
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
