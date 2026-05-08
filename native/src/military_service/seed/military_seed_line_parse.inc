/* Military service seed CSV line parsing. Included from native/KBOFix.c. */

static int kbo_parse_military_service_seed_line(const char* line, KboMilitaryServiceSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char copy[240] = {0};
    size_t len = strlen(line);
    if (len >= sizeof(copy)) {
        len = sizeof(copy) - 1u;
    }
    memcpy(copy, line, len);

    char* p = copy;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    char* fields[8] = {0};
    int field_count = 0;
    fields[field_count++] = p;
    for (char* q = p; *q != '\0' && field_count < 8; q++) {
        if (*q == ',') {
            *q = '\0';
            fields[field_count++] = q + 1;
        }
    }
    for (int i = 0; i < field_count; i++) {
        kbo_military_trim_csv_token_in_place(fields[i]);
    }
    if (fields[0] == NULL || fields[0][0] == '\0'
            || _stricmp(fields[0], "source_key") == 0
            || _stricmp(fields[0], "player_id") == 0) {
        return 0;
    }

    snprintf(out->key, sizeof(out->key), "%s", fields[0]);
    uint32_t parsed_u32 = 0u;
    if (kbo_military_parse_u32_full_token(out->key, &parsed_u32)) {
        out->player_id = parsed_u32;
    }

    if (field_count == 3
            && fields[1][0] >= '0' && fields[1][0] <= '9'
            && fields[2][0] >= '0' && fields[2][0] <= '9') {
        uint32_t today_serial = kbo_current_date_serial();
        uint32_t days_left = (uint32_t)strtoul(fields[2], NULL, 10);
        snprintf(out->service_team_code, sizeof(out->service_team_code), "SANG");
        snprintf(out->original_team_code, sizeof(out->original_team_code), "%s", fields[1]);
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        out->service_return_yyyymmdd = today_serial != 0u
            ? kbo_military_serial_to_yyyymmdd(today_serial + days_left)
            : 0u;
        return out->key[0] != '\0' || out->player_id != 0u;
    }

    if (field_count > 5 && fields[5][0] != '\0'
            && kbo_military_parse_u32_full_token(fields[5], &parsed_u32)) {
        out->player_id = parsed_u32;
    }
    snprintf(out->service_team_code, sizeof(out->service_team_code), "%s",
        (field_count > 1 && fields[1][0] != '\0') ? fields[1] : "SANG");
    if (field_count > 2 && fields[2][0] != '\0') {
        snprintf(out->original_team_code, sizeof(out->original_team_code), "%s", fields[2]);
    }

    uint32_t numeric_field4 = 0u;
    int field4_is_numeric = field_count > 4
        && fields[4][0] != '\0'
        && kbo_military_parse_u32_full_token(fields[4], &numeric_field4);
    int looks_like_old_start_total = field_count > 5
        && field4_is_numeric
        && kbo_military_parse_yyyymmdd(fields[3]) != 0u;

    if (looks_like_old_start_total) {
        out->service_start_yyyymmdd = kbo_military_parse_yyyymmdd(fields[3]);
        out->service_total_days = (int32_t)numeric_field4;
        out->service_return_yyyymmdd = kbo_military_yyyymmdd_add_days(
            out->service_start_yyyymmdd,
            out->service_total_days > 0 ? out->service_total_days : KBO_MILITARY_SERVICE_DAYS);
    } else {
        out->service_return_yyyymmdd = field_count > 3 ? kbo_military_parse_yyyymmdd(fields[3]) : 0u;
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        if (field_count > 4 && field4_is_numeric) {
            out->player_id = numeric_field4;
        }
    }
    if (out->service_total_days <= 0) {
        out->service_total_days = KBO_MILITARY_SERVICE_DAYS;
    }
    return out->key[0] != '\0' || out->player_id != 0u;
}
