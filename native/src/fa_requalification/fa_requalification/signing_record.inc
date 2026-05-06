static int kbo_record_fa_requalification_signing(uint32_t player_id, uint32_t team_id, uint32_t signing_year, const char* source)
{
    if (player_id == 0u || team_id == 0u || signing_year < 1982u || signing_year > 2200u) {
        return 0;
    }

    kbo_lock_fa_requalification_records();
    KboFaRequalificationRecord records[KBO_FA_REQUALIFICATION_MAX];
    int count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (records[i].player_id == player_id) {
            found = i;
            break;
        }
    }

    if (found >= 0) {
        if (records[found].original_team_id == team_id && records[found].last_fa_year == signing_year) {
            append_logf(
                "KBO FA requalification signing already recorded source=%s player=%u team=%u signing_year=%u fa_count=%u",
                source != NULL ? source : "",
                player_id,
                team_id,
                signing_year,
                records[found].fa_count);
            kbo_unlock_fa_requalification_records();
            return 1;
        }
        records[found].original_team_id = team_id;
        records[found].last_fa_year = signing_year;
        records[found].fa_count = records[found].fa_count + 1u;
        if (records[found].fa_count == 0u) {
            records[found].fa_count = 1u;
        }
    } else {
        if (count >= KBO_FA_REQUALIFICATION_MAX) {
            append_logf("KBO FA requalification signing record dropped source=%s player=%u team=%u reason=max_records", source != NULL ? source : "", player_id, team_id);
            kbo_unlock_fa_requalification_records();
            return 0;
        }
        records[count++] = (KboFaRequalificationRecord){
            player_id,
            team_id,
            signing_year,
            1u
        };
    }

    int ok = kbo_write_fa_requalification_records(records, count);
    append_logf(
        "KBO FA requalification signing recorded source=%s player=%u team=%u signing_year=%u eligible_year=%u records=%d ok=%d",
        source != NULL ? source : "",
        player_id,
        team_id,
        signing_year,
        signing_year + KBO_FA_REQUALIFICATION_YEARS,
        count,
        ok);
    kbo_unlock_fa_requalification_records();
    return ok;
}

