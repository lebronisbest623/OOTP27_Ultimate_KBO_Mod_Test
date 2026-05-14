
using Polly;
using Polly.Retry;

internal static class LauncherLog
{
    private static readonly ResiliencePipeline WriteRetry = new ResiliencePipelineBuilder()
        .AddRetry(new RetryStrategyOptions
        {
            ShouldHandle = new PredicateBuilder().Handle<IOException>(),
            MaxRetryAttempts = 4,
            Delay = TimeSpan.FromMilliseconds(25),
            BackoffType = DelayBackoffType.Constant,
        })
        .Build();

    public static void Log(string path, string message)
    {
        var line = $"[{DateTimeOffset.Now:O}] {message}{Environment.NewLine}";
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);

        WriteRetry.Execute(() =>
        {
            using var stream = new FileStream(path, FileMode.Append, FileAccess.Write, FileShare.ReadWrite);
            using var writer = new StreamWriter(stream);
            writer.Write(line);
        });
    }
}
