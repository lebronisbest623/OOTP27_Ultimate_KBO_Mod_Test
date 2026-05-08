/* Foreign roster audit output paths. Included from foreign_roster_audit.inc. */

static int get_kbo_foreign_roster_audit_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_roster_audit.csv", out, out_size);
}

static int get_kbo_foreign_roster_snapshot_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_roster_snapshot.csv", out, out_size);
}
