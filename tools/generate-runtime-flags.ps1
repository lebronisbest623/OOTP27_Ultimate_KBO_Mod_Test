$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ManifestPath = Join-Path $RepoRoot "config\kbo-runtime-flags.json"
$CSharpPath = Join-Path $RepoRoot "src\KBOLauncher\Infrastructure\KboFlags.RuntimeDefinitions.cs"
$NativeUiPath = Join-Path $RepoRoot "native\src\hotkey_window\views\mod\runtime_flags\runtime_flags.generated.inc"
$NativeAliasPath = Join-Path $RepoRoot "native\src\core\core_flags\keys\runtime_flag_aliases.generated.inc"

$ValidLifecycles = @("User", "Recovery", "Diagnostic", "Legacy")

function Test-JsonProperty($Object, [string]$Name) {
    return $Object.PSObject.Properties.Name -contains $Name
}

function Require-String($Object, [string]$Name, [string]$Context) {
    if (-not (Test-JsonProperty $Object $Name) -or [string]::IsNullOrWhiteSpace($Object.$Name)) {
        throw "$Context requires '$Name'."
    }
    return [string]$Object.$Name
}

function Convert-ToCsString([string]$Value) {
    return '"' + (($Value -replace '\\', '\\') -replace '"', '\"') + '"'
}

function Convert-ToCString([string]$Value) {
    return '"' + (($Value -replace '\\', '\\') -replace '"', '\"') + '"'
}

function Convert-ToCsNullableBool($Value) {
    if ($null -eq $Value) {
        return "null"
    }

    return $(if ([bool]$Value) { "true" } else { "false" })
}

function Convert-ToCsBool($Value) {
    return $(if ([bool]$Value) { "true" } else { "false" })
}

function Convert-ToCIntBool($Value) {
    return $(if ([bool]$Value) { "1" } else { "0" })
}

function Convert-ToNativeFlagCategory([string]$Lifecycle) {
    switch ($Lifecycle) {
        "User" { return "KBO_MOD_FLAG_USER" }
        "Recovery" { return "KBO_MOD_FLAG_RECOVERY" }
        "Diagnostic" { return "KBO_MOD_FLAG_DIAGNOSTIC" }
        "Legacy" { return "KBO_MOD_FLAG_LEGACY" }
        default { throw "Unsupported runtime flag lifecycle '$Lifecycle'." }
    }
}

function Write-Utf8NoBom([string]$Path, [string[]]$Lines) {
    $Encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, (($Lines -join "`r`n") + "`r`n"), $Encoding)
}

$Manifest = Get-Content -Raw -Encoding UTF8 $ManifestPath | ConvertFrom-Json
if ($null -eq $Manifest.flags -or $Manifest.flags.Count -eq 0) {
    throw "Runtime flag manifest must contain at least one flag."
}

$SeenKeys = @{}
$Flags = @()
foreach ($Flag in $Manifest.flags) {
    $Key = Require-String $Flag "key" "Runtime flag"
    if ($SeenKeys.ContainsKey($Key)) {
        throw "Duplicate runtime flag key '$Key'."
    }
    $SeenKeys[$Key] = $true

    if ($Key -notmatch '^[a-z0-9_]+$') {
        throw "Runtime flag key '$Key' must be lowercase snake_case."
    }

    if (-not (Test-JsonProperty $Flag "defaultValue")) {
        throw "Runtime flag '$Key' requires 'defaultValue' (use null when there is no launcher seed)."
    }

    if (-not (Test-JsonProperty $Flag "importLegacy")) {
        throw "Runtime flag '$Key' requires 'importLegacy'."
    }

    $Lifecycle = Require-String $Flag "lifecycle" "Runtime flag '$Key'"
    if ($ValidLifecycles -notcontains $Lifecycle) {
        throw "Runtime flag '$Key' has unsupported lifecycle '$Lifecycle'."
    }

    $Ui = $null
    if (Test-JsonProperty $Flag "ui") {
        $UiLabel = Require-String $Flag.ui "label" "Runtime flag '$Key' UI"
        $EnabledValue = if (Test-JsonProperty $Flag.ui "enabledValue") { [int]$Flag.ui.enabledValue } else { 1 }
        if ($EnabledValue -ne 0 -and $EnabledValue -ne 1) {
            throw "Runtime flag '$Key' UI enabledValue must be 0 or 1."
        }

        if (-not (Test-JsonProperty $Flag.ui "defaultEnabled")) {
            throw "Runtime flag '$Key' UI requires 'defaultEnabled'."
        }

        $CompanionEnableKey = if (Test-JsonProperty $Flag.ui "companionEnableKey") { [string]$Flag.ui.companionEnableKey } else { $null }

        $Ui = [pscustomobject]@{
            Label = $UiLabel
            EnabledValue = $EnabledValue
            DefaultEnabled = [bool]$Flag.ui.defaultEnabled
            CompanionEnableKey = $CompanionEnableKey
        }
    }

    $Flags += [pscustomobject]@{
        Key = $Key
        DefaultValue = $Flag.defaultValue
        ImportLegacy = [bool]$Flag.importLegacy
        Lifecycle = $Lifecycle
        Ui = $Ui
    }
}

foreach ($Flag in $Flags | Where-Object { $null -ne $_.Ui -and -not [string]::IsNullOrWhiteSpace($_.Ui.CompanionEnableKey) }) {
    if (-not $SeenKeys.ContainsKey($Flag.Ui.CompanionEnableKey)) {
        throw "Runtime flag '$($Flag.Key)' UI companion '$($Flag.Ui.CompanionEnableKey)' is not declared as a runtime flag."
    }
}

