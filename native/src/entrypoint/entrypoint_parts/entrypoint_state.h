static HANDLE g_kbo_process_instance_mutex = NULL;
static volatile LONG g_kbo_sangmu_fa_hooks_install_started = 0;
static volatile LONG g_kbo_full_runtime_install_started = 0;
static volatile LONG g_kbo_full_runtime_marker_wait_started = 0;

#define KBO_REQUIRED_ROSTER_MARKER_URL "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO"

typedef struct KboSangmuFaHookInstallRequest {
    int enable_signability;
    int enable_offer;
    int enable_legacy;
} KboSangmuFaHookInstallRequest;
