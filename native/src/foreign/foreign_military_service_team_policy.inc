/* Military-service team FA policy helpers. */

#define KBO_DEFAULT_SANGMU_TEAM_ID 21u

static volatile LONG g_kbo_military_policy_sang_team_id = (LONG)KBO_DEFAULT_SANGMU_TEAM_ID;
static volatile LONG g_kbo_military_policy_override_loaded = 0;

static void kbo_load_military_service_team_policy_override_once(void)
{
    if (InterlockedCompareExchange(&g_kbo_military_policy_override_loaded, 1, 0) != 0) {
        return;
    }

    char local[MAX_PATH] = {0};
    DWORD local_len = GetEnvironmentVariableA("LOCALAPPDATA", local, (DWORD)sizeof(local));
    if (local_len == 0 || local_len >= sizeof(local) || local[0] == '\0') {
        append_logf("KBO military FA team policy fixed sangmu=%u source=default", KBO_DEFAULT_SANGMU_TEAM_ID);
        return;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\kbo_sangmu_team_id.txt", local);
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        append_logf("KBO military FA team policy fixed sangmu=%u source=default", KBO_DEFAULT_SANGMU_TEAM_ID);
        return;
    }

    char buffer[64] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[read] = '\0';

    unsigned long value = strtoul(buffer, NULL, 10);
    if (value > 0ul && value <= 1000000ul) {
        InterlockedExchange(&g_kbo_military_policy_sang_team_id, (LONG)value);
        append_logf("KBO military FA team policy fixed sangmu=%lu source=%s", value, path);
    } else {
        append_logf("KBO military FA team policy override ignored value=%s source=%s", buffer, path);
    }
}

static int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    uint32_t cached_sang_id = (uint32_t)InterlockedCompareExchange(&g_kbo_military_policy_sang_team_id, 0, 0);

    return team_id == cached_sang_id;
}
