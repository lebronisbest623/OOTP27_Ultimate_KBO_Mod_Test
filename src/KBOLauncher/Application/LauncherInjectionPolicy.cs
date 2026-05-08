internal enum LauncherInjectionMode
{
    None,
    AttachExisting,
    LaunchThenWaitForMarkedSave,
    PresaveAllstarBootstrap,
}

internal sealed record LauncherInjectionDecision(
    LauncherInjectionMode Mode,
    bool ShouldInject,
    bool RequiresSupportedBuild,
    bool RequiresRosterMarkerBeforeInjection,
    bool AllowsPresaveInjection,
    string Reason);

internal static class LauncherInjectionPolicy
{
    public static LauncherInjectionDecision Decide(
        bool isDefaultRun,
        string? dllPath,
        bool supportedOotpBuild,
        bool attachRequested,
        bool allstarBootstrapRequested)
    {
        if (dllPath is null)
        {
            return new(
                LauncherInjectionMode.None,
                ShouldInject: false,
                RequiresSupportedBuild: false,
                RequiresRosterMarkerBeforeInjection: false,
                AllowsPresaveInjection: false,
                "dll_not_requested");
        }

        if (!supportedOotpBuild)
        {
            return new(
                LauncherInjectionMode.None,
                ShouldInject: false,
                RequiresSupportedBuild: true,
                RequiresRosterMarkerBeforeInjection: false,
                AllowsPresaveInjection: false,
                "unsupported_ootp_build");
        }

        if (attachRequested)
        {
            return new(
                LauncherInjectionMode.AttachExisting,
                ShouldInject: true,
                RequiresSupportedBuild: true,
                RequiresRosterMarkerBeforeInjection: true,
                AllowsPresaveInjection: false,
                isDefaultRun ? "default_attach_existing" : "explicit_attach_existing");
        }

        if (allstarBootstrapRequested)
        {
            return new(
                LauncherInjectionMode.PresaveAllstarBootstrap,
                ShouldInject: true,
                RequiresSupportedBuild: true,
                RequiresRosterMarkerBeforeInjection: false,
                AllowsPresaveInjection: true,
                "presave_allstar_bootstrap");
        }

        return new(
            LauncherInjectionMode.LaunchThenWaitForMarkedSave,
            ShouldInject: true,
            RequiresSupportedBuild: true,
            RequiresRosterMarkerBeforeInjection: true,
            AllowsPresaveInjection: false,
            "launch_wait_for_marked_save");
    }
}
