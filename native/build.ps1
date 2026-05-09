param(
    [switch] $Clean
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $Root "bin"
$ObjDir = Join-Path $Root "obj"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

function Resolve-CommandPath {
    param(
        [string[]] $Candidates,

        [Parameter(Mandatory = $true)]
        [string] $Name
    )

    foreach ($Candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($Candidate)) {
            continue
        }

        $Command = Get-Command $Candidate -ErrorAction SilentlyContinue
        if ($Command) {
            return $Command.Source
        }

        if (Test-Path -LiteralPath $Candidate) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
    }

    throw "Could not find $Name. Install MinGW-w64 GCC, add gcc.exe to PATH, or set KBO_GCC to the full gcc.exe path."
}

function Resolve-WebView2Root {
    $Candidates = @()

    if (-not [string]::IsNullOrWhiteSpace($env:WEBVIEW2_NUGET_ROOT)) {
        $Candidates += $env:WEBVIEW2_NUGET_ROOT
    }

    $NuGetPackagesRoot = if (-not [string]::IsNullOrWhiteSpace($env:NUGET_PACKAGES)) {
        $env:NUGET_PACKAGES
    }
    else {
        Join-Path $env:USERPROFILE ".nuget\packages"
    }

    $PackageRoot = Join-Path $NuGetPackagesRoot "microsoft.web.webview2"
    if (Test-Path -LiteralPath $PackageRoot) {
        $Candidates += Get-ChildItem -LiteralPath $PackageRoot -Directory |
            Sort-Object {
                try {
                    [version]$_.Name
                }
                catch {
                    [version]"0.0"
                }
            } -Descending |
            ForEach-Object { $_.FullName }
    }

    foreach ($Candidate in $Candidates) {
        $Include = Join-Path $Candidate "build\native\include"
        $Loader = Join-Path $Candidate "runtimes\win-x64\native\WebView2Loader.dll"
        if ((Test-Path -LiteralPath $Include) -and (Test-Path -LiteralPath $Loader)) {
            return $Candidate
        }
    }

    throw "Could not find Microsoft.Web.WebView2 native files. Run 'dotnet add package Microsoft.Web.WebView2' or set WEBVIEW2_NUGET_ROOT to the package version directory."
}

function Get-MingwGccCandidates {
    $Candidates = @()

    if (-not [string]::IsNullOrWhiteSpace($env:KBO_GCC)) {
        $Candidates += $env:KBO_GCC
    }

    $WingetPackages = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path -LiteralPath $WingetPackages) {
        $Candidates += Get-ChildItem -LiteralPath $WingetPackages -Recurse -Filter "gcc.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\mingw(32|64)\\bin\\gcc\.exe$" } |
            Sort-Object FullName -Descending |
            ForEach-Object { $_.FullName }
    }

    $Candidates += @(
        "C:\msys64\ucrt64\bin\gcc.exe",
        "C:\msys64\mingw64\bin\gcc.exe",
        "C:\Program Files\mingw64\bin\gcc.exe",
        "C:\mingw64\bin\gcc.exe",
        "gcc.exe"
    )

    return $Candidates
}

function Convert-ToObjectStem {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourcePath
    )

    $Relative = Get-PathRelativeToRoot -Path $SourcePath
    return ($Relative -replace '[:\\/]+', '_' -replace '\.c$', '')
}

function Get-PathRelativeToRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $ResolvedRoot = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\') + '\'
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $RootUri = [Uri]$ResolvedRoot
    $PathUri = [Uri]$ResolvedPath
    return [Uri]::UnescapeDataString($RootUri.MakeRelativeUri($PathUri).ToString()).Replace('/', '\')
}

function Read-DependencyPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string] $DepPath
    )

    if (-not (Test-Path -LiteralPath $DepPath)) {
        return @()
    }

    $Text = Get-Content -LiteralPath $DepPath -Raw
    $Text = $Text -replace "\\\r?\n", " "
    $ColonMatch = [regex]::Match($Text, ":\s")
    if (-not $ColonMatch.Success) {
        return @()
    }

    $DependencyText = $Text.Substring($ColonMatch.Index + $ColonMatch.Length)
    return $DependencyText -split "\s+" |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { $_ -replace "\\ ", " " } |
        Where-Object { Test-Path -LiteralPath $_ }
}

function Test-ObjectOutOfDate {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourcePath,

        [Parameter(Mandatory = $true)]
        [string] $ObjectPath,

        [Parameter(Mandatory = $true)]
        [string] $DepPath
    )

    if (-not (Test-Path -LiteralPath $ObjectPath)) {
        return $true
    }

    if (-not (Test-Path -LiteralPath $DepPath)) {
        return $true
    }

    $ObjectTime = (Get-Item -LiteralPath $ObjectPath).LastWriteTimeUtc
    if ((Get-Item -LiteralPath $SourcePath).LastWriteTimeUtc -gt $ObjectTime) {
        return $true
    }

    foreach ($Dependency in Read-DependencyPaths -DepPath $DepPath) {
        if (-not (Test-Path -LiteralPath $Dependency)) {
            return $true
        }
        if ((Get-Item -LiteralPath $Dependency).LastWriteTimeUtc -gt $ObjectTime) {
            return $true
        }
    }

    return $false
}

