/* Core KBO league context resolution. */

static uintptr_t get_kbo_league_event_manager(void)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t event_manager = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    if (event_manager == 0 || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    return event_manager;
}

static uint32_t kbo_read_kbo_league_id_override(void)
{
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        append_logf("KBO league id override: LOCALAPPDATA missing or invalid=%lu", got);
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\kbo_league_id.txt", local_app_data);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO league id override file missing=%s", path);
        return 0;
    }

    char raw[64] = {0};
    DWORD read = 0;
    ReadFile(file, raw, sizeof(raw) - 1, &read, NULL);
    CloseHandle(file);
    if (read == 0) {
        append_logf("KBO league id override read failed: empty file=%s", path);
        return 0;
    }

    raw[sizeof(raw) - 1] = '\0';

    uint32_t value = 0;
    size_t idx = 0;
    if (read >= 3u && (unsigned char)raw[0] == 0xEFu && (unsigned char)raw[1] == 0xBBu && (unsigned char)raw[2] == 0xBFu) {
        idx = 3u;
    }

    if (idx + 2u < (size_t)read && (unsigned char)raw[idx] == 0xFFu && (unsigned char)raw[idx + 1u] == 0xFEu) {
        idx += 2u;
        while (idx + 1u < (size_t)read) {
            uint16_t ch = (uint16_t)((unsigned char)raw[idx]) | ((uint16_t)(unsigned char)raw[idx + 1u] << 8);
            if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t') {
                idx += 2u;
                continue;
            }
            if (ch >= '0' && ch <= '9') {
                value = (uint32_t)(value * 10u + (unsigned)(ch - '0'));
                idx += 2u;
                while (idx + 1u < (size_t)read) {
                    ch = (uint16_t)((unsigned char)raw[idx]) | ((uint16_t)(unsigned char)raw[idx + 1u] << 8);
                    if (ch >= '0' && ch <= '9') {
                        value = (uint32_t)(value * 10u + (unsigned)(ch - '0'));
                        idx += 2u;
                        continue;
                    }
                    break;
                }
            }
            break;
        }
        if (value == 0) {
            append_logf("KBO league id override parse failed (utf16) file=%s read=%lu", path, read);
            return 0;
        }
        append_logf("KBO league id override file=%s utf16 value=%u", path, value);
        return value;
    }

    while (idx < (size_t)read && (raw[idx] == '\r' || raw[idx] == '\n' || raw[idx] == ' ' || raw[idx] == '\t')) {
        ++idx;
    }
    if (idx >= (size_t)read) {
        append_logf("KBO league id override parse failed: no body in file=%s", path);
        return 0;
    }

    while (idx < (size_t)read && raw[idx] >= '0' && raw[idx] <= '9') {
        value = (uint32_t)(value * 10u + (unsigned)(raw[idx] - '0'));
        ++idx;
    }
    if (value == 0) {
        append_logf("KBO league id override parse failed file=%s", path);
        return 0;
    }

    append_logf("KBO league id override file=%s value=%u", path, value);
    return value;
}
static uint32_t kbo_find_current_kbo_league_year(void)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    return kbo_current_date_is_valid(&year, &month, &day) ? year : 0;
}

