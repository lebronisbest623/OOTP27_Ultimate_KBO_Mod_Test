namespace KBOLauncher.Tests;

using FluentAssertions;
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

        decision.ShouldInject.Should().BeFalse();
        decision.Mode.Should().Be(global::LauncherInjectionMode.None);
        decision.RequiresSupportedBuild.Should().BeFalse();
        decision.Reason.Should().Be("dll_not_requested");
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

        decision.ShouldInject.Should().BeFalse();
        decision.RequiresSupportedBuild.Should().BeTrue();
        decision.AllowsPresaveInjection.Should().BeFalse();
        decision.Reason.Should().Be("unsupported_ootp_build");
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

        decision.ShouldInject.Should().BeTrue();
        decision.Mode.Should().Be(global::LauncherInjectionMode.AttachExisting);
        decision.RequiresRosterMarkerBeforeInjection.Should().BeTrue();
        decision.AllowsPresaveInjection.Should().BeFalse();
        decision.Reason.Should().Be(expectedReason);
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

        decision.ShouldInject.Should().BeTrue();
        decision.Mode.Should().Be(global::LauncherInjectionMode.LaunchThenWaitForMarkedSave);
        decision.RequiresRosterMarkerBeforeInjection.Should().BeTrue();
        decision.AllowsPresaveInjection.Should().BeFalse();
    }

    [Fact]
    public void Decide_AllstarBootstrapAllowsPresaveInjection()
    {
        var decision = global::LauncherInjectionPolicy.Decide(
            isDefaultRun: true,
            dllPath: @"C:\mods\KBOFix.dll",
            supportedOotpBuild: true,
            attachRequested: false,
            allstarBootstrapRequested: true);

        decision.ShouldInject.Should().BeTrue();
        decision.Mode.Should().Be(global::LauncherInjectionMode.PresaveAllstarBootstrap);
        decision.RequiresRosterMarkerBeforeInjection.Should().BeFalse();
        decision.AllowsPresaveInjection.Should().BeTrue();
        decision.Reason.Should().Be("presave_allstar_bootstrap");
    }
}
