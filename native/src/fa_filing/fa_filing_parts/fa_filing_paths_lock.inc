static int kbo_get_fa_filing_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_filing.csv", out, out_size);
}

static void kbo_fa_filing_enter_lock(void)
{
    while (InterlockedCompareExchange(&g_kbo_fa_filing_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_fa_filing_leave_lock(void)
{
    InterlockedExchange(&g_kbo_fa_filing_lock, 0);
}
