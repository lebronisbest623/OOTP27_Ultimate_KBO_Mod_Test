/* International established free-agent quality diagnostics. Included from native/src/foreign_waiver_ai.inc. */

__declspec(noinline) void ootp_kbo_intl_established_fa_player_probe_wrapper(
    uintptr_t player_ptr,
    uintptr_t league_ptr)
{
    if (player_ptr == 0 || league_ptr == 0) {
        return;
    }

    uint32_t primary_id = 0u;
    uint32_t fallback_id = 0u;
    if (!kbo_intl_established_fa_league_matches(league_ptr, &primary_id, &fallback_id)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        append_logf(
            "international established FA quality probe skipped reason=player_unreadable player=%p league=%p league_id=%u/%u",
            (void*)player_ptr,
            (void*)league_ptr,
            primary_id,
            fallback_id);
        return;
    }

    LONG slot = InterlockedIncrement(&g_kbo_intl_established_fa_quality_probe_log_count);
    if (slot > 512) {
        if (slot == 513) {
            append_log_line("international established FA quality probe suppressed after 512 generated players");
        }
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    uint8_t draft_class = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_CLASS_OFFSET);
    uint8_t draft_subtype = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET);
    uint8_t draft_eligible = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET);
    uint8_t draft_extra = *(uint8_t*)(player + OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET);
    uint8_t generation_flags = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_FLAGS_OFFSET);
    uint8_t generation_context = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET);
    uint8_t generation_grade = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_GRADE_OFFSET);
    uint8_t generation_special = *(uint8_t*)(player + OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int32_t value_score = kbo_foreign_waiver_value_score(player);
    int asian_quota = kbo_nation_is_asian_quota_candidate(nation_id);

    append_logf(
        "international established FA quality probe #%ld player=%p player_id=%u league=%p league_id=%u/%u nation=%u asian_quota=%d age=%d pos=%u/%u gen=flags:%u context:%u grade:%u special:%u draft=db1:%u db2:%u db5:%u db6:%u value=overall:%d talent:%d ratings:%d career:%d score:%d",
        slot,
        (void*)player_ptr,
        player_id,
        (void*)league_ptr,
        primary_id,
        fallback_id,
        nation_id,
        asian_quota,
        (int)age,
        position_group,
        position_role,
        generation_flags,
        generation_context,
        generation_grade,
        generation_special,
        draft_class,
        draft_subtype,
        draft_eligible,
        draft_extra,
        overall,
        talent,
        ratings,
        career,
        value_score);
}
