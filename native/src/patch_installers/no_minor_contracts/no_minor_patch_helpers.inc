static int patch_kbo_no_minor_contract_write_site(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t size)
{
    uint8_t* target = (uint8_t*)((uintptr_t)exe + rva);
    if (!memory_range_readable(target, size)) {
        append_logf("%s target unreadable target=%p rva=0x%x", label, target, rva);
        return 0;
    }
    return patch_static_bytes(label, target, expected, patch, size);
}

