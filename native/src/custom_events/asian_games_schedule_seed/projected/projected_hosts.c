#include "../../runtime/common/custom_events_common.h"
#include "builtin_and_projected.h"
#include <stdio.h>
#include <string.h>
#include "../../../core/dates/core_text_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/core_flags/api/flags_api.h"

typedef struct KboAsianGamesProjectedHost {
    char city[48];
    char country[48];
} KboAsianGamesProjectedHost;

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

int kbo_choose_projected_asian_games_host(
    uint32_t year,
    char* out_city,
    size_t city_size,
    char* out_country,
    size_t country_size)
{
    if (out_city == NULL || city_size == 0u || out_country == NULL || country_size == 0u) {
        return 0;
    }
    out_city[0] = '\0';
    out_country[0] = '\0';

    KboAsianGamesProjectedHost hosts[64];
    int host_count = kbo_load_asian_games_projected_hosts(hosts, (int)(sizeof(hosts) / sizeof(hosts[0])));
    int host_index = kbo_choose_projected_asian_games_host_index(year, hosts, host_count);
    if (host_index < 0) {
        return 0;
    }

    kbo_asian_games_schedule_copy_text(out_city, city_size, hosts[host_index].city);
    kbo_asian_games_schedule_copy_text(out_country, country_size, hosts[host_index].country);
    return 1;
}
