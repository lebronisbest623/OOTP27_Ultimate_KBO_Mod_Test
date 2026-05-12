#include "cbt_service_time_probe_internal.h"

void kbo_cbt_probe_dump_candidate_rows(
    uint8_t* player,
    uint32_t player_id,
    uint32_t player_year,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id,
    const uint32_t* team_ids,
    int team_count,
    uint32_t vector_offset,
    uintptr_t begin,
    uintptr_t end)
{
    SIZE_T byte_count = (SIZE_T)(end - begin);
    if (byte_count < 16u || byte_count > 64u * 1024u || !memory_range_readable((void*)begin, byte_count < 256u ? byte_count : 256u)) {
        return;
    }

    static const uint32_t preferred_strides[] = { 16u, 24u, 28u, 32u, 40u, 48u, 56u, 64u };
    for (size_t stride_index = 0; stride_index < sizeof(preferred_strides) / sizeof(preferred_strides[0]); stride_index++) {
        uint32_t stride = preferred_strides[stride_index];
        if (byte_count < stride || (byte_count % stride) != 0u) {
            continue;
        }

        uint32_t entries = (uint32_t)(byte_count / stride);
        uint32_t dump_entries = entries < 5u ? entries : 5u;
        uint32_t inspect_entries = entries < 32u ? entries : 32u;
        uint32_t u16_year_hits = 0u;
        uint32_t u16_team_hits = 0u;
        uint32_t u16_kbo_team_hits = 0u;
        uint32_t u16_year_team_near_hits = 0u;
        uint32_t u32_year_hits = 0u;
        uint32_t u32_team_hits = 0u;
        uint32_t u32_kbo_team_hits = 0u;
        uint32_t u32_year_team_near_hits = 0u;
        uint32_t first_u16_year = 0u, first_u16_year_row = 0u, first_u16_year_off = 0u;
        uint32_t first_u16_kbo_team = 0u, first_u16_kbo_team_row = 0u, first_u16_kbo_team_off = 0u;

        for (uint32_t i = 0; i < inspect_entries; i++) {
            uintptr_t row = begin + ((uintptr_t)i * stride);
            if (!memory_range_readable((void*)row, stride)) {
                continue;
            }
            for (uint32_t off = 0u; off + sizeof(uint16_t) <= stride; off += 2u) {
                uint32_t value = (uint32_t)*(uint16_t*)(row + off);
                if (kbo_cbt_probe_year_plausible(value, player_year)) {
                    u16_year_hits++;
                    if (first_u16_year == 0u) {
                        first_u16_year = value;
                        first_u16_year_row = i;
                        first_u16_year_off = off;
                    }
                }
                if (kbo_cbt_probe_team_plausible(value, current_team_id, active_team_id, original_team_id)) {
                    u16_team_hits++;
                }
                if (kbo_cbt_probe_team_in_set(value, team_ids, team_count)) {
                    u16_kbo_team_hits++;
                    if (first_u16_kbo_team == 0u) {
                        first_u16_kbo_team = value;
                        first_u16_kbo_team_row = i;
                        first_u16_kbo_team_off = off;
                    }
                }
            }
            for (uint32_t off = 0u; off + sizeof(uint32_t) <= stride; off += 4u) {
                uint32_t value = *(uint32_t*)(row + off);
                if (kbo_cbt_probe_year_plausible(value, player_year)) {
                    u32_year_hits++;
                }
                if (kbo_cbt_probe_team_plausible(value, current_team_id, active_team_id, original_team_id)) {
                    u32_team_hits++;
                }
                if (kbo_cbt_probe_team_in_set(value, team_ids, team_count)) {
                    u32_kbo_team_hits++;
                }
            }

            for (uint32_t year_off = 0u; year_off + sizeof(uint16_t) <= stride; year_off += 2u) {
                uint32_t year_value = (uint32_t)*(uint16_t*)(row + year_off);
                if (!kbo_cbt_probe_year_plausible(year_value, player_year)) {
                    continue;
                }
                for (uint32_t team_off = 0u; team_off + sizeof(uint16_t) <= stride; team_off += 2u) {
                    if (team_off == year_off) {
                        continue;
                    }
                    uint32_t team_value = (uint32_t)*(uint16_t*)(row + team_off);
                    if (kbo_cbt_probe_team_in_set(team_value, team_ids, team_count)) {
                        u16_year_team_near_hits++;
                    }
                }
            }
            for (uint32_t year_off = 0u; year_off + sizeof(uint32_t) <= stride; year_off += 4u) {
                uint32_t year_value = *(uint32_t*)(row + year_off);
                if (!kbo_cbt_probe_year_plausible(year_value, player_year)) {
                    continue;
                }
                for (uint32_t team_off = 0u; team_off + sizeof(uint32_t) <= stride; team_off += 4u) {
                    if (team_off == year_off) {
                        continue;
                    }
                    uint32_t team_value = *(uint32_t*)(row + team_off);
                    if (kbo_cbt_probe_team_in_set(team_value, team_ids, team_count)) {
                        u32_year_team_near_hits++;
                    }
                }
            }
        }

        if (u16_year_team_near_hits == 0u && u32_year_team_near_hits == 0u) {
            continue;
        }

        append_logf(
            "CBT_SERVICE_PROBE: season_vector_candidate player=%u p=%p vector_off=0x%x bytes=%llu stride=%u entries=%u inspect=%u u16_year_hits=%u first_year=%u r%u@0x%x u16_kbo_team_hits=%u first_team=%u r%u@0x%x u16_year_team_near=%u u32_year_hits=%u u32_kbo_team_hits=%u u32_year_team_near=%u cur=%u active=%u original=%u year=%u",
            player_id,
            (void*)player,
            vector_offset,
            (unsigned long long)byte_count,
            stride,
            entries,
            inspect_entries,
            u16_year_hits,
            first_u16_year,
            first_u16_year_row,
            first_u16_year_off,
            u16_kbo_team_hits,
            first_u16_kbo_team,
            first_u16_kbo_team_row,
            first_u16_kbo_team_off,
            u16_year_team_near_hits,
            u32_year_hits,
            u32_kbo_team_hits,
            u32_year_team_near_hits,
            current_team_id,
            active_team_id,
            original_team_id,
            player_year);

        for (uint32_t i = 0; i < dump_entries; i++) {
            uintptr_t row = begin + ((uintptr_t)i * stride);
            char hex[256] = {0};
            kbo_cbt_probe_append_row_hex(hex, sizeof(hex), row, stride <= 48u ? stride : 48u);
            append_logf(
                "CBT_SERVICE_PROBE: season_vector_row player=%u vector_off=0x%x stride=%u row=%u hex=%s",
                player_id,
                vector_offset,
                stride,
                i,
                hex);
        }
    }
}


