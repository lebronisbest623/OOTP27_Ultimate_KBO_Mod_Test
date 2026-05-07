/* International established free-agent generation multiplier. Included from native/src/foreign_waiver_ai.inc. */

static LONG g_kbo_intl_established_fa_quality_probe_log_count = 0;
static LONG g_kbo_intl_established_fa_generation_filter_block_count = 0;

static uint32_t kbo_intl_established_fa_read_league_id(uintptr_t league_ptr, uint32_t offset)
{
    if (league_ptr == 0 || !memory_range_readable((void*)(league_ptr + offset), sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(league_ptr + offset);
}

static int kbo_intl_established_fa_league_matches(uintptr_t league_ptr, uint32_t* out_primary_id, uint32_t* out_fallback_id)
{
    uint32_t primary_id = kbo_intl_established_fa_read_league_id(league_ptr, OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    uint32_t fallback_id = kbo_intl_established_fa_read_league_id(league_ptr, OOTP27_KBO_LEAGUE_ID_OFFSET);
    if (out_primary_id != NULL) {
        *out_primary_id = primary_id;
    }
    if (out_fallback_id != NULL) {
        *out_fallback_id = fallback_id;
    }

    uint32_t configured_id = kbo_get_foreign_waiver_league_id();
    if (configured_id == 0u) {
        configured_id = kbo_resolve_kbo_league_id();
    }
    if (configured_id == 0u) {
        return 0;
    }
    return primary_id == configured_id || fallback_id == configured_id;
}

__declspec(noinline) int32_t ootp_kbo_intl_established_fa_count_wrapper(int32_t original_count, uintptr_t league_ptr)
{
    if (original_count <= 0) {
        return original_count;
    }

    uint32_t primary_id = 0u;
    uint32_t fallback_id = 0u;
    if (!kbo_intl_established_fa_league_matches(league_ptr, &primary_id, &fallback_id)) {
        return original_count;
    }

    InterlockedExchange(&g_kbo_intl_established_fa_quality_probe_log_count, 0);
    InterlockedExchange(&g_kbo_intl_established_fa_generation_filter_block_count, 0);

    int multiplier = kbo_get_intl_established_fa_multiplier();
    if (multiplier <= 1) {
        return original_count;
    }

    int64_t scaled = (int64_t)original_count * (int64_t)multiplier;
    if (scaled > 1000) {
        scaled = 1000;
    }

    append_logf(
        "international established FA multiplier league=%p league_id=%u/%u original=%d multiplier=%d scaled=%d",
        (void*)league_ptr,
        primary_id,
        fallback_id,
        original_count,
        multiplier,
        (int32_t)scaled);
    kbo_intl_established_fa_postscan_schedule(
        original_count,
        (int32_t)scaled,
        multiplier,
        primary_id,
        fallback_id);
    return (int32_t)scaled;
}
