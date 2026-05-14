#include "core_csv.h"

#include <stdlib.h>
#include <string.h>

#include "../../../third_party/csv_fast_reader/csv.h"

struct KboCsvReader {
    CsvHandle handle;
    char* row;
};

KboCsvReader* kbo_csv_reader_open(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    CsvHandle handle = CsvOpen(path);
    if (handle == NULL) {
        return NULL;
    }

    KboCsvReader* reader = (KboCsvReader*)calloc(1u, sizeof(KboCsvReader));
    if (reader == NULL) {
        CsvClose(handle);
        return NULL;
    }

    reader->handle = handle;
    return reader;
}

void kbo_csv_reader_close(KboCsvReader* reader)
{
    if (reader == NULL) {
        return;
    }
    CsvClose(reader->handle);
    free(reader);
}

int kbo_csv_reader_next_row(KboCsvReader* reader)
{
    if (reader == NULL || reader->handle == NULL) {
        return 0;
    }
    reader->row = CsvReadNextRow(reader->handle);
    return reader->row != NULL;
}

int kbo_csv_reader_next_field(KboCsvReader* reader, char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    if (reader == NULL || reader->handle == NULL || reader->row == NULL || out == NULL || out_size == 0u) {
        return 0;
    }

    const char* field = CsvReadNextCol(reader->row, reader->handle);
    if (field == NULL) {
        return 0;
    }

    size_t len = strlen(field);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, field, len);
    out[len] = '\0';
    return 1;
}

static int kbo_csv_reader_read_fields_internal(
    KboCsvReader* reader,
    char* fields,
    size_t field_size,
    int max_fields,
    int trim)
{
    if (reader == NULL || fields == NULL || field_size == 0u || max_fields <= 0) {
        return 0;
    }

    memset(fields, 0, field_size * (size_t)max_fields);
    int count = 0;
    while (count < max_fields) {
        char* slot = fields + ((size_t)count * field_size);
        if (!kbo_csv_reader_next_field(reader, slot, field_size)) {
            break;
        }
        if (trim) {
            kbo_csv_trim_token_in_place(slot);
        }
        count++;
    }
    return count;
}

int kbo_csv_reader_read_fields(KboCsvReader* reader, char* fields, size_t field_size, int max_fields)
{
    return kbo_csv_reader_read_fields_internal(reader, fields, field_size, max_fields, 0);
}

int kbo_csv_reader_read_trimmed_fields(KboCsvReader* reader, char* fields, size_t field_size, int max_fields)
{
    return kbo_csv_reader_read_fields_internal(reader, fields, field_size, max_fields, 1);
}
