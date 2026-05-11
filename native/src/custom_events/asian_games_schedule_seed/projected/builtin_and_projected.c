#include "../../runtime/common/custom_events_common.h"
#include "builtin_and_projected.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../runtime_memory/runtime_memory.h"

typedef struct KboAsianGamesProjectedHost {
    char city[48];
    char country[48];
} KboAsianGamesProjectedHost;

int kbo_add_asian_games_schedule_seed_locked(const KboAsianGamesScheduleSeed* seed)
{
    if (seed == NULL || seed->year == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        if (g_kbo_asian_games_schedule_seeds[i].year == seed->year) {
            g_kbo_asian_games_schedule_seeds[i] = *seed;
            return 0;
        }
    }
    if (g_kbo_asian_games_schedule_seed_count >= KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX) {
        return 0;
    }
    g_kbo_asian_games_schedule_seeds[g_kbo_asian_games_schedule_seed_count++] = *seed;
    return 1;
}

void kbo_add_builtin_asian_games_schedule_seed_locked(
    uint32_t year,
    const char* host_city,
    const char* host_country,
    const char* status,
    uint32_t tournament_start,
    uint32_t tournament_end,
    uint32_t selection_date,
    uint32_t departure_date,
    uint32_t final_date,
    const char* notes)
{
    KboAsianGamesScheduleSeed seed;
    memset(&seed, 0, sizeof(seed));
    seed.year = year;
    kbo_asian_games_schedule_copy_text(seed.host_city, sizeof(seed.host_city), host_city);
    kbo_asian_games_schedule_copy_text(seed.host_country, sizeof(seed.host_country), host_country);
    kbo_asian_games_schedule_copy_text(seed.status, sizeof(seed.status), status);
    seed.tournament_start = tournament_start;
    seed.tournament_end = tournament_end;
    seed.selection_date = selection_date;
    seed.departure_date = departure_date;
    seed.final_date = final_date;
    seed.auto_schedule = 1u;
    kbo_asian_games_schedule_copy_text(seed.notes, sizeof(seed.notes), notes);
    kbo_add_asian_games_schedule_seed_locked(&seed);
}

void kbo_add_builtin_asian_games_schedule_seeds_locked(void)
{
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2026u,
        "Aichi-Nagoya",
        "Japan",
        "official",
        20260919u,
        20261004u,
        20260805u,
        20260919u,
        20261004u,
        "Built-in Aichi-Nagoya 2026 Asian Games schedule.");
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2031u,
        "Doha",
        "Qatar",
        "provisional",
        20311104u,
        20311119u,
        20310920u,
        20311104u,
        20311119u,
        "Built-in provisional Doha 2031 Asian Games schedule.");
    kbo_add_builtin_asian_games_schedule_seed_locked(
        2035u,
        "Riyadh",
        "Saudi Arabia",
        "provisional",
        20351129u,
        20351214u,
        20351015u,
        20351129u,
        20351214u,
        "Built-in provisional Riyadh 2035 Asian Games schedule.");
}

int kbo_asian_games_year_is_projected(uint32_t year)
{
    return year >= 2039u && year <= 2200u && ((year - 2039u) % 4u) == 0u;
}

uint32_t kbo_next_projected_asian_games_year(uint32_t from_year)
{
    if (from_year > 2200u) {
        return 0u;
    }
    if (from_year <= 2039u) {
        return 2039u;
    }
    uint32_t delta = from_year - 2039u;
    uint32_t year = 2039u + ((delta + 3u) / 4u) * 4u;
    return year <= 2200u ? year : 0u;
}

static uint32_t kbo_asian_games_projected_hash(uint32_t year, uint32_t salt)
{
    uint32_t value = year ^ (salt * 0x9E3779B9u);
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

static int kbo_get_global_asian_games_projected_hosts_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\asian_games_projected_hosts.csv", local_app_data);
    return out[0] != '\0';
}

static int kbo_get_save_asian_games_projected_hosts_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("asian_games_projected_hosts.csv", out, out_size);
}

