$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $Root "bin"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$Gcc = "C:\Users\user\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe"
if (-not (Test-Path -LiteralPath $Gcc)) {
    $Gcc = "gcc.exe"
}

$WebView2Root = Join-Path $env:USERPROFILE ".nuget\packages\microsoft.web.webview2\1.0.3065.39"
$WebView2Include = Join-Path $WebView2Root "build\native\include"
$WebView2Loader = Join-Path $WebView2Root "runtimes\win-x64\native\WebView2Loader.dll"
if (Test-Path -LiteralPath $WebView2Loader) {
    Copy-Item -LiteralPath $WebView2Loader -Destination (Join-Path $OutDir "WebView2Loader.dll") -Force
}

& $Gcc -shared -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $WebView2Include `
    -o (Join-Path $OutDir "KBOFix.dll") `
    (Join-Path $Root "KBOFix.c") `
    -lgdi32 `
    -lmsimg32 `
    -lole32 `
    -luuid `
    -lwindowscodecs
if ($LASTEXITCODE -ne 0) {
    throw "Failed to build KBOFix.dll"
}

Write-Host "Built $(Join-Path $OutDir 'KBOFix.dll')"
