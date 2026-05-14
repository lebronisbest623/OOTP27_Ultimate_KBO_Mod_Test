namespace KBOLauncher.Tests;

using System.ComponentModel;
using FluentAssertions;
using Xunit;

public sealed class LauncherLaunchFlowTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launch-flow-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void PrintUnsupportedBuildInjectionWarning_IncludesActionableBuildDetails()
    {
        var originalError = Console.Error;
        using var writer = new StringWriter();
        Console.SetError(writer);
        try
        {
            global::LauncherGuardStatus.PrintUnsupportedBuildInjectionWarning(
                new global::OotpBuildInfo(true, 0x11111111u, 0x22222222u, null));
        }
        finally
        {
            Console.SetError(originalError);
        }

        var text = writer.ToString();
        text.Should().Contain("KBOFix injection disabled");
        text.Should().Contain("timestamp=0x11111111");
        text.Should().Contain("size_of_image=0x22222222");
        text.Should().Contain("Update the launcher/native supported-build list");
    }

    [Fact]
    public void InjectDll_WhenOpenProcessFails_LogsPidDllPathAndWin32Error()
    {
        var dllPath = Path.Combine(tempDir, "KBOFix.dll");
        var logPath = Path.Combine(tempDir, "launcher.log");
        Directory.CreateDirectory(tempDir);
        File.WriteAllBytes(dllPath, [1]);

        var act = () => global::DllInjector.InjectDll(-1, dllPath, logPath);
        var ex = act.Should().Throw<Win32Exception>().Which;

        var log = File.ReadAllText(logPath);
        log.Should().Contain("inject_start pid=-1");
        log.Should().Contain("inject_failed pid=-1");
        log.Should().Contain(Path.GetFullPath(dllPath));
        ex.Message.Should().Contain("OpenProcess failed win32=");
        log.Should().Contain("OpenProcess failed win32=");
    }

    public void Dispose()
    {
        try
        {
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, recursive: true);
            }
        }
        catch (IOException)
        {
        }
    }
}
