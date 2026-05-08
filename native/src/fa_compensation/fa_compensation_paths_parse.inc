static int kbo_get_fa_compensation_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation.csv", out, out_size);
}

static int kbo_get_fa_compensation_protected_lists_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation_protected_lists.csv", out, out_size);
}

static int kbo_get_fa_compensation_decisions_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation_decisions.csv", out, out_size);
}

static int kbo_get_fa_compensation_protection_debug_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation_protection_debug.csv", out, out_size);
}

static uint32_t kbo_fa_compensation_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    char* end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    return end != text ? (uint32_t)value : 0u;
}

static int32_t kbo_fa_compensation_parse_i32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    char* end = NULL;
    long value = strtol(text, &end, 10);
    return end != text ? (int32_t)value : 0;
}

static void kbo_fa_compensation_copy_token(const char* value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }
    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    if (len > 0u) {
        memcpy(out, value, len);
    }
    out[len] = '\0';
}
