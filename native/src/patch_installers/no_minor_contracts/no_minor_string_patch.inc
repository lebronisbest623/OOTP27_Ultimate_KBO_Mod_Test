static int patch_kbo_no_minor_contract_string_at(uint8_t* target, const char* from, const char* to, size_t len)
{
    if (target == NULL || from == NULL || to == NULL || len == 0 || !memory_range_readable(target, len)) {
        return 0;
    }
    if (memcmp(target, from, len) != 0) {
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, len, PAGE_READWRITE, &old_protect)) {
        append_logf("KBO no-minor-contract string patch VirtualProtect failed target=%p error=%lu", target, GetLastError());
        return 0;
    }

    memcpy(target, to, len);
    FlushInstructionCache(GetCurrentProcess(), target, len);

    DWORD ignored = 0;
    VirtualProtect(target, len, old_protect, &ignored);
    append_logf("installed KBO no-minor-contract string patch target=%p text=%.*s", target, (int)len, to);
    return 1;
}

static int install_kbo_no_minor_contract_string_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    const char from_title[] = "Minor League Contract";
    const char to_title[] = "Major League Contract";
    const char from_sentence[] = "Minor League contract";
    const char to_sentence[] = "Major League contract";
    const char from_lower[] = "minor league contract";
    const char to_lower[] = "major league contract";
    const char from_option[] = "Minor League with Major League Option";
    const char to_option[] = "Major League Contract                ";
    const size_t max_patch_len = sizeof(from_option) - 1u;

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_log_line("KBO no-minor-contract string patch skipped: invalid DOS header");
        return 0;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        append_log_line("KBO no-minor-contract string patch skipped: invalid NT header");
        return 0;
    }

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    int patched = 0;
    int scanned = 0;
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_READ)) {
            continue;
        }
        if (chars & IMAGE_SCN_MEM_EXECUTE) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD size = sections[s].Misc.VirtualSize;
        if (size < max_patch_len || !memory_range_readable(section, size)) {
            continue;
        }

        for (DWORD off = 0; off + max_patch_len < size; off++) {
            uint8_t* p = section + off;
            if (memcmp(p, from_title, sizeof(from_title) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_title,
                    to_title,
                    sizeof(from_title) - 1u);
            } else if (memcmp(p, from_sentence, sizeof(from_sentence) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_sentence,
                    to_sentence,
                    sizeof(from_sentence) - 1u);
            } else if (memcmp(p, from_lower, sizeof(from_lower) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_lower,
                    to_lower,
                    sizeof(from_lower) - 1u);
            } else if (memcmp(p, from_option, sizeof(from_option) - 1u) == 0) {
                scanned++;
                patched += patch_kbo_no_minor_contract_string_at(
                    p,
                    from_option,
                    to_option,
                    sizeof(from_option) - 1u);
            }
        }
    }

    append_logf(
        "KBO no-minor-contract string patch complete scanned_sites=%d patched=%d",
        scanned,
        patched);
    return patched > 0;
}