static int kbo_import_asian_games_projected_hosts_file(
    const char* path,
    KboAsianGamesProjectedHost* hosts,
    int capacity,
    int count)
{
    if (path == NULL || path[0] == '\0' || hosts == NULL || capacity <= 0 || count >= capacity) {
        return count;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return count;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
        CloseHandle(file);
        return count;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return count;
    }

    DWORD read = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        char* cursor = raw;
        while (*cursor != '\0' && count < capacity) {
            char* next = strchr(cursor, '\n');
            if (next == NULL) {
                next = cursor + strlen(cursor);
            }

            char line[256] = {0};
            size_t line_len = (size_t)(next - cursor);
            while (line_len > 0u && (cursor[line_len - 1u] == '\r' || cursor[line_len - 1u] == '\n')) {
                line_len--;
            }
            if (line_len >= sizeof(line)) {
                line_len = sizeof(line) - 1u;
            }
            memcpy(line, cursor, line_len);

            char* p = line;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p != '\0' && *p != '#' && *p != ';') {
                const char* cell = p;
                char city[48] = {0};
                char country[48] = {0};
                kbo_asian_games_schedule_read_cell(&cell, city, sizeof(city));
                kbo_asian_games_schedule_read_cell(&cell, country, sizeof(country));
                if (!ascii_equals_ignore_case(city, "city")
                        && !ascii_equals_ignore_case(city, "host_city")
                        && city[0] != '\0'
                        && country[0] != '\0') {
                    kbo_asian_games_schedule_copy_text(hosts[count].city, sizeof(hosts[count].city), city);
                    kbo_asian_games_schedule_copy_text(hosts[count].country, sizeof(hosts[count].country), country);
                    count++;
                }
            }

            if (*next == '\0') {
                break;
            }
            cursor = next + 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
    return count;
}

static int kbo_load_asian_games_projected_hosts(KboAsianGamesProjectedHost* hosts, int capacity)
{
    if (hosts == NULL || capacity <= 0) {
        return 0;
    }
    memset(hosts, 0, sizeof(hosts[0]) * (size_t)capacity);

    int count = 0;
    char global_path[MAX_PATH] = {0};
    char save_path[MAX_PATH] = {0};
    kbo_get_global_asian_games_projected_hosts_path(global_path, sizeof(global_path));
    kbo_get_save_asian_games_projected_hosts_path(save_path, sizeof(save_path));
    count = kbo_import_asian_games_projected_hosts_file(global_path, hosts, capacity, count);
    count = kbo_import_asian_games_projected_hosts_file(save_path, hosts, capacity, count);
    return count;
}

static int kbo_asian_games_projected_host_conflicts_with_schedule(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t year,
    const char* city,
    const char* country,
    uint32_t city_cooldown_years,
    uint32_t country_cooldown_years)
{
    if (schedule == NULL || schedule->year == 0u || schedule->year >= year) {
        return 0;
    }
    uint32_t years_since = year - schedule->year;
    if (city != NULL
            && city[0] != '\0'
            && schedule->host_city[0] != '\0'
            && years_since <= city_cooldown_years
            && ascii_equals_ignore_case(schedule->host_city, city)) {
        return 1;
    }
    if (country != NULL
            && country[0] != '\0'
            && schedule->host_country[0] != '\0'
            && years_since <= country_cooldown_years
            && ascii_equals_ignore_case(schedule->host_country, country)) {
        return 1;
    }
    return 0;
}

static int kbo_asian_games_projected_host_is_available(
    uint32_t year,
    const char* city,
    const char* country,
    uint32_t city_cooldown_years,
    uint32_t country_cooldown_years)
{
    if (city == NULL || city[0] == '\0' || country == NULL || country[0] == '\0') {
        return 0;
    }

    for (int i = 0; i < g_kbo_asian_games_schedule_seed_count; i++) {
        if (kbo_asian_games_projected_host_conflicts_with_schedule(
                &g_kbo_asian_games_schedule_seeds[i],
                year,
                city,
                country,
                city_cooldown_years,
                country_cooldown_years)) {
            return 0;
        }
    }

    for (uint32_t previous_year = year - 4u;
            previous_year >= 2039u && year - previous_year <= city_cooldown_years;
            previous_year -= 4u) {
        KboAsianGamesScheduleSeed previous;
        kbo_build_projected_asian_games_schedule(previous_year, &previous);
        if (kbo_asian_games_projected_host_conflicts_with_schedule(
                &previous,
                year,
                city,
                country,
                city_cooldown_years,
                country_cooldown_years)) {
            return 0;
        }
        if (previous_year < 2043u) {
            break;
        }
    }

    return 1;
}

