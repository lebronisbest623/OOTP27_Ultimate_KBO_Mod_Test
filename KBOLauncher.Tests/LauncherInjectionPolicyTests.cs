namespace KBOLauncher.Tests;

using Xunit;

public sealed class LauncherInjectionPolicyTests
{
    [Fact]
    public void Decide_DisablesInjectionWhenDllIsNotRequested()
    {
        var decision = global::LauncherInjectionPolicy.Decide(
            isDefaultRun: true,
            dllPath: null,
            supportedOotpBuild: true,
            attachRequested: false,
            allstarBootstrapRequested: false);

        Assert.False(decision.ShouldInject);
        Assert.Equal(global::LauncherInjectionMode.None, decision.Mode);
        Assert.False(decision.RequiresSupportedBuild);
        Assert.Equal("dll_not_requested", decision.Reason);
    }

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void Decide_BlocksAllInjectionWhenBuildIsUnsupported(bool attachRequested)
    {
        var decision = global::LauncherInjectionPolicy.Decide(
            isDefaultRun: false,
            dllPath: @"C:\mods\KBOFix.dll",
            supportedOotpBuild: false,
            attachRequested: attachRequested,
            allstarBootstrapRequested: true);

        Assert.False(decision.ShouldInject);
        Assert.True(decision.RequiresSupportedBuild);
        Assert.False(decision.AllowsPresaveInjection);
        Assert.Equal("unsupported_ootp_build", decision.Reason);
    }

    [Theory]
    [InlineData(true, "default_attach_existing")]
    [InlineData(false, "explicit_attach_existing")]
    public void Decide_AttachRequiresRosterMarkerBeforeInjection(bool isDefaultRun, string expectedReason)
    {
        var decision = global::LauncherInjectionPolicy.Decide(
            isDefaultRun: isDefaultRun,
            dllPath: @"C:\mods\KBOFix.dll",
            supportedOotpBuild: true,
            attachRequested: true,
            allstarBootstrapRequested: true);

        Assert.True(decision.ShouldInject);
        Assert.Equal(global::LauncherInjectionMode.AttachExisting, decision.Mode);
        Assert.True(decision.RequiresRosterMarkerBeforeInjection);
        Assert.False(decision.AllowsPresaveInjection);
        Assert.Equal(expectedReason, decision.Reason);
    }

    [Fact]
    public void Decide_LaunchWithoutBootstrapStillRequiresRosterMarkerBeforeInjection()
    {
        var decision = global::LauncherInjectionPolicy.Decide(
            isDefaultRun: false,
            dllPath: @"C:\mods\KBOFix.dll",
            supportedOotpBuild: true,
            attachRequested: false,
            allstarBootstrapRequested: false);

        Assert.True(decision.ShouldInject);
        Assert.Equal(global::LauncherInjectionMode.LaunchThenWaitForMarkedSave, decision.Mode);
        Assert.True(decision.RequiresRosterMarkerBeforeInjection);
        Assert.False(decision.AllowsPresaveInjection);
    }

    [Fact]
    public void Decide_AllstarBootstrapStillWaitsForRosterMarker()
    {
        var decision = global::LauncherInjectionPolicy.Decide(
            isDefaultRun: true,
            dllPath: @"C:\mods\KBOFix.dll",
            supportedOotpBuild: true,
            attachRequested: false,
            allstarBootstrapRequested: true);

        Assert.True(decision.ShouldInject);
        Assert.Equal(global::LauncherInjectionMode.LaunchThenWaitForMarkedSave, decision.Mode);
        Assert.True(decision.RequiresRosterMarkerBeforeInjection);
        Assert.False(decision.AllowsPresaveInjection);
        Assert.Equal("presave_allstar_bootstrap_retired", decision.Reason);
    }
}
