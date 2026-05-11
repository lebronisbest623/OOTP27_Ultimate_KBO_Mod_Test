param(
    [switch]$RebuildOptimizer
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Dist = Join-Path $RepoRoot "dist"

if (Test-Path -LiteralPath $Dist) {
    Remove-Item -LiteralPath $Dist -Recurse -Force
}
New-Item -ItemType Directory -Path $Dist | Out-Null

Write-Host "==> Building native DLL..."
& powershell -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "native\build.ps1")
if ($LASTEXITCODE -ne 0) { throw "Native build failed" }

Write-Host "==> Building optimizer tool..."
$OptimizerSource = Join-Path $RepoRoot "tools\kbo_optimizer.py"
$PyInstallerWork = Join-Path $RepoRoot "obj\pyinstaller"
$OptimizerExe = Join-Path $RepoRoot "tools\kbo_optimizer.exe"
$LegacyOptimizerExe = Join-Path $RepoRoot "tools\amateur_assignment_optimizer.exe"

$OptimizerNeedsBuild = $RebuildOptimizer `
    -or -not (Test-Path -LiteralPath $OptimizerExe -PathType Leaf) `
    -or ((Get-Item -LiteralPath $OptimizerExe).LastWriteTimeUtc -lt (Get-Item -LiteralPath $OptimizerSource).LastWriteTimeUtc)

if ($OptimizerNeedsBuild) {
    if (Test-Path -LiteralPath $OptimizerExe) {
        Remove-Item -LiteralPath $OptimizerExe -Force
    }
    if (Test-Path -LiteralPath $LegacyOptimizerExe) {
        Remove-Item -LiteralPath $LegacyOptimizerExe -Force
    }
    New-Item -ItemType Directory -Path $PyInstallerWork -Force | Out-Null
    $PyInstallerExcludes = @(
        "boto3",
        "botocore",
        "fsspec",
        "IPython",
        "llvmlite",
        "lxml",
        "PyQt5",
        "PyQt6",
        "PySide6",
        "numba",
        "matplotlib",
        "openpyxl",
        "PIL",
        "pyarrow",
        "pygame",
        "pytest",
        "sqlalchemy",
        "scipy",
        "tkinter",
        "zmq"
    )
    $PyInstallerArgs = @(
        "--onefile",
        "--name", "kbo_optimizer",
        "--distpath", (Join-Path $RepoRoot "tools"),
        "--workpath", $PyInstallerWork,
        "--specpath", $PyInstallerWork
    )
    foreach ($Module in $PyInstallerExcludes) {
        $PyInstallerArgs += @("--exclude-module", $Module)
    }
    $OrToolsLibDir = & python -c "import ortools, pathlib; print(pathlib.Path(ortools.__file__).parent / '.libs')"
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($OrToolsLibDir)) {
        throw "Could not locate OR-Tools native library directory"
    }
    foreach ($DllName in @("abseil_dll.dll", "libprotobuf.dll", "ortools.dll")) {
        $DllPath = Join-Path $OrToolsLibDir $DllName
        if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
            throw "Required OR-Tools native DLL missing: $DllPath"
        }
        $PyInstallerArgs += @("--add-binary", "$DllPath;.")
    }
    $PyInstallerArgs += $OptimizerSource
    & pyinstaller @PyInstallerArgs
    if ($LASTEXITCODE -ne 0) { throw "Optimizer tool build failed" }
} else {
    Write-Host "==> Optimizer tool is up to date; skipping PyInstaller. Use -RebuildOptimizer to force."
}
if (-not (Test-Path -LiteralPath $OptimizerExe -PathType Leaf)) {
    throw "Optimizer tool build did not produce expected exe: $OptimizerExe"
}

Write-Host "==> Publishing launcher..."
& dotnet publish (Join-Path $RepoRoot "src\KBOLauncher\KBOLauncher.csproj") `
    -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o $Dist
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed" }

Get-ChildItem $Dist -File | Where-Object { $_.Extension -in ".pdb", ".xml" } | Remove-Item -Force

Write-Host "==> Copying native files..."
Copy-Item (Join-Path $RepoRoot "native\bin\KBOFix.dll") $Dist -Force
Copy-Item (Join-Path $RepoRoot "native\bin\WebView2Loader.dll") $Dist -Force
Copy-Item (Join-Path $RepoRoot "native\kbo_league_id.txt") $Dist -Force

Write-Host "==> Copying UI assets..."
$DistAssets = Join-Path $Dist "assets"
if (Test-Path -LiteralPath $DistAssets) {
    Remove-Item -LiteralPath $DistAssets -Recurse -Force
}
New-Item -ItemType Directory -Path $DistAssets | Out-Null
Copy-Item (Join-Path $RepoRoot "assets\*") $DistAssets -Recurse -Force

Write-Host "==> Copying tool payloads..."
$DistTools = Join-Path $Dist "tools"
if (Test-Path -LiteralPath $DistTools) {
    Remove-Item -LiteralPath $DistTools -Recurse -Force
}
New-Item -ItemType Directory -Path $DistTools | Out-Null
Copy-Item $OptimizerExe $DistTools -Force
Copy-Item $OptimizerSource $DistTools -Force

Write-Host "==> Validating release payload..."
$RequiredFiles = @(
    "KBOLauncher.exe",
    "KBOFix.dll",
    "WebView2Loader.dll",
    "kbo_league_id.txt",
    "assets\fonts\JejuGothic-Regular.ttf",
    "assets\fonts\JejuGothic-OFL.txt",
    "assets\icons\github-mark.png",
    "data\seeds\allstar_teams.csv",
    "data\seeds\asian_games_projected_hosts.csv",
    "data\seeds\asian_games_schedule_seed.csv",
    "data\seeds\college_reputation_seed.csv",
    "data\seeds\fa_rules.json",
    "data\seeds\foreign_replacement_players_seed.csv",
    "data\seeds\high_school_reputation_seed.csv",
    "data\seeds\military_service_seed.csv",
    "tools\kbo_optimizer.exe",
    "tools\kbo_optimizer.py"
)
& powershell -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "tests\release\verify-release-artifact.ps1") -RepoRoot $RepoRoot -Dist $Dist
if ($LASTEXITCODE -ne 0) { throw "Release payload validation failed" }

$FileCount = (Get-ChildItem $Dist -Recurse -File).Count
Write-Host "==> Done: $FileCount files in dist\"
