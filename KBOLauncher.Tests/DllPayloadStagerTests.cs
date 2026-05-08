namespace KBOLauncher.Tests;

using Xunit;

public sealed class DllPayloadStagerTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-dll-stage-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void PrepareInjectableDllCopy_StagesUniquelyNamedDllAndCompanionFiles()
    {
        var sourceDir = Path.Combine(tempDir, "source");
        var runDir = Path.Combine(tempDir, "run_dlls");
        var logPath = Path.Combine(tempDir, "launcher.log");
        var dllPath = Path.Combine(sourceDir, "KBOFix.dll");
        Directory.CreateDirectory(Path.Combine(sourceDir, "assets", "fonts"));
        File.WriteAllBytes(dllPath, [1, 2, 3, 4]);
        File.WriteAllBytes(Path.Combine(sourceDir, "WebView2Loader.dll"), [5, 6, 7]);
        File.WriteAllText(Path.Combine(sourceDir, "assets", "fonts", "JejuGothic-Regular.ttf"), "font");

        var staged = global::DllPayloadStager.PrepareInjectableDllCopy(dllPath, runDir, logPath);

        Assert.StartsWith(Path.Combine(runDir, "KBOFix-"), staged, StringComparison.OrdinalIgnoreCase);
        Assert.EndsWith(".dll", staged, StringComparison.OrdinalIgnoreCase);
        Assert.Equal([1, 2, 3, 4], File.ReadAllBytes(staged));
        Assert.Equal([5, 6, 7], File.ReadAllBytes(Path.Combine(runDir, "WebView2Loader.dll")));
        Assert.Equal("font", File.ReadAllText(Path.Combine(runDir, "assets", "fonts", "JejuGothic-Regular.ttf")));
        Assert.Contains("dll_copy", File.ReadAllText(logPath));
        Assert.Contains("webview2_loader_copy", File.ReadAllText(logPath));
        Assert.Contains("assets_copy", File.ReadAllText(logPath));
    }

    [Fact]
    public void PrepareInjectableDllCopy_ThrowsWhenSourceDllIsMissing()
    {
        var missingPath = Path.Combine(tempDir, "missing", "KBOFix.dll");

        var ex = Assert.Throws<FileNotFoundException>(() =>
            global::DllPayloadStager.PrepareInjectableDllCopy(
                missingPath,
                Path.Combine(tempDir, "run_dlls"),
                Path.Combine(tempDir, "launcher.log")));

        Assert.Equal(Path.GetFullPath(missingPath), ex.FileName);
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
