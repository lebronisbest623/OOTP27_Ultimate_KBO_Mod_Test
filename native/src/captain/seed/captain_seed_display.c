#include "../internal/captain_selection_internal.h"
#include "../../core/sync/spin_lock.h"

typedef struct KboCaptainDisplayCache {
    uint32_t season;
    uint32_t league_id;
    uint32_t team_id;
    uint32_t player_id;
    int found;
    char loaded_key[MAX_PATH * 6];
    char player_name[128];
    char source[24];
} KboCaptainDisplayCache;

static KboSpinLock g_kbo_captain_display_cache_lock = KBO_SPIN_LOCK_INIT;
static KboCaptainDisplayCache g_kbo_captain_display_cache;

static void kbo_lock_captain_display_cache(void)
{
    kbo_spin_lock(&g_kbo_captain_display_cache_lock);
}

static void kbo_unlock_captain_display_cache(void)
{
    kbo_spin_unlock(&g_kbo_captain_display_cache_lock);
}

static int kbo_captain_display_cache_matches(
    const KboCaptainDisplayCache* cache,
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    const char* loaded_key)
{
    return cache != NULL
        && cache->season == season
        && cache->league_id == league_id
        && cache->team_id == team_id
        && strcmp(cache->loaded_key, loaded_key != NULL ? loaded_key : "") == 0;
}

static void kbo_copy_captain_display_outputs(
    const KboCaptainDisplayCache* display,
    char* out_name,
    size_t out_name_size,
    uint32_t* out_player_id,
    char* out_source,
    size_t out_source_size)
{
    if (out_name != NULL && out_name_size > 0u) {
        snprintf(out_name, out_name_size, "%s", display != NULL ? display->player_name : "");
    }
    if (out_player_id != NULL) {
        *out_player_id = display != NULL ? display->player_id : 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        snprintf(out_source, out_source_size, "%s", display != NULL ? display->source : "");
    }
}

static void kbo_captain_display_loaded_key(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char path[MAX_PATH] = {0};
    char csv_key[192] = "csv:missing";
    if (kbo_captain_selection_csv_path(season, path, sizeof(path))) {
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        if (GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
            snprintf(
                csv_key,
                sizeof(csv_key),
                "csv:%08lx%08lx:%08lx%08lx",
                (unsigned long)attrs.ftLastWriteTime.dwHighDateTime,
                (unsigned long)attrs.ftLastWriteTime.dwLowDateTime,
                (unsigned long)attrs.nFileSizeHigh,
                (unsigned long)attrs.nFileSizeLow);
        }
    }

    snprintf(
        out,
        out_size,
        "%s|%s",
        g_kbo_captain_seed_loaded_key,
        csv_key);
}

