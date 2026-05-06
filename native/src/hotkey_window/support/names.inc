static void kbo_hub_draw_text(HDC hdc, const char* text, RECT rect, COLORREF color, HFONT font, UINT format)
{
    if (text == NULL) {
        return;
    }

    HFONT old_font = NULL;
    if (font != NULL) {
        old_font = (HFONT)SelectObject(hdc, font);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len > 0) {
        WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
        if (wide != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len) > 0) {
                DrawTextW(hdc, wide, -1, &rect, format);
            }
            HeapFree(GetProcessHeap(), 0, wide);
        }
    }

    if (old_font != NULL) {
        SelectObject(hdc, old_font);
    }
}

static int kbo_hub_ascii_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int kbo_hub_ascii_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int kbo_hub_ascii_is_alnum(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int kbo_hub_name_fragment_plausible(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    size_t len = strlen(text);
    if (len < 2 || len > 32) {
        return 0;
    }

    int alpha_count = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (kbo_hub_ascii_is_digit(c)) {
            return 0;
        }
        if (c == '_' || c == '/' || c == '\\' || c == ':' || c == '%' || c == '#' || c == '$' || c == '@') {
            return 0;
        }
        if (!(kbo_hub_ascii_is_alpha(c) || c == ' ' || c == '-' || c == '\'' || c == '.')) {
            return 0;
        }
        if (kbo_hub_ascii_is_alpha(c)) {
            alpha_count++;
        }
    }

    if (alpha_count < 2) {
        return 0;
    }
    if (ascii_equals_ignore_case(text, "KBO")
            || ascii_equals_ignore_case(text, "MLB")
            || ascii_equals_ignore_case(text, "SANG")
            || ascii_equals_ignore_case(text, "KPB")
            || ascii_equals_ignore_case(text, "USA")) {
        return 0;
    }

    return 1;
}

static int kbo_hub_copy_raw_pointer_string(uint8_t* object_base, uint32_t pointer_offset, char* out, size_t out_size)
{
    if (object_base == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    uint8_t* pointer_slot = object_base + pointer_offset;
    if (!memory_range_readable(pointer_slot, sizeof(char*))) {
        return 0;
    }
    const char* text = *(const char**)pointer_slot;
    return copy_limited_ascii_string(text, out, out_size);
}

static int kbo_hub_append_unique_name_fragment(char fragments[][48], int* count, const char* text)
{
    if (fragments == NULL || count == NULL || *count >= 3 || !kbo_hub_name_fragment_plausible(text)) {
        return 0;
    }
    for (int i = 0; i < *count; i++) {
        if (ascii_equals_ignore_case(fragments[i], text)) {
            return 0;
        }
    }
    snprintf(fragments[*count], 48, "%s", text);
    (*count)++;
    return 1;
}

static void kbo_hub_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    kbo_copy_player_display_name(player, out, out_size);
}

static int kbo_hub_team_name_candidate_score(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return -1000;
    }

    size_t len = strlen(text);
    if (len < 2 || len > 64) {
        return -1000;
    }

    int alpha_count = 0;
    int space_count = 0;
    int lowercase_count = 0;
    int uppercase_count = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c < 0x20 || c > 0x7e || kbo_hub_ascii_is_digit(c)) {
            return -1000;
        }
        if (kbo_hub_ascii_is_alpha(c)) {
            alpha_count++;
            if (c >= 'a' && c <= 'z') { lowercase_count++; } else { uppercase_count++; }
        } else if (c == ' ') {
            space_count++;
        } else if (!(c == '-' || c == '\'' || c == '.' || c == '&')) {
            return -1000;
        }
    }

    if (alpha_count < 2) {
        return -1000;
    }

    int score = (int)len;
    if (space_count > 0) { score += 80; }
    if (lowercase_count > 0) { score += 20; }
    if (space_count == 0 && len <= 5 && uppercase_count == alpha_count) { score -= 80; }
    if (ascii_equals_ignore_case(text, "SANG") || ascii_equals_ignore_case(text, "KPB")) { score -= 200; }

    return score;
}