function Test-LinkOutOfDate {
    param(
        [Parameter(Mandatory = $true)]
        [string] $OutputPath,

        [Parameter(Mandatory = $true)]
        [string[]] $ObjectPaths,

        [Parameter(Mandatory = $true)]
        [string] $LoaderPath
    )

    if (-not (Test-Path -LiteralPath $OutputPath)) {
        return $true
    }

    $OutputTime = (Get-Item -LiteralPath $OutputPath).LastWriteTimeUtc
    if ((Get-Item -LiteralPath $LoaderPath).LastWriteTimeUtc -gt $OutputTime) {
        return $true
    }

    foreach ($ObjectPath in $ObjectPaths) {
        if ((Get-Item -LiteralPath $ObjectPath).LastWriteTimeUtc -gt $OutputTime) {
            return $true
        }
    }

    return $false
}

function Convert-ToGccResponsePath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    return (Resolve-Path -LiteralPath $Path).Path.Replace('\', '/')
}

$Gcc = Resolve-CommandPath -Name "gcc.exe" -Candidates @(
    Get-MingwGccCandidates
)

$WebView2Root = Resolve-WebView2Root
$WebView2Include = Join-Path $WebView2Root "build\native\include"
$WebView2Loader = Join-Path $WebView2Root "runtimes\win-x64\native\WebView2Loader.dll"

if ($Clean) {
    Remove-Item -LiteralPath $ObjDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $OutDir "KBOFix.dll") -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null
}

Copy-Item -LiteralPath $WebView2Loader -Destination (Join-Path $OutDir "WebView2Loader.dll") -Force

Write-Host "GCC: $Gcc"
Write-Host "WebView2: $WebView2Root"

$NativeSources = @(
    Join-Path $Root "KBOFix.c"
)
$NativeSources += Get-ChildItem -LiteralPath (Join-Path $Root "src") -Recurse -Filter "*.c" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }

$Objects = @()
$CompiledCount = 0
foreach ($Source in $NativeSources) {
    $Stem = Convert-ToObjectStem -SourcePath $Source
    $ObjectPath = Join-Path $ObjDir "$Stem.o"
    $DepPath = Join-Path $ObjDir "$Stem.d"
    $Objects += $ObjectPath

    if (-not (Test-ObjectOutOfDate -SourcePath $Source -ObjectPath $ObjectPath -DepPath $DepPath)) {
        continue
    }

    Write-Host "Compiling $(Get-PathRelativeToRoot -Path $Source)"
    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $GccOutput = & $Gcc -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
        -isystem $WebView2Include `
        -MMD -MP -MF $DepPath `
        -c $Source `
        -o $ObjectPath 2>&1
    $GccExitCode = $LASTEXITCODE
    $ErrorActionPreference = $PreviousErrorActionPreference
    $GccOutput | ForEach-Object { Write-Host $_ }
    if ($GccExitCode -ne 0) {
        throw "Failed to compile $Source"
    }
    $CompiledCount += 1
}

$DllPath = Join-Path $OutDir "KBOFix.dll"
if (Test-LinkOutOfDate -OutputPath $DllPath -ObjectPaths $Objects -LoaderPath $WebView2Loader) {
    Write-Host "Linking KBOFix.dll ($($Objects.Count) objects, $CompiledCount compiled)"
    $ResponsePath = Join-Path $ObjDir "KBOFix.link.rsp"
    $ResponseArgs = @(
        "-shared"
        "-o"
        "`"$($DllPath.Replace('\', '/'))`""
    )
    $ResponseArgs += $Objects | ForEach-Object { "`"$(Convert-ToGccResponsePath -Path $_)`"" }
    $ResponseArgs += @(
        "-lgdi32"
        "-lmsimg32"
        "-lole32"
        "-luuid"
        "-lshell32"
        "-lwindowscodecs"
    )
    Set-Content -LiteralPath $ResponsePath -Value ($ResponseArgs -join " ") -Encoding ASCII
    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $GccOutput = & $Gcc -shared `
        "@$ResponsePath" 2>&1
    $GccExitCode = $LASTEXITCODE
    $ErrorActionPreference = $PreviousErrorActionPreference
    $GccOutput | ForEach-Object { Write-Host $_ }
    if ($GccExitCode -ne 0) {
        throw "Failed to link KBOFix.dll"
    }
}
else {
    Write-Host "KBOFix.dll is up to date ($($Objects.Count) objects, $CompiledCount compiled)"
}

if ($CompiledCount -eq 0) {
    Write-Host "No native sources changed."
}

Write-Host "Built $DllPath"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$LauncherOutputDirs = @(
    (Join-Path $RepoRoot "bin\Release\net8.0"),
    (Join-Path $RepoRoot "bin\Debug\net8.0"),
    (Join-Path $RepoRoot "src\KBOLauncher\bin\Release\net8.0"),
    (Join-Path $RepoRoot "src\KBOLauncher\bin\Debug\net8.0")
)
foreach ($LauncherOutputDir in $LauncherOutputDirs) {
    if (Test-Path $LauncherOutputDir) {
        Copy-Item -LiteralPath $DllPath -Destination (Join-Path $LauncherOutputDir "KBOFix.dll") -Force
        $LoaderPath = Join-Path $OutDir "WebView2Loader.dll"
        if (Test-Path $LoaderPath) {
            Copy-Item -LiteralPath $LoaderPath -Destination (Join-Path $LauncherOutputDir "WebView2Loader.dll") -Force
        }
        $AssetsPath = Join-Path $OutDir "assets"
        if (Test-Path $AssetsPath) {
            Copy-Item -LiteralPath $AssetsPath -Destination (Join-Path $LauncherOutputDir "assets") -Recurse -Force
        }
        Write-Host "Synced native payload to $LauncherOutputDir"
    }
}
