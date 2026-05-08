static int patch_kbo_no_minor_contract_scan_byte(uint8_t* imm_ptr)
{
    if (imm_ptr == NULL || !memory_range_readable(imm_ptr, 1)) {
        return 0;
    }
    if (*imm_ptr == 1u) {
        return 0;
    }
    if (*imm_ptr != 0u && *imm_ptr != 2u && *imm_ptr != 3u) {
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(imm_ptr, 1, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("KBO no-minor-contract scan patch VirtualProtect failed target=%p error=%lu", imm_ptr, GetLastError());
        return 0;
    }

    uint8_t old_value = *imm_ptr;
    *imm_ptr = 1u;
    FlushInstructionCache(GetCurrentProcess(), imm_ptr, 1);

    DWORD ignored = 0;
    VirtualProtect(imm_ptr, 1, old_protect, &ignored);
    append_logf(
        "installed KBO no-minor-contract scan patch imm target=%p old=%u new=1",
        imm_ptr,
        (unsigned)old_value);
    return 1;
}

static int install_kbo_no_minor_contract_scan_patch(HMODULE exe)
{
    if (exe == NULL) {
        return 0;
    }

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_log_line("KBO no-minor-contract scan patch skipped: invalid DOS header");
        return 0;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        append_log_line("KBO no-minor-contract scan patch skipped: invalid NT header");
        return 0;
    }

    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    int patched = 0;
    int scanned_sites = 0;
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }
        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD size = sections[s].Misc.VirtualSize;
        if (size < 12u || !memory_range_readable(section, size)) {
            continue;
        }
        for (DWORD off = 0; off + 8u < size; off++) {
            uint8_t* p = section + off;
            if (p[0] != 0xC6u) {
                continue;
            }
            /* C6 /0 disp32 imm8: mov byte ptr [reg+0x8a8], imm8 */
            if (p[2] == 0xA8u && p[3] == 0x08u && p[4] == 0x00u && p[5] == 0x00u) {
                uint8_t value = p[6];
                if (value == 0u || value == 2u || value == 3u) {
                    scanned_sites++;
                    patched += patch_kbo_no_minor_contract_scan_byte(p + 6);
                }
            }
            /* C6 /0 SIB disp32 imm8: mov byte ptr [base+index+0x8a8], imm8 */
            if (p[3] == 0xA8u && p[4] == 0x08u && p[5] == 0x00u && p[6] == 0x00u) {
                uint8_t value = p[7];
                if (value == 0u || value == 2u || value == 3u) {
                    scanned_sites++;
                    patched += patch_kbo_no_minor_contract_scan_byte(p + 7);
                }
            }
        }
    }

    append_logf(
        "KBO no-minor-contract scan patch complete scanned_sites=%d patched=%d",
        scanned_sites,
        patched);
    return patched > 0;
}