static void kbo_hub_copy_team_display_name_from_ptr(uint8_t* team, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
        return;
    }

    static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };

    char city[64] = {0};
    char nickname[64] = {0};
    char best[96] = {0};
    int best_score = -1000;

    copy_ootp_string_object_text(team, 0x10u, city,     sizeof(city));
    copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));

    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char text[96] = {0};
        if (!copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))) {
            continue;
        }
        int score = kbo_hub_team_name_candidate_score(text);
        if (score > best_score) {
            best_score = score;
            snprintf(best, sizeof(best), "%s", text);
        }
    }

    if (best_score > 0 && strchr(best, ' ') != NULL) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    if (kbo_hub_team_name_candidate_score(city) > 0
            && kbo_hub_team_name_candidate_score(nickname) > 0
            && !ascii_equals_ignore_case(city, nickname)) {
        snprintf(out, out_size, "%s %s", city, nickname);
        return;
    }

    if (best_score > 0) {
        snprintf(out, out_size, "%s", best);
        return;
    }

    snprintf(out, out_size, "%s", fallback != NULL && fallback[0] != '\0' ? fallback : "Unknown club");
}

static void kbo_hub_copy_team_display_name_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL) {
        kbo_hub_copy_team_display_name_from_ptr(team, out, out_size, fallback);
        return;
    }

    if (fallback != NULL && fallback[0] != '\0') {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0) {
        snprintf(out, out_size, "Unknown club #%u", team_id);
    } else {
        snprintf(out, out_size, "Unknown club");
    }
}

static void kbo_hub_copy_team_abbrev_by_id(uint32_t team_id, char* out, size_t out_size, const char* fallback)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint32_t parent_team_id = 0u;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }
    if (parent_team_id != 0u && parent_team_id != team_id) {
        char parent_abbrev[16] = {0};
        kbo_hub_copy_team_abbrev_by_id(parent_team_id, parent_abbrev, sizeof(parent_abbrev), fallback);
        if (parent_abbrev[0] != '\0' && strcmp(parent_abbrev, "-") != 0) {
            snprintf(out, out_size, "%s2", parent_abbrev);
            return;
        }
    }

    char name[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, name, sizeof(name), fallback);
    if (kbo_ascii_contains_ignore_case(name, "Lotte"))      { snprintf(out, out_size, "LOT");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Doosan"))     { snprintf(out, out_size, "DOO");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Samsung"))    { snprintf(out, out_size, "SAM");  return; }
    if (kbo_ascii_contains_ignore_case(name, "KIA"))        { snprintf(out, out_size, "KIA");  return; }
    if (kbo_ascii_contains_ignore_case(name, "SSG"))        { snprintf(out, out_size, "SSG");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Hanwha"))     { snprintf(out, out_size, "HAN");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Kiwoom"))     { snprintf(out, out_size, "KIW");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Nexen"))      { snprintf(out, out_size, "NEX");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Heroes"))     { snprintf(out, out_size, "KIW");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Dinos"))      { snprintf(out, out_size, "NC");   return; }
    if (kbo_ascii_contains_ignore_case(name, "NC"))         { snprintf(out, out_size, "NC");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Wiz"))        { snprintf(out, out_size, "KT");   return; }
    if (kbo_ascii_contains_ignore_case(name, "KT"))         { snprintf(out, out_size, "KT");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Twins"))      { snprintf(out, out_size, "LG");   return; }
    if (kbo_ascii_contains_ignore_case(name, "LG"))         { snprintf(out, out_size, "LG");   return; }
    if (kbo_ascii_contains_ignore_case(name, "Sangmu"))     { snprintf(out, out_size, "SANG"); return; }
    if (kbo_ascii_contains_ignore_case(name, "Police"))     { snprintf(out, out_size, "KPB");  return; }
    if (kbo_ascii_contains_ignore_case(name, "Ulsan"))      { snprintf(out, out_size, "ULS");  return; }

    if (fallback != NULL && fallback[0] != '\0' && strlen(fallback) <= 6u) {
        snprintf(out, out_size, "%s", fallback);
    } else if (team_id != 0u) {
        snprintf(out, out_size, "T%u", team_id);
    } else {
        snprintf(out, out_size, "-");
    }
}