static uint8_t* kbo_captain_display_find_player_by_id(uint32_t player_id, int* out_vector_available)
{
    if (out_vector_available != NULL) {
        *out_vector_available = 0;
    }
    if (player_id == 0u) {
        return NULL;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    if (player_vector == 0u || player_count <= 0 || player_count > 200000
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return NULL;
    }
    if (out_vector_available != NULL) {
        *out_vector_available = 1;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

static int kbo_captain_display_fill_from_row(
    const KboCaptainSelectionRow* row,
    KboCaptainDisplayCache* out)
{
    if (row == NULL || out == NULL || row->team_id == 0u) {
        return 0;
    }

    int vector_available = 0;
    uint8_t* player = kbo_captain_display_find_player_by_id(row->player_id, &vector_available);
    if (vector_available) {
        if (player == NULL
                || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
                || !kbo_player_current_assignment_matches_team_or_affiliate(player, row->team_id)) {
            return 0;
        }
    }

    out->player_id = row->player_id;
    snprintf(out->player_name, sizeof(out->player_name), "%s", row->player_name);
    snprintf(out->source, sizeof(out->source), "selection_csv");
    if (player != NULL) {
        char resolved_name[128] = {0};
        kbo_copy_player_display_name(player, resolved_name, sizeof(resolved_name));
        if (resolved_name[0] != '\0' && strcmp(resolved_name, "Unknown player") != 0) {
            snprintf(out->player_name, sizeof(out->player_name), "%s", resolved_name);
        }
    }
    out->found = out->player_name[0] != '\0' || out->player_id != 0u;
    return out->found;
}

static int kbo_captain_display_fill_from_selection_csv(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    KboCaptainDisplayCache* out)
{
    if (out == NULL || team_id == 0u) {
        return 0;
    }

    KboCaptainSelectionRow rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(rows, 0, sizeof(rows));
    int row_count = kbo_captain_load_selection_csv(season, rows, KBO_CAPTAIN_MAX_TEAMS);
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id != team_id) {
            continue;
        }
        if (rows[i].league_id != 0u && league_id != 0u && rows[i].league_id != league_id) {
            continue;
        }
        return kbo_captain_display_fill_from_row(&rows[i], out);
    }
    return 0;
}

static int kbo_captain_compute_display_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    const char* loaded_key,
    KboCaptainDisplayCache* out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    out->season = season;
    out->league_id = league_id;
    out->team_id = team_id;
    snprintf(out->loaded_key, sizeof(out->loaded_key), "%s", loaded_key != NULL ? loaded_key : "");

    if (kbo_captain_display_fill_from_selection_csv(season, league_id, team_id, out)) {
        return 1;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    KboCaptainSeed seed;
    if (!kbo_find_best_captain_seed_for_team(season, league_id, team_id, team, &seed)) {
        return 0;
    }

    snprintf(out->player_name, sizeof(out->player_name), "%s", seed.player_name);
    snprintf(out->source, sizeof(out->source), "%s", seed.source);
    out->player_id = seed.player_id;

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    int vector_available = 0;
    int resolved_seed_player = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            && player_vector != 0u
            && player_count > 0
            && player_count <= 200000
            && memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        vector_available = 1;
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
                continue;
            }
            if (!kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
                continue;
            }
            if (!kbo_captain_seed_matches_player(&seed, player)) {
                continue;
            }

            out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            resolved_seed_player = 1;
            char resolved_name[128] = {0};
            kbo_copy_player_display_name(player, resolved_name, sizeof(resolved_name));
            if (resolved_name[0] != '\0' && strcmp(resolved_name, "Unknown player") != 0) {
                snprintf(out->player_name, sizeof(out->player_name), "%s", resolved_name);
            }
            break;
        }
    }

    if (vector_available && !resolved_seed_player) {
        out->player_name[0] = '\0';
        out->source[0] = '\0';
        out->player_id = 0u;
        out->found = 0;
        return 0;
    }

    out->found = out->player_name[0] != '\0' || out->player_id != 0u;
    return out->found;
}

int kbo_get_captain_for_team(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    char* out_name,
    size_t out_name_size,
    uint32_t* out_player_id,
    char* out_source,
    size_t out_source_size)
{
    if (out_name != NULL && out_name_size > 0u) {
        out_name[0] = '\0';
    }
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        out_source[0] = '\0';
    }
    if (season == 0u || team_id == 0u) {
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();

    char loaded_key[MAX_PATH * 6] = {0};
    kbo_captain_display_loaded_key(season, loaded_key, sizeof(loaded_key));

    kbo_lock_captain_display_cache();
    if (kbo_captain_display_cache_matches(
            &g_kbo_captain_display_cache,
            season,
            league_id,
            team_id,
            loaded_key)
            && (out_player_id == NULL
                || g_kbo_captain_display_cache.player_id != 0u
                || !g_kbo_captain_display_cache.found)) {
        KboCaptainDisplayCache cached = g_kbo_captain_display_cache;
        kbo_unlock_captain_display_cache();
        kbo_copy_captain_display_outputs(
            &cached,
            out_name,
            out_name_size,
            out_player_id,
            out_source,
            out_source_size);
        return cached.found;
    }
    kbo_unlock_captain_display_cache();

    KboCaptainDisplayCache computed;
    kbo_captain_compute_display_for_team(season, league_id, team_id, loaded_key, &computed);

    kbo_lock_captain_display_cache();
    g_kbo_captain_display_cache = computed;
    kbo_unlock_captain_display_cache();

    kbo_copy_captain_display_outputs(
        &computed,
        out_name,
        out_name_size,
        out_player_id,
        out_source,
        out_source_size);
    return computed.found;
}

int kbo_captain_player_is_team_captain(
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id,
    uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    uint32_t captain_player_id = 0u;
    if (!kbo_get_captain_for_team(
            season,
            league_id,
            team_id,
            NULL,
            0u,
            &captain_player_id,
            NULL,
            0u)) {
        return 0;
    }
    return captain_player_id != 0u && captain_player_id == player_id;
}
