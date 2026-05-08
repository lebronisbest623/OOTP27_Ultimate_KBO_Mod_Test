using static LauncherPaths;
using static LauncherLog;

internal static class DllPayloadStager
{
    public static string PrepareInjectableDllCopy(string dllPath, string logPath)
    {
        return PrepareInjectableDllCopy(dllPath, GetKboLocalDataPath("run_dlls"), logPath);
    }

    internal static string PrepareInjectableDllCopy(string dllPath, string runDllDir, string logPath)
    {
        var fullDllPath = Path.GetFullPath(dllPath);
        if (!File.Exists(fullDllPath))
        {
            throw new FileNotFoundException("Native DLL was not found.", fullDllPath);
        }
    
        Directory.CreateDirectory(runDllDir);
    
        var extension = Path.GetExtension(fullDllPath);
        var stem = Path.GetFileNameWithoutExtension(fullDllPath);
        var copyPath = Path.Combine(
            runDllDir,
            $"{stem}-{DateTimeOffset.Now:yyyyMMddHHmmssfff}-{Environment.ProcessId}{extension}");
    
        File.Copy(fullDllPath, copyPath, overwrite: false);
    
        var loaderSource = Path.Combine(Path.GetDirectoryName(fullDllPath) ?? string.Empty, "WebView2Loader.dll");
        if (File.Exists(loaderSource))
        {
            var loaderTarget = Path.Combine(runDllDir, "WebView2Loader.dll");
            File.Copy(loaderSource, loaderTarget, overwrite: true);
            Log(logPath, $"webview2_loader_copy source={loaderSource} target={loaderTarget}");
        }

        var assetSource = Path.Combine(Path.GetDirectoryName(fullDllPath) ?? string.Empty, "assets");
        if (Directory.Exists(assetSource))
        {
            var assetTarget = Path.Combine(runDllDir, "assets");
            CopyDirectory(assetSource, assetTarget);
            Log(logPath, $"assets_copy source={assetSource} target={assetTarget}");
        }
    
        Log(logPath, $"dll_copy source={fullDllPath} target={copyPath}");
        return copyPath;
    }

    private static void CopyDirectory(string sourceDir, string targetDir)
    {
        Directory.CreateDirectory(targetDir);
        foreach (var file in Directory.EnumerateFiles(sourceDir, "*", SearchOption.TopDirectoryOnly))
        {
            File.Copy(file, Path.Combine(targetDir, Path.GetFileName(file)), overwrite: true);
        }

        foreach (var directory in Directory.EnumerateDirectories(sourceDir, "*", SearchOption.TopDirectoryOnly))
        {
            CopyDirectory(directory, Path.Combine(targetDir, Path.GetFileName(directory)));
        }
    }
}
