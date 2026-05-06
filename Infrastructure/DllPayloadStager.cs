using static LauncherPaths;
using static LauncherLog;

internal static class DllPayloadStager
{
    public static string PrepareInjectableDllCopy(string dllPath, string logPath)
    {
        var fullDllPath = Path.GetFullPath(dllPath);
        if (!File.Exists(fullDllPath))
        {
            throw new FileNotFoundException("Native DLL was not found.", fullDllPath);
        }
    
        var runDllDir = GetKboLocalDataPath("run_dlls");
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
    
        Log(logPath, $"dll_copy source={fullDllPath} target={copyPath}");
        return copyPath;
    }
}