$SingleDivisionAllstarFlagKeys = @()
foreach ($Key in $Manifest.singleDivisionAllstarFlagKeys) {
    if (-not $SeenKeys.ContainsKey([string]$Key)) {
        throw "singleDivisionAllstarFlagKeys contains unknown flag '$Key'."
    }
    $SingleDivisionAllstarFlagKeys += [string]$Key
}

$SeenLegacyAliases = @{}
$Aliases = @()
foreach ($Alias in $Manifest.aliases) {
    $LegacyKey = Require-String $Alias "legacyKey" "Runtime flag alias"
    $CanonicalKey = Require-String $Alias "canonicalKey" "Runtime flag alias '$LegacyKey'"
    if ($SeenLegacyAliases.ContainsKey($LegacyKey)) {
        throw "Duplicate runtime flag alias '$LegacyKey'."
    }
    if (-not $SeenKeys.ContainsKey($CanonicalKey)) {
        throw "Runtime flag alias '$LegacyKey' points at unknown canonical key '$CanonicalKey'."
    }
    $SeenLegacyAliases[$LegacyKey] = $true
    $Aliases += [pscustomobject]@{
        LegacyKey = $LegacyKey
        CanonicalKey = $CanonicalKey
    }
}

$CsLines = @(
    "// <auto-generated />",
    "// Source: config/kbo-runtime-flags.json",
    "",
    "internal static partial class KboFlags",
    "{",
    "    private enum RuntimeFlagLifecycle",
    "    {",
    "        User,",
    "        Recovery,",
    "        Diagnostic,",
    "        Legacy,",
    "    }",
    "",
    "    private sealed record RuntimeFlagDefinition(",
    "        string Key,",
    "        bool? DefaultValue,",
    "        bool ImportLegacy,",
    "        RuntimeFlagLifecycle Lifecycle);",
    "",
    "    private sealed record RuntimeFlagAlias(",
    "        string LegacyKey,",
    "        string CanonicalKey);",
    "",
    "    private static readonly RuntimeFlagDefinition[] RuntimeFlags =",
    "    ["
)
foreach ($Flag in $Flags) {
    $CsLines += '        new({0}, {1}, {2}, RuntimeFlagLifecycle.{3}),' -f `
        (Convert-ToCsString $Flag.Key),
        (Convert-ToCsNullableBool $Flag.DefaultValue),
        (Convert-ToCsBool $Flag.ImportLegacy),
        $Flag.Lifecycle
}
$CsLines += @(
    "    ];",
    "",
    "    private static readonly string[] SingleDivisionAllstarFlagFiles =",
    "    ["
)
foreach ($Key in $SingleDivisionAllstarFlagKeys) {
    $CsLines += '        {0},' -f (Convert-ToCsString ($Key + ".txt"))
}
$CsLines += @(
    "    ];",
    "",
    "    private static readonly RuntimeFlagAlias[] RuntimeFlagAliases =",
    "    ["
)
foreach ($Alias in $Aliases) {
    $CsLines += '        new({0}, {1}),' -f (Convert-ToCsString $Alias.LegacyKey), (Convert-ToCsString $Alias.CanonicalKey)
}
$CsLines += @(
    "    ];",
    "",
    "    private static readonly HashSet<string> LegacyImportFlagKeys = RuntimeFlags",
    "        .Where(flag => flag.ImportLegacy)",
    "        .Select(flag => flag.Key)",
    "        .Concat(RuntimeFlagAliases.Select(alias => alias.LegacyKey))",
    "        .ToHashSet(StringComparer.OrdinalIgnoreCase);",
    "",
    "}"
)

$NativeUiLines = @(
    "/* <auto-generated />",
    "   Source: config/kbo-runtime-flags.json */"
)
foreach ($Flag in $Flags | Where-Object { $null -ne $_.Ui }) {
    $Companion = if ([string]::IsNullOrWhiteSpace($Flag.Ui.CompanionEnableKey)) { "NULL" } else { Convert-ToCString $Flag.Ui.CompanionEnableKey }
    $NativeUiLines += '    {{ {0}, {1}, {2}, {3}, {4}, {5} }},' -f `
        (Convert-ToCString $Flag.Key),
        (Convert-ToCString $Flag.Ui.Label),
        $Flag.Ui.EnabledValue,
        (Convert-ToCIntBool $Flag.Ui.DefaultEnabled),
        $Companion,
        (Convert-ToNativeFlagCategory $Flag.Lifecycle)
}

$NativeAliasLines = @(
    "/* <auto-generated />",
    "   Source: config/kbo-runtime-flags.json */"
)
foreach ($Alias in $Aliases) {
    $NativeAliasLines += '    if (strcmp(key, {0}) == 0) {{' -f (Convert-ToCString $Alias.CanonicalKey)
    $NativeAliasLines += '        return {0};' -f (Convert-ToCString $Alias.LegacyKey)
    $NativeAliasLines += "    }"
    $NativeAliasLines += ""
}

Write-Utf8NoBom $CSharpPath $CsLines
Write-Utf8NoBom $NativeUiPath $NativeUiLines
Write-Utf8NoBom $NativeAliasPath $NativeAliasLines

Write-Host "Generated $CSharpPath"
Write-Host "Generated $NativeUiPath"
Write-Host "Generated $NativeAliasPath"