static int kbo_league_candidate_matches_id(uintptr_t candidate, uint32_t league_id, uint32_t id_offset)
{
    if (candidate == 0 || league_id == 0
            || !memory_range_readable((void*)candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
            || !memory_range_readable((void*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint32_t candidate_id = *(uint32_t*)(candidate + id_offset);
    if (candidate_id != league_id) {
        return 0;
    }

    uint32_t league_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    if (league_year < 1982u || league_year > 2200u) {
        return 0;
    }
    uint32_t current_year = kbo_find_current_kbo_league_year();
    if (current_year >= 1982u && current_year <= 2200u
            && (league_year + 1u < current_year || league_year > current_year + 1u)) {
        return 0;
    }

    uint8_t phase = *(uint8_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET);
    if (phase > 4u) {
        return 0;
    }

    uint32_t phase_year = *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET);
    if (phase_year != 0u && (phase_year < 1982u || phase_year > 2200u)) {
        return 0;
    }

    return 1;
}

static int kbo_league_scan_protect_allows_read(DWORD protect)
{
    if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0) {
        return 0;
    }
    protect &= 0xffu;
    return protect == PAGE_READONLY
        || protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY;
}

static uintptr_t kbo_find_league_ptr_by_memory_scan(uint32_t league_id)
{
    static uintptr_t cached_ptr = 0;
    static uint32_t cached_league_id = 0;
    static uint32_t cached_id_offset = 0;
    static ULONGLONG last_scan_ms = 0;

    if (cached_ptr != 0 && cached_league_id == league_id
            && kbo_league_candidate_matches_id(cached_ptr, league_id, cached_id_offset)) {
        return cached_ptr;
    }
    cached_ptr = 0;
    cached_league_id = 0;
    cached_id_offset = 0;

    ULONGLONG now = GetTickCount64();
    if (now - last_scan_ms < 10000u) {
        return 0;
    }
    last_scan_ms = now;
    append_logf("KBO league ptr memory scan started league_id=%u", league_id);

    static const uint32_t id_offsets[] = {
        OOTP27_KBO_LEAGUE_ID_OFFSET,
        OOTP27_KBO_LEAGUE_ID_OFFSET + 8u
    };

    uintptr_t address = 0x10000u;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)address, &mbi, sizeof(mbi)) != 0) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = base + mbi.RegionSize;
        if (end <= base) {
            break;
        }

        if (mbi.State == MEM_COMMIT
                && mbi.Type == MEM_PRIVATE
                && kbo_league_scan_protect_allows_read(mbi.Protect)
                && mbi.RegionSize >= (SIZE_T)(OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
                && mbi.RegionSize <= (SIZE_T)0x04000000u) {
            uintptr_t scan_end = end >= sizeof(uint32_t) ? end - sizeof(uint32_t) : base;
            for (uintptr_t p = base; p <= scan_end; p += sizeof(uint32_t)) {
                uint32_t probe = 0;
                SIZE_T bytes_read = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)p, &probe, sizeof(probe), &bytes_read)
                        || bytes_read != sizeof(probe)
                        || probe != league_id) {
                    continue;
                }

                for (size_t i = 0; i < sizeof(id_offsets) / sizeof(id_offsets[0]); i++) {
                    uint32_t id_offset = id_offsets[i];
                    if (p < id_offset) {
                        continue;
                    }

                    uintptr_t candidate = p - id_offset;
                    if (!kbo_league_candidate_matches_id(candidate, league_id, id_offset)) {
                        continue;
                    }

                    cached_ptr = candidate;
                    cached_league_id = league_id;
                    cached_id_offset = id_offset;
                    append_logf(
                        "KBO league ptr found by memory scan league_id=%u ptr=%p id_offset=0x%x league_year=%u phase=%u phase_year=%u",
                        league_id,
                        (void*)candidate,
                        id_offset,
                        *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_YEAR_OFFSET),
                        (unsigned)*(uint8_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_OFFSET),
                        *(uint32_t*)(candidate + OOTP27_KBO_LEAGUE_PHASE_YEAR_OFFSET));
                    return candidate;
                }
            }
        }

        address = end;
        if (address >= (uintptr_t)0x0000800000000000ull) {
            break;
        }
    }

    append_logf("KBO league ptr memory scan missed league_id=%u", league_id);
    return 0;
}