void kbo_cbt_probe_scan_player_inline_tables(
    uint8_t* player,
    uint32_t player_id,
    uint32_t player_year,
    const uint32_t* team_ids,
    int team_count,
    int* emitted_inline)
{
    if (player == NULL || emitted_inline == NULL || *emitted_inline >= 40) {
        return;
    }

    for (uint32_t off = 0x700u; off + 96u <= 0x980u && off + 96u <= OOTP27_PLAYER_SCAN_BYTES && *emitted_inline < 40; off += 8u) {
        if (!memory_range_readable(player + off, 64u)) {
            continue;
        }

        uint32_t year_hits = 0u;
        uint32_t kbo_team_hits = 0u;
        uint32_t year_team_near_hits = 0u;
        uint32_t first_year = 0u, first_year_off = 0u;
        uint32_t first_team = 0u, first_team_off = 0u;

        for (uint32_t inner = 0u; inner + sizeof(uint16_t) <= 64u; inner += 2u) {
            uint32_t value = (uint32_t)*(uint16_t*)(player + off + inner);
            if (kbo_cbt_probe_service_year_plausible(value, player_year)) {
                year_hits++;
                if (first_year == 0u) {
                    first_year = value;
                    first_year_off = off + inner;
                }
            }
            if (kbo_cbt_probe_team_in_set(value, team_ids, team_count)) {
                kbo_team_hits++;
                if (first_team == 0u) {
                    first_team = value;
                    first_team_off = off + inner;
                }
            }
        }

        for (uint32_t year_inner = 0u; year_inner + sizeof(uint16_t) <= 64u; year_inner += 2u) {
            uint32_t year_value = (uint32_t)*(uint16_t*)(player + off + year_inner);
            if (!kbo_cbt_probe_service_year_plausible(year_value, player_year)) {
                continue;
            }
            uint32_t lo = year_inner > 16u ? year_inner - 16u : 0u;
            uint32_t hi = year_inner + 18u <= 64u ? year_inner + 18u : 64u;
            for (uint32_t team_inner = lo; team_inner + sizeof(uint16_t) <= hi; team_inner += 2u) {
                if (team_inner == year_inner) {
                    continue;
                }
                uint32_t team_value = (uint32_t)*(uint16_t*)(player + off + team_inner);
                if (kbo_cbt_probe_team_in_set(team_value, team_ids, team_count)) {
                    year_team_near_hits++;
                }
            }
        }

        if (year_hits > 1u && kbo_team_hits > 0u && year_team_near_hits > 0u) {
            char hex[256] = {0};
            kbo_cbt_probe_append_row_hex(hex, sizeof(hex), (uintptr_t)(player + off), 64u);
            append_logf(
                "CBT_SERVICE_PROBE: inline_candidate player=%u p=%p off=0x%x bytes=64 u16_year_hits=%u first_year=%u@0x%x u16_kbo_team_hits=%u first_team=%u@0x%x u16_year_team_near=%u hex=%s",
                player_id,
                (void*)player,
                off,
                year_hits,
                first_year,
                first_year_off,
                kbo_team_hits,
                first_team,
                first_team_off,
                year_team_near_hits,
                hex);
            (*emitted_inline)++;
        }
    }
}

