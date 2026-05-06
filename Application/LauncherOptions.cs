record LauncherOptions(
    string? OotpPath,
    string? DllPath,
    int? AttachPid,
    bool AttachExisting,
    bool? EnableForeignWaiverAi,
    bool? EnableSingleDivisionAllstarEvents,
    bool DryRun,
    bool AllowSecondInstance,
    bool ShowHelp,
    IReadOnlyList<string> OotpArgs)
{
    public static LauncherOptions Parse(string[] args)
    {
        string? ootpPath = null;
        string? dllPath = null;
        int? attachPid = null;
        var attachExisting = false;
        bool? enableForeignWaiverAi = null;
        bool? enableSingleDivisionAllstarEvents = null;
        var dryRun = false;
        var allowSecondInstance = false;
        var showHelp = false;
        var ootpArgs = new List<string>();

        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];
            switch (arg)
            {
                case "--ootp":
                    if (++i >= args.Length)
                        throw new ArgumentException("--ootp requires a path");
                    ootpPath = args[i];
                    break;
                case "--dll":
                    if (++i >= args.Length)
                        throw new ArgumentException("--dll requires a path");
                    dllPath = args[i];
                    break;
                case "--attach-pid":
                    if (++i >= args.Length || !int.TryParse(args[i], out var parsedPid))
                        throw new ArgumentException("--attach-pid requires a numeric process id");
                    attachPid = parsedPid;
                    break;
                case "--attach-existing":
                    attachExisting = true;
                    break;
                case "--enable-foreign-waiver-ai":
                    enableForeignWaiverAi = true;
                    break;
                case "--disable-foreign-waiver-ai":
                    enableForeignWaiverAi = false;
                    break;
                case "--enable-single-division-allstar-events":
                    enableSingleDivisionAllstarEvents = true;
                    break;
                case "--disable-single-division-allstar-events":
                    enableSingleDivisionAllstarEvents = false;
                    break;
                case "--dry-run":
                    dryRun = true;
                    break;
                case "--allow-second-instance":
                    allowSecondInstance = true;
                    break;
                case "-h":
                case "--help":
                    showHelp = true;
                    break;
                case "--":
                    ootpArgs.AddRange(args.Skip(i + 1));
                    i = args.Length;
                    break;
                default:
                    throw new ArgumentException($"Unknown argument: {arg}");
            }
        }

        return new LauncherOptions(
            ootpPath,
            dllPath,
            attachPid,
            attachExisting,
            enableForeignWaiverAi,
            enableSingleDivisionAllstarEvents,
            dryRun,
            allowSecondInstance,
            showHelp,
            ootpArgs);
    }

    public static void PrintHelp()
    {
        Console.WriteLine("""
        KBOLauncher

        Usage:
          KBOLauncher
          KBOLauncher [--ootp path] [--allow-second-instance] [--] [ootp args...]
          KBOLauncher --attach-existing --dll path
          KBOLauncher --attach-pid pid --dll path

        Options:
          --ootp PATH                  Explicit ootp27.exe path.
          --dll PATH                   Native DLL to load into OOTP.
          --attach-existing            Attach to the single running OOTP process.
          --attach-pid PID             Attach to a specific OOTP process id.
          --enable-foreign-waiver-ai    Enable foreign waiver assistant log.
          --disable-foreign-waiver-ai   Disable foreign waiver assistant log.
          --enable-single-division-allstar-events
                                       Enable single-division all-star event creation.
          --disable-single-division-allstar-events
                                       Disable single-division all-star event creation.
          --dry-run                    Resolve paths and detect existing process without launching.
          --allow-second-instance      Launch even if an OOTP process is already running.
          --                           Pass remaining arguments to OOTP.

        Safety:
          KBOFix injection is disabled unless ootp27.exe matches a verified build.
          KBOFix injection also requires the currently opened OOTP .lg save description.txt
          to contain the official roster marker URL.
        """);
    }
}
