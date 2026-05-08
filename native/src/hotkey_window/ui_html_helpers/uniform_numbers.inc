typedef struct KboHubUniformNumberEntry {
    uint32_t player_id;
    char number[8];
} KboHubUniformNumberEntry;

#define KBO_HUB_UNIFORM_NUMBER_CACHE_CAP 8192

static KboHubUniformNumberEntry g_kbo_hub_uniform_number_cache[KBO_HUB_UNIFORM_NUMBER_CACHE_CAP];
static int g_kbo_hub_uniform_number_cache_count = 0;
static char g_kbo_hub_uniform_number_cache_save_path[MAX_PATH] = {0};

static int kbo_hub_csv_copy_field(const char* line, int target_index, char* out, size_t out_size)
{
    if (line == NULL || target_index < 0 || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    int field = 0;
    int quoted = 0;
    size_t len = 0;
    for (const char* p = line; ; p++) {
        char ch = *p;
        int end = ch == '\0' || ch == '\r' || ch == '\n';
        if (!end && ch == '"') {
            if (quoted && p[1] == '"') {
                if (field == target_index && len + 1 < out_size) {
                    out[len++] = '"';
                }
                p++;
            } else {
                quoted = !quoted;
            }
            continue;
        }
        if (end || (ch == ',' && !quoted)) {
            if (field == target_index) {
                out[len] = '\0';
                kbo_hub_trim_ascii(out);
                return 1;
            }
            field++;
            len = 0;
            if (end) {
                break;
            }
            continue;
        }
        if (field == target_index && len + 1 < out_size) {
            out[len++] = ch;
        }
    }
    return 0;
}

static void kbo_hub_uniform_number_cache_put(uint32_t player_id, const char* number)
{
    if (player_id == 0u || number == NULL || number[0] == '\0') {
        return;
    }

    char clean[8] = {0};
    snprintf(clean, sizeof(clean), "%s", number);
    kbo_hub_trim_ascii(clean);
    if (clean[0] == '\0') {
        return;
    }

    for (int i = 0; i < g_kbo_hub_uniform_number_cache_count; i++) {
        if (g_kbo_hub_uniform_number_cache[i].player_id == player_id) {
            snprintf(g_kbo_hub_uniform_number_cache[i].number, sizeof(g_kbo_hub_uniform_number_cache[i].number), "%s", clean);
            return;
        }
    }
    if (g_kbo_hub_uniform_number_cache_count >= KBO_HUB_UNIFORM_NUMBER_CACHE_CAP) {
        return;
    }

    KboHubUniformNumberEntry* entry = &g_kbo_hub_uniform_number_cache[g_kbo_hub_uniform_number_cache_count++];
    entry->player_id = player_id;
    snprintf(entry->number, sizeof(entry->number), "%s", clean);
}

static void kbo_hub_load_uniform_numbers_from_csv(const char* path, int id_column, int number_column)
{
    if (path == NULL || path[0] == '\0') {
        return;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }

    char line[8192];
    while (fgets(line, sizeof(line), file) != NULL) {
        const char* first = line;
        while (*first == ' ' || *first == '\t') {
            first++;
        }
        if (*first == '\0' || *first == '#' || *first == '/' || *first == ';') {
            continue;
        }

        char id_text[32] = {0};
        char number_text[16] = {0};
        if (!kbo_hub_csv_copy_field(line, id_column, id_text, sizeof(id_text))
                || !kbo_hub_csv_copy_field(line, number_column, number_text, sizeof(number_text))) {
            continue;
        }
        if (id_text[0] < '0' || id_text[0] > '9') {
            continue;
        }
        uint32_t player_id = (uint32_t)strtoul(id_text, NULL, 10);
        kbo_hub_uniform_number_cache_put(player_id, number_text);
    }

    fclose(file);
}

static void kbo_hub_load_uniform_numbers_for_current_save(void)
{
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        g_kbo_hub_uniform_number_cache_count = 0;
        g_kbo_hub_uniform_number_cache_save_path[0] = '\0';
        return;
    }
    if (_stricmp(g_kbo_hub_uniform_number_cache_save_path, save_path) == 0) {
        return;
    }

    g_kbo_hub_uniform_number_cache_count = 0;
    snprintf(g_kbo_hub_uniform_number_cache_save_path, sizeof(g_kbo_hub_uniform_number_cache_save_path), "%s", save_path);

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\import_export\\kbo_rosters.csv", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 8);
    snprintf(path, sizeof(path), "%s\\import_export\\kbo_rosters.txt", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 8);
    snprintf(path, sizeof(path), "%s\\data\\derived\\matching\\uniform_number_patch_report.csv", save_path);
    kbo_hub_load_uniform_numbers_from_csv(path, 0, 7);
}

static void kbo_webview_copy_player_uniform_number(uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (player_id == 0u) {
        return;
    }

    kbo_hub_load_uniform_numbers_for_current_save();
    for (int i = 0; i < g_kbo_hub_uniform_number_cache_count; i++) {
        if (g_kbo_hub_uniform_number_cache[i].player_id == player_id) {
            snprintf(out, out_size, "%s", g_kbo_hub_uniform_number_cache[i].number);
            return;
        }
    }
}

