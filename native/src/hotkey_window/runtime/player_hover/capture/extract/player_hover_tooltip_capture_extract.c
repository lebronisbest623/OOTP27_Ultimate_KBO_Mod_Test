#include "../player_hover_manager_probe_internal.h"

static int kbo_tooltip_parse_builder_display_value(const char* line, const char* kind, int* out_value)
{
    char needle[48] = {0};
    if (line == NULL || kind == NULL || out_value == NULL) {
        return 0;
    }
    *out_value = 0;
    snprintf(needle, sizeof(needle), "rating_common kind=%s display=", kind);
    const char* value_text = strstr(line, needle);
    if (value_text == NULL) {
        return 0;
    }
    value_text += strlen(needle);

    char* end = NULL;
    long value = strtol(value_text, &end, 10);
    if (end == value_text || value <= 0 || value > 500) {
        return 0;
    }
    *out_value = (int)value;
    return 1;
}

int kbo_tooltip_extract_overall_potential_from_builder_capture(
    int* out_overall,
    int* out_potential)
{
    const char* cursor = g_kbo_player_tooltip_builder_capture;
    int overall = 0;
    int potential = 0;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (out_overall == NULL || out_potential == NULL || cursor[0] == '\0') {
        return 0;
    }

    while (*cursor != '\0' && (overall == 0 || potential == 0)) {
        const char* line_end = strchr(cursor, '\n');
        size_t line_len = line_end != NULL ? (size_t)(line_end - cursor) : strlen(cursor);
        if (line_len > 0u && line_len < 280u) {
            char line[280] = {0};
            int value = 0;
            memcpy(line, cursor, line_len);
            if (overall == 0 && kbo_tooltip_parse_builder_display_value(line, "OVR", &value)) {
                overall = value;
            } else if (potential == 0 && kbo_tooltip_parse_builder_display_value(line, "POT", &value)) {
                potential = value;
            }
        }
        if (line_end == NULL) {
            break;
        }
        cursor = line_end + 1;
    }

    if (overall == 0 || potential == 0) {
        return 0;
    }
    *out_overall = overall;
    *out_potential = potential;
    return 1;
}

static int kbo_tooltip_parse_format_capture_value(const char* line, int* out_value)
{
    if (line == NULL || out_value == NULL) {
        return 0;
    }
    *out_value = 0;
    if (strstr(line, "numeric=1") == NULL
            || strstr(line, "format=\"%c%hd,%hd,%hd,%hd%c\"") != NULL) {
        return 0;
    }

    const char* text = strstr(line, "text=\"");
    if (text != NULL) {
        text += 6;
        char* end = NULL;
        long value = strtol(text, &end, 10);
        if (end != text && end != NULL && *end == '"' && value > 0 && value <= 500) {
            *out_value = (int)value;
            return 1;
        }
    }

    const char* value_hint = strstr(line, "value=");
    if (value_hint != NULL) {
        value_hint += 6;
        char* end = NULL;
        long value = strtol(value_hint, &end, 10);
        if (end != value_hint && value > 0 && value <= 500) {
            *out_value = (int)value;
            return 1;
        }
    }
    return 0;
}

int kbo_tooltip_extract_overall_potential_from_format_capture(
    int* out_overall,
    int* out_potential)
{
    const char* cursor = g_kbo_player_tooltip_rating_capture;
    int values[2] = {0};
    int count = 0;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (out_overall == NULL || out_potential == NULL || cursor[0] == '\0') {
        return 0;
    }

    while (*cursor != '\0' && count < 2) {
        const char* line_end = strchr(cursor, '\n');
        size_t line_len = line_end != NULL ? (size_t)(line_end - cursor) : strlen(cursor);
        if (line_len > 0u && line_len < 360u) {
            char line[360] = {0};
            memcpy(line, cursor, line_len);
            int value = 0;
            if (kbo_tooltip_parse_format_capture_value(line, &value)) {
                values[count++] = value;
            }
        }
        if (line_end == NULL) {
            break;
        }
        cursor = line_end + 1;
    }

    if (count < 2) {
        return 0;
    }
    *out_overall = values[0];
    *out_potential = values[1];
    return 1;
}
