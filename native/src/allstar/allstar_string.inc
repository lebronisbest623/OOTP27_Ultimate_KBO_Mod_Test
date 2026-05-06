/* All-Star OOTP string helper resolution. */

static OotpStringAssignFn get_ootp_string_assign_fn(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    void* assign_fn = (void*)((uintptr_t)exe + OOTP27_PISD_STRING_ASSIGN_RVA);
    if (!memory_range_readable(assign_fn, 16)) {
        return NULL;
    }
    return (OotpStringAssignFn)assign_fn;
}
