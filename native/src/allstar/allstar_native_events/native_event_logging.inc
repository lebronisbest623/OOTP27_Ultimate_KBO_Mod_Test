static void log_kbo_allstar_native_event_state(const char* prefix, uintptr_t league_ptr, const char* source)
{
    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (league == NULL || !memory_range_readable(league, layout.league_id_fallback_offset + sizeof(uint32_t))) {
        append_logf("KBO all-star native events %s source=%s league=%p reason=unreadable", prefix, source != NULL ? source : "", league);
        return;
    }

    uint16_t asg_year = 0;
    uint8_t asg_day = 0;
    uint8_t asg_month = 0;
    if (memory_range_readable(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET, 8u)) {
        asg_year = *(uint16_t*)(league + OOTP27_ALLSTAR_DATE_YEAR_OFFSET);
        asg_day = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_DAY_OFFSET);
        asg_month = *(uint8_t*)(league + OOTP27_ALLSTAR_DATE_MONTH_OFFSET);
    }

    append_logf(
        "KBO all-star native events %s source=%s league=%p league_id=%u/%u year=%u phase=%u game=%u auto=%u team0=0x%08x team1=0x%08x asg=%04u-%02u-%02u",
        prefix,
        source != NULL ? source : "",
        league,
        kbo_allstar_read_u32(league, layout.league_id_primary_offset),
        kbo_allstar_read_u32(league, layout.league_id_fallback_offset),
        *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET),
        *(uint8_t*)(league + OOTP27_KBO_LEAGUE_PHASE_OFFSET),
        *(uint8_t*)(league + layout.game_flag_offset),
        *(uint8_t*)(league + layout.auto_schedule_offset),
        kbo_allstar_read_u32(league, layout.team_a_offset),
        kbo_allstar_read_u32(league, layout.team_b_offset),
        (unsigned)asg_year,
        (unsigned)asg_month,
        (unsigned)asg_day);
}
