static void kbo_hub_sanitize_logo_prefix(const char* name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (name == NULL) {
        return;
    }

    size_t write = 0;
    int last_was_sep = 1;
    for (size_t read = 0; name[read] != '\0' && write + 1 < out_size; read++) {
        char ch = name[read];
        if (kbo_hub_ascii_is_alnum(ch)) {
            out[write++] = ch;
            last_was_sep = 0;
        } else if (!last_was_sep) {
            out[write++] = '_';
            last_was_sep = 1;
        }
    }
    while (write > 0 && out[write - 1] == '_') {
        write--;
    }
    out[write] = '\0';
}

static int kbo_hub_parse_four_digits(const char* text, int index)
{
    int value = 0;
    for (int i = 0; i < 4; i++) {
        char ch = text[index + i];
        if (ch < '0' || ch > '9') {
            return -1;
        }
        value = (value * 10) + (ch - '0');
    }
    return value;
}

static int kbo_hub_logo_filename_score(const char* file_name, uint32_t year)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return -1000000;
    }

    int has_year_token = 0;
    int best_year_score = year == 0 ? 0 : -100000;
    size_t len = strlen(file_name);
    for (size_t i = 0; i + 4 <= len; i++) {
        int start_year = kbo_hub_parse_four_digits(file_name, (int)i);
        if (start_year < 1800 || start_year > 3000) {
            continue;
        }
        has_year_token = 1;
        int score = -5000;
        if (i + 9 <= len && file_name[i + 4] == '-') {
            int end_year = kbo_hub_parse_four_digits(file_name, (int)i + 5);
            if (end_year >= start_year) {
                score = (year >= (uint32_t)start_year && year <= (uint32_t)end_year)
                    ? 10000 - abs((int)year - start_year) : -1000;
                i += 8;
            }
        } else if (year == (uint32_t)start_year) {
            score = 12000;
        }
        if (score > best_year_score) {
            best_year_score = score;
        }
    }

    int score = has_year_token ? best_year_score : 100;
    if (strstr(file_name, "_small_50") != NULL) { score += 700; }
    if (strstr(file_name, "_away")     != NULL
            || strstr(file_name, "_Away") != NULL) { score -= 500; }
    return score;
}

static int kbo_hub_find_best_logo_file(
    const char* directory, const char* search_prefix,
    uint32_t year, char* out, size_t out_size)
{
    if (directory == NULL || search_prefix == NULL || search_prefix[0] == '\0'
            || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\%s*.*", directory, search_prefix);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int best_score = -1000000;
    char best_path[MAX_PATH] = {0};
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        size_t name_len = strlen(find_data.cFileName);
        int is_png = name_len >= 4 && ascii_equals_ignore_case(find_data.cFileName + name_len - 4, ".png");
        int is_oi  = name_len >= 3 && ascii_equals_ignore_case(find_data.cFileName + name_len - 3, ".oi");
        if (!is_png && !is_oi) {
            continue;
        }
        int score = kbo_hub_logo_filename_score(find_data.cFileName, year);
        if (is_png) { score += 50; }
        if (score > best_score) {
            best_score = score;
            snprintf(best_path, sizeof(best_path), "%s\\%s", directory, find_data.cFileName);
        }
    } while (FindNextFileA(find, &find_data));

    FindClose(find);
    if (best_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", best_path);
    return 1;
}

static int kbo_hub_get_league_logo_path(uint32_t league_id, uint32_t year, char* out, size_t out_size)
{
    (void)league_id;
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    const char* league_logo = "2024~ KBO.png";
    if      (year >= 1982 && year <= 1999) { league_logo = "1982~1999 KBO.png"; }
    else if (year >= 2000 && year <= 2011) { league_logo = "2000~2011 KBO.png"; }
    else if (year >= 2012 && year <= 2023) { league_logo = "2012~2023 KBO.png"; }

    kbo_hub_logo_asset_path(league_logo, out, out_size);
    return out[0] != '\0' && GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES;
}

static int kbo_hub_get_team_logo_path(uint32_t team_id, uint32_t year, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    char prefix[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, team_name, sizeof(team_name), NULL);
    kbo_hub_sanitize_logo_prefix(team_name, prefix, sizeof(prefix));
    if (prefix[0] == '\0') {
        return 0;
    }

    if (kbo_hub_find_best_logo_file(
            "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\logos",
            prefix, year, out, out_size)) {
        return 1;
    }

    char cap_prefix[112] = {0};
    snprintf(cap_prefix, sizeof(cap_prefix), "caps_%s", prefix);
    return kbo_hub_find_best_logo_file(
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Out of the Park Baseball 27\\data\\ballcaps",
        cap_prefix, year, out, out_size);
}

static void kbo_hub_ensure_header_logos(uint32_t league_id, uint32_t team_id, uint32_t year)
{
    if (g_kbo_hub_logo_cache_league_id == league_id
            && g_kbo_hub_logo_cache_team_id == team_id
            && g_kbo_hub_logo_cache_year == year) {
        return;
    }

    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_league_logo);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_team_logo);
    g_kbo_hub_logo_cache_league_id = league_id;
    g_kbo_hub_logo_cache_team_id   = team_id;
    g_kbo_hub_logo_cache_year      = year;

    char path[MAX_PATH] = {0};
    kbo_hub_get_league_logo_path(league_id, year, path, sizeof(path));
    g_kbo_hub_asset_selected_league_logo = kbo_hub_load_png_hbitmap_wic(path);

    if (kbo_hub_get_team_logo_path(team_id, year, path, sizeof(path))) {
        g_kbo_hub_asset_selected_team_logo = kbo_hub_load_png_hbitmap_wic(path);
    }
}
