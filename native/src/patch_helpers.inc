static void write_u64(uint8_t* dst, uint64_t value)
{
    for (int i = 0; i < 8; i++) {
        dst[i] = (uint8_t)((value >> (i * 8)) & 0xffu);
    }
}

static int is_r11_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0x49
        && target[1] == 0xBB
        && target[10] == 0x41
        && target[11] == 0xFF
        && target[12] == 0xE3;
}

static int is_rax_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0x48
        && target[1] == 0xB8
        && target[10] == 0xFF
        && target[11] == 0xE0;
}

static int is_rip_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0xFF
        && target[1] == 0x25
        && target[2] == 0x00
        && target[3] == 0x00
        && target[4] == 0x00
        && target[5] == 0x00;
}

static void log_patch_bytes_mismatch(const char* label, const uint8_t* target, size_t size)
{
    char bytes[128] = {0};
    size_t offset = 0;
    for (size_t i = 0; i < size && offset + 4 < sizeof(bytes); i++) {
        offset += (size_t)snprintf(bytes + offset, sizeof(bytes) - offset, "%s%02X", i == 0 ? "" : " ", target[i]);
    }
    append_logf("%s bytes mismatch at %p: %s", label, target, bytes);
}

static int patch_static_bytes(const char* label, uint8_t* target, const uint8_t* expected, const uint8_t* patch, size_t size)
{
    if (memcmp(target, patch, size) == 0) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }

    if (memcmp(target, expected, size) != 0) {
        log_patch_bytes_mismatch(label, target, size);
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);

    DWORD ignored = 0;
    VirtualProtect(target, size, old_protect, &ignored);

    append_logf("installed %s target=%p", label, target);
    return 1;
}
