$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$TestSrc = Join-Path $PSScriptRoot "test_main.c"
$TestExe = Join-Path $PSScriptRoot "tests.exe"

function Resolve-Gcc {
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

    throw "Could not find gcc.exe. Install MinGW-w64 GCC, add gcc.exe to PATH, or set KBO_GCC to the full gcc.exe path."
}

$Gcc = Resolve-Gcc
Write-Host "GCC: $Gcc"

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $TestExe `
    $TestSrc `
    (Join-Path $Root "src\allstar\allstar_csv_parse.c") `
    (Join-Path $Root "src\foreign\foreign_csv_parse.c") `
    (Join-Path $Root "src\foreign\replacement_seed\foreign_replacement_seed_parse.c") `
    (Join-Path $Root "src\military_service\military_service_date.c") `
    (Join-Path $Root "src\core\core_text_date.c") `
    (Join-Path $Root "src\core\core_sql_escape.c") `
    (Join-Path $Root "src\core\core_flags\flag_key.c") `
    (Join-Path $Root "src\core\core_flags\json_bool_parser.c") `
    (Join-Path $Root "src\core\core_flags\localappdata_reader.c")
if ($LASTEXITCODE -ne 0) {
    throw "Build failed"
}

& $TestExe
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed"
}

Write-Host "All native tests passed."
