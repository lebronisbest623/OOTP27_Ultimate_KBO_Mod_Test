/* Foreign reserve-right active-state helpers. */

static int kbo_is_foreign_waiver_right_active(const KboForeignWaiverRetention* rec, uint32_t today)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->team_id == 0u
            || rec->retained_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd < rec->retained_on_yyyymmdd
            || today == 0u) {
        return 0;
    }
    return rec->retained_on_yyyymmdd <= today && today <= rec->expires_on_yyyymmdd;
}

static int kbo_is_foreign_waiver_right_expired(const KboForeignWaiverRetention* rec, uint32_t today)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->team_id == 0u
            || rec->retained_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd < rec->retained_on_yyyymmdd
            || today == 0u) {
        return 1;
    }
    return today > rec->expires_on_yyyymmdd;
}
