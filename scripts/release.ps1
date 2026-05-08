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
Copy-Item (Join-Path $RepoRoot "assets") (Join-Path $Dist "assets") -Recurse -Force

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
    "data\seeds\asian_games_schedule_seed.csv",
    "data\seeds\college_reputation_seed.csv",
    "data\seeds\fa_rules.json",
    "data\seeds\foreign_replacement_players_seed.csv",
    "data\seeds\high_school_reputation_seed.csv",
    "data\seeds\military_service_seed.csv"
)

foreach ($RequiredFile in $RequiredFiles) {
    $Path = Join-Path $Dist $RequiredFile
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Release payload missing required file: $RequiredFile"
    }
}

$LeagueId = (Get-Content -LiteralPath (Join-Path $Dist "kbo_league_id.txt") -Raw).Trim()
if ($LeagueId -ne "100") {
    throw "Release payload has unexpected kbo_league_id.txt value: '$LeagueId'"
}

$FileCount = (Get-ChildItem $Dist -Recurse -File).Count
Write-Host "==> Done: $FileCount files in dist\"
