$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $Root "bin"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

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

$Gcc = Resolve-CommandPath -Name "gcc.exe" -Candidates @(
    Get-MingwGccCandidates
)

$WebView2Root = Resolve-WebView2Root
$WebView2Include = Join-Path $WebView2Root "build\native\include"
$WebView2Loader = Join-Path $WebView2Root "runtimes\win-x64\native\WebView2Loader.dll"

Copy-Item -LiteralPath $WebView2Loader -Destination (Join-Path $OutDir "WebView2Loader.dll") -Force

Write-Host "GCC: $Gcc"
Write-Host "WebView2: $WebView2Root"

& $Gcc -shared -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $WebView2Include `
    -o (Join-Path $OutDir "KBOFix.dll") `
    (Join-Path $Root "KBOFix.c") `
    (Join-Path $Root "src\core\core_log.c") `
    (Join-Path $Root "src\core\core_text_date.c") `
    (Join-Path $Root "src\core\core_flags\json_bool_parser.c") `
    (Join-Path $Root "src\core\core_flags\flag_key.c") `
    (Join-Path $Root "src\core\core_flags\localappdata_reader.c") `
    (Join-Path $Root "src\core\core_flags\flags_api.c") `
    -lgdi32 `
    -lmsimg32 `
    -lole32 `
    -luuid `
    -lshell32 `
    -lwindowscodecs
if ($LASTEXITCODE -ne 0) {
    throw "Failed to build KBOFix.dll"
}

Write-Host "Built $(Join-Path $OutDir 'KBOFix.dll')"
