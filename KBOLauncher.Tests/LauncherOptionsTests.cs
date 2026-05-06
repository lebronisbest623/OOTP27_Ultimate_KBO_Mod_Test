namespace KBOLauncher.Tests;

using Xunit;

public sealed class LauncherOptionsTests
{
    [Fact]
    public void Parse_RecognizesLauncherFlagsAndOotpArguments()
    {
        var options = global::LauncherOptions.Parse([
            "--ootp", @"D:\Games\ootp27.exe",
            "--dll", @"C:\mods\KBOFix.dll",
            "--attach-existing",
            "--disable-foreign-waiver-ai",
            "--",
            "no3d",
            "debug"
        ]);

        Assert.Equal(@"D:\Games\ootp27.exe", options.OotpPath);
        Assert.Equal(@"C:\mods\KBOFix.dll", options.DllPath);
        Assert.True(options.AttachExisting);
        Assert.False(options.EnableForeignWaiverAi);
        Assert.Equal(["no3d", "debug"], options.OotpArgs);
    }

    [Fact]
    public void Parse_RejectsAttachPidWithoutNumericValue()
    {
        var ex = Assert.Throws<ArgumentException>(() => global::LauncherOptions.Parse(["--attach-pid", "abc"]));
        Assert.Contains("--attach-pid", ex.Message);
    }

    [Fact]
    public void Parse_RecognizesDisableFlagsDryRunAndHelp()
    {
        var options = global::LauncherOptions.Parse([
            "--attach-pid", "1234",
            "--enable-foreign-waiver-ai",
            "--disable-single-division-allstar-events",
            "--dry-run",
            "--allow-second-instance",
            "--help"
        ]);

        Assert.Equal(1234, options.AttachPid);
        Assert.True(options.EnableForeignWaiverAi);
        Assert.False(options.EnableSingleDivisionAllstarEvents);
        Assert.True(options.DryRun);
        Assert.True(options.AllowSecondInstance);
        Assert.True(options.ShowHelp);
    }

    [Fact]
    public void Parse_RejectsUnknownArgument()
    {
        var ex = Assert.Throws<ArgumentException>(() => global::LauncherOptions.Parse(["--wat"]));
        Assert.Contains("Unknown argument", ex.Message);
    }
}