static int kbo_choose_projected_asian_games_host_index(
    uint32_t year,
    const KboAsianGamesProjectedHost* hosts,
    int host_count)
{
    if (hosts == NULL || host_count <= 0) {
        return -1;
    }
    uint32_t start = kbo_asian_games_projected_hash(year, 17u) % (uint32_t)host_count;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t city_cooldown_years = pass == 0 ? 24u : 24u;
        uint32_t country_cooldown_years = pass == 0 ? 16u : 0u;
        for (int offset = 0; offset < host_count; offset++) {
            int index = (int)((start + (uint32_t)offset) % (uint32_t)host_count);
            if (kbo_asian_games_projected_host_is_available(
                    year,
                    hosts[index].city,
                    hosts[index].country,
                    city_cooldown_years,
                    country_cooldown_years)) {
                return index;
            }
        }
    }
    return (int)start;
}

static uint32_t kbo_subtract_days_yyyymmdd(uint32_t yyyymmdd, uint32_t days)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year == 0u || month == 0u || month > 12u || day == 0u || day > (uint32_t)kbo_days_in_month(year, month)) {
        return 0u;
    }
    while (days > 0u) {
        if (day > 1u) {
            day--;
        } else if (month > 1u) {
            month--;
            day = (uint32_t)kbo_days_in_month(year, month);
        } else {
            if (year == 0u) {
                return 0u;
            }
            year--;
            month = 12u;
            day = (uint32_t)kbo_days_in_month(year, month);
        }
        days--;
    }
    return year * 10000u + month * 100u + day;
}

void kbo_build_projected_asian_games_schedule(uint32_t year, KboAsianGamesScheduleSeed* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!kbo_asian_games_year_is_projected(year)) {
        return;
    }
    KboAsianGamesProjectedHost hosts[64];
    int host_count = kbo_load_asian_games_projected_hosts(hosts, (int)(sizeof(hosts) / sizeof(hosts[0])));
    int host_index = kbo_choose_projected_asian_games_host_index(year, hosts, host_count);
    uint32_t start_day = 10u + (kbo_asian_games_projected_hash(year, 31u) % 19u);
    uint32_t duration_days = 14u + (kbo_asian_games_projected_hash(year, 43u) % 5u);
    uint32_t selection_lead_days = 42u + (kbo_asian_games_projected_hash(year, 59u) % 18u);
    uint32_t tournament_start = year * 10000u + 900u + start_day;
    uint32_t tournament_end = kbo_add_days_yyyymmdd(tournament_start, duration_days);
    uint32_t selection_date = kbo_subtract_days_yyyymmdd(tournament_start, selection_lead_days);
    if (selection_date / 10000u != year) {
        selection_date = year * 10000u + 805u;
    }
    out->year = year;
    if (host_index >= 0) {
        kbo_asian_games_schedule_copy_text(out->host_city, sizeof(out->host_city), hosts[host_index].city);
        kbo_asian_games_schedule_copy_text(out->host_country, sizeof(out->host_country), hosts[host_index].country);
    }
    kbo_asian_games_schedule_copy_text(out->status, sizeof(out->status), "projected");
    out->tournament_start = tournament_start;
    out->tournament_end = tournament_end;
    out->selection_date = selection_date;
    out->departure_date = out->tournament_start;
    out->final_date = out->tournament_end;
    out->auto_schedule = 1u;
    kbo_asian_games_schedule_copy_text(
        out->notes,
        sizeof(out->notes),
        "Projected post-2035 Asian Games schedule; override this year in asian_games_schedule_seed.csv when official dates are known.");
}
