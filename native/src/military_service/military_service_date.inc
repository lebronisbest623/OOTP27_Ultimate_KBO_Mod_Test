/* Military service date serial and current-date helpers. Included from native/src/military_service_loan.inc. */

static uint32_t kbo_date_serial(uint32_t year, uint32_t month, uint32_t day)
{
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    uint32_t y = year - 1u;
    uint32_t serial = y * 365u + y / 4u - y / 100u + y / 400u;
    serial += days_before_month[month - 1u];
    if (month > 2 && kbo_is_leap_year(year)) {
        serial++;
    }
    serial += day;
    return serial;
}

static uint32_t kbo_current_date_serial(void)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return 0;
    }
    return kbo_date_serial(year, month, day);
}


