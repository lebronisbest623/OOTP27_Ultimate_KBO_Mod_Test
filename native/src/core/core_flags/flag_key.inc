static int kbo_flag_key_from_file_name(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }

    const char* base = file_name;
    for (const char* p = file_name; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') {
            base = p + 1;
        }
    }

    size_t len = strlen(base);
    if (len > 4 && ascii_equals_ignore_case(base + len - 4, ".txt")) {
        len -= 4;
    }
    if (len == 0 || len >= out_size) {
        return 0;
    }

    memcpy(out, base, len);
    out[len] = '\0';
    return 1;
}
