/* Military service seed resolution from players.dat. Included from native/src/military_service/military_service_seed_store.inc. */

static uint32_t kbo_resolve_military_service_seed_key_from_players_dat(const char* key)
{
    if (key == NULL || key[0] == '\0' || (key[0] >= '0' && key[0] <= '9')) {
        return 0u;
    }

    char players_dat_path[MAX_PATH] = {0};
    if (!kbo_get_current_players_dat_path_for_military_seed(players_dat_path, sizeof(players_dat_path))) {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat missing or save not written", key);
        return 0u;
    }

    HANDLE file = CreateFileA(players_dat_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat open failed gle=%lu", key, (unsigned long)GetLastError());
        return 0u;
    }

    LARGE_INTEGER file_size;
    file_size.QuadPart = 0;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 || file_size.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(file);
        append_logf("KBO military service seed unresolved key=%s reason=players.dat size unsupported", key);
        return 0u;
    }

    uint8_t* raw = (uint8_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)file_size.QuadPart);
    if (raw == NULL) {
        CloseHandle(file);
        return 0u;
    }

    DWORD read = 0;
    int read_ok = ReadFile(file, raw, (DWORD)file_size.QuadPart, &read, NULL) && read == (DWORD)file_size.QuadPart;
    CloseHandle(file);
    if (!read_ok) {
        HeapFree(GetProcessHeap(), 0, raw);
        append_logf("KBO military service seed unresolved key=%s reason=players.dat read failed", key);
        return 0u;
    }

    size_t key_len = strlen(key);
    uint32_t matched_player_id = 0u;
    int ambiguous = 0;
    for (size_t pos = 0; pos + key_len <= (size_t)read; pos++) {
        if (memcmp(raw + pos, key, key_len) != 0) {
            continue;
        }
        char before = pos > 0u ? (char)raw[pos - 1u] : '\0';
        char after = (char)raw[pos + key_len];
        if (kbo_military_ascii_is_seed_id_char(before) || kbo_military_ascii_is_seed_id_char(after)) {
            continue;
        }

        uint32_t record_player_id = kbo_military_resolve_player_id_from_players_dat_record_start(raw, (size_t)read, pos);
        if (record_player_id != 0u) {
            if (matched_player_id != 0u && matched_player_id != record_player_id) {
                ambiguous = 1;
                break;
            }
            matched_player_id = record_player_id;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    if (ambiguous) {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat ambiguous", key);
        return 0u;
    }
    if (matched_player_id != 0u) {
        append_logf("KBO military service seed resolved via players.dat key=%s player=%u", key, matched_player_id);
    } else {
        append_logf("KBO military service seed unresolved key=%s reason=players.dat no matching player record header", key);
    }
    return matched_player_id;
}