static uintptr_t kbo_find_league_ptr_from_global_vectors(uint32_t league_id)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0 || league_id == 0) {
        return 0;
    }

    static const uint32_t league_vec_offsets[] = {
        0xa0u, 0xa8u, 0xb0u, 0xb8u, 0xc0u, 0xc8u, 0xd0u, 0xd8u,
        0xe0u, 0xe8u, 0xf0u, 0xf8u, 0x100u, 0x108u, 0x110u, 0x118u, 0x120u, 0x128u
    };

    for (size_t i = 0; i < sizeof(league_vec_offsets) / sizeof(league_vec_offsets[0]); i++) {
        uint32_t vec_off = league_vec_offsets[i];
        uint32_t cnt_off = vec_off + 8u;
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }

        uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
        int32_t candidate_count = *(int32_t*)(global + cnt_off);
        if (candidate_vec == 0 || candidate_count <= 0 || candidate_count > 10000
                || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t league_candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (league_candidate == 0 || !memory_range_readable((void*)league_candidate, OOTP27_KBO_LEAGUE_ID_OFFSET + 12u)) {
                continue;
            }
            uint32_t legacy_id = *(uint32_t*)(league_candidate + OOTP27_KBO_LEAGUE_ID_OFFSET);
            uint32_t alternate_id = *(uint32_t*)(league_candidate + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
            if (legacy_id != league_id && alternate_id != league_id) {
                continue;
            }
            if (!kbo_league_candidate_matches_id(league_candidate, league_id,
                    legacy_id == league_id ? OOTP27_KBO_LEAGUE_ID_OFFSET : OOTP27_KBO_LEAGUE_ID_OFFSET + 8u)) {
                continue;
            }
            return league_candidate;
        }
    }

    return 0;
}

static uintptr_t kbo_find_league_ptr_from_id(uint32_t league_id)
{
    uintptr_t league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
    if (league_ptr != 0) {
        return league_ptr;
    }

    return kbo_find_league_ptr_by_memory_scan(league_id);
}

static uint32_t kbo_find_league_year_from_id(uint32_t league_id)
{
    uintptr_t league_ptr = kbo_find_league_ptr_from_id(league_id);
    if (league_ptr == 0
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    return *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
}

static uint32_t kbo_find_league_year_from_id_no_scan(uint32_t league_id)
{
    uintptr_t league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
    if (league_ptr == 0
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    return *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
}

static uint32_t kbo_resolve_kbo_league_id(void)
{
    static uint32_t cached_league_id = 0;
    uint32_t override = kbo_read_kbo_league_id_override();
    if (override != 0) {
        if (cached_league_id == 0 || cached_league_id != override) {
            append_logf("KBO league id override used override=%u cached=%u", override, cached_league_id);
        }
        cached_league_id = override;
        append_logf("KBO league id resolved by override=%u", cached_league_id);
        return cached_league_id;
    }

    if (cached_league_id != 0) {
        return cached_league_id;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    uint32_t league_ids[128] = {0};
    int32_t counts[128] = {0};
    int32_t seen = 0;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        if (((uint8_t*)team_ptr)[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t league_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (league_id == 0 || league_id > 2000000u) {
            continue;
        }

        int index = -1;
        for (int j = 0; j < seen; j++) {
            if (league_ids[j] == league_id) {
                index = j;
                break;
            }
        }
        if (index < 0) {
            if (seen >= (int32_t)(sizeof(league_ids) / sizeof(league_ids[0]))) {
                continue;
            }
            league_ids[seen] = league_id;
            counts[seen] = 0;
            index = seen;
            seen++;
        }
        counts[index]++;
    }

    uint32_t current_year = kbo_find_current_kbo_league_year();
    uint32_t best_league_id = 0;
    int best_count = -1;
    for (int i = 0; i < seen; i++) {
        if (counts[i] <= 0) {
            continue;
        }

        uint32_t league_year = 0;
        if (current_year != 0) {
            league_year = kbo_find_league_year_from_id(league_ids[i]);
        }
        int score = counts[i];
        if (current_year != 0 && league_year == current_year) {
            score += 400;
        }
        if (score > best_count) {
            best_count = score;
            best_league_id = league_ids[i];
        }
    }

    if (best_league_id == 0 && seen > 0) {
        int fallback_count = counts[0];
        best_league_id = league_ids[0];
        for (int i = 1; i < seen; i++) {
            if (counts[i] > fallback_count) {
                fallback_count = counts[i];
                best_league_id = league_ids[i];
            }
        }
    }

    if (best_league_id == 0) {
        best_league_id = OOTP27_KBO_MAIN_LEAGUE_ID;
    }

    cached_league_id = best_league_id;
    append_logf("KBO league id resolved current_year=%u team_count=%d selected=%u",
        current_year, team_count, best_league_id);
    return best_league_id;
}
