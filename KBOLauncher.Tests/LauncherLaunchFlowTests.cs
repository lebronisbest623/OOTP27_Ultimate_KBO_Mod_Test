namespace KBOLauncher.Tests;

using System.ComponentModel;
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
        Assert.Contains("KBOFix injection disabled", text);
        Assert.Contains("timestamp=0x11111111", text);
        Assert.Contains("size_of_image=0x22222222", text);
        Assert.Contains("Update the launcher/native supported-build list", text);
    }

    [Fact]
    public void InjectDll_WhenOpenProcessFails_LogsPidDllPathAndWin32Error()
    {
        var dllPath = Path.Combine(tempDir, "KBOFix.dll");
        var logPath = Path.Combine(tempDir, "launcher.log");
        Directory.CreateDirectory(tempDir);
        File.WriteAllBytes(dllPath, [1]);

        var ex = Assert.Throws<Win32Exception>(() =>
            global::DllInjector.InjectDll(-1, dllPath, logPath));

        var log = File.ReadAllText(logPath);
        Assert.Contains("inject_start pid=-1", log);
        Assert.Contains("inject_failed pid=-1", log);
        Assert.Contains(Path.GetFullPath(dllPath), log);
        Assert.Contains("OpenProcess failed win32=", ex.Message);
        Assert.Contains("OpenProcess failed win32=", log);
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
