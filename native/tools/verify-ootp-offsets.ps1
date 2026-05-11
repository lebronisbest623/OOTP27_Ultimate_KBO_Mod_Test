<#
.SYNOPSIS
    Verifies that catalogued OOTP player struct offsets are still valid
    against a live OOTP process.

.DESCRIPTION
    Discovers the global database and player vector in the running OOTP
    process, samples up to -SampleCount players, and for each catalogued
    offset reports whether the field values fall within expected ranges.

    Offsets that fall below -MinPassRate receive a list of candidate
    replacements found by scanning +-ScanRadius bytes, together with the
    exact #define lines to update in ootp_offsets.h.

.EXAMPLE
    # Find the OOTP PID, then run:
    Get-Process | Where-Object { $_.Name -like "OOTP*" } | Select-Object Id, Name
    .\verify-ootp-offsets.ps1 -ProcessId 1234

.EXAMPLE
    # Wider scan window and more samples for a thorough check after a major patch:
    .\verify-ootp-offsets.ps1 -ProcessId 1234 -SampleCount 100 -ScanRadius 0x80
#>
param(
    [Parameter(Mandatory)][int]    $ProcessId,
    [int]                          $SampleCount = 60,
    [int]                          $ScanRadius  = 0x40,
    [double]                       $MinPassRate = 0.80
)

$ErrorActionPreference = 'Stop'

# ---- P/Invoke ----------------------------------------------------------------
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class KboVerOff {
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern IntPtr OpenProcess(UInt32 access, bool inherit, UInt32 pid);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool ReadProcessMemory(IntPtr h, UInt64 addr, byte[] buf, UIntPtr sz, out UIntPtr rd);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool CloseHandle(IntPtr h);
}
"@

$hProcess = [KboVerOff]::OpenProcess(0x0410, $false, [uint32]$ProcessId)
if ($hProcess -eq [IntPtr]::Zero) { throw "OpenProcess failed for PID $ProcessId" }

# ---- Memory helpers ----------------------------------------------------------
function Read-Bytes([uint64]$Addr, [int]$Sz) {
    if ($Addr -lt 0x10000 -or $Sz -le 0) { return $null }
    $buf = [byte[]]::new($Sz)
    [UIntPtr]$rd = [UIntPtr]::Zero
    if (-not [KboVerOff]::ReadProcessMemory($hProcess, $Addr, $buf, [UIntPtr]::new([uint64]$Sz), [ref]$rd)) { return $null }
    if ($rd.ToUInt64() -ne [uint64]$Sz) { return $null }
    $buf
}

function U8  ($b, [int]$o) { $b[$o] }
function U16 ($b, [int]$o) { [BitConverter]::ToUInt16($b, $o) }
function I16 ($b, [int]$o) { [BitConverter]::ToInt16($b, $o) }
function U32 ($b, [int]$o) { [BitConverter]::ToUInt32($b, $o) }
function I32 ($b, [int]$o) { [BitConverter]::ToInt32($b, $o) }
function U64 ($b, [int]$o) { [BitConverter]::ToUInt64($b, $o) }

function Read-Field($Bytes, [int]$Off, [string]$Typ) {
    switch ($Typ) {
        'u8'  { $Bytes[$Off] }
        'u16' { U16 $Bytes $Off }
        'i16' { I16 $Bytes $Off }
        'u32' { U32 $Bytes $Off }
        'i32' { I32 $Bytes $Off }
        default { $null }
    }
}

function Get-TypeWidth([string]$Typ) {
    switch ($Typ) { 'u8' { 1 } 'u16' { 2 } 'i16' { 2 } 'u32' { 4 } 'i32' { 4 } default { 1 } }
}

# ---- Global database discovery (mirrors scan_uniform_memory.ps1) -------------
function Test-GlobalDb([uint64]$Ptr) {
    $db = Read-Bytes $Ptr 0xb0
    if ($null -eq $db) { return $false }
    $teamCount = I32 $db 0x9c
    if ($teamCount -lt 2 -or $teamCount -gt 500) { return $false }
    $teamVec = U64 $db 0x90
    if ($teamVec -eq 0) { return $false }
    $n = [Math]::Min($teamCount, 4)
    $ptrs = Read-Bytes $teamVec ($n * 8)
    if ($null -eq $ptrs) { return $false }
    for ($i = 0; $i -lt $n; $i++) {
        $tp = U64 $ptrs ($i * 8)
        if ($tp -eq 0) { return $false }
        $team = Read-Bytes $tp 0x4460
        if ($null -eq $team) { return $false }
        $tid = U32 $team 0x4450
        $lid = U32 $team 0x120
        if ($tid -eq 0 -or $tid -gt 100000 -or $lid -eq 0 -or $lid -gt 100000) { return $false }
    }
    $true
}

# ---- Player plausibility (uses age@0x7c and id@0xb4 as anchors) -------------
# These two fields are the most structurally stable; if they drift, the
# scan_uniform_memory.ps1 tool can locate them via reference uniform data.
function Test-Player([uint64]$Ptr) {
    if ($Ptr -eq 0) { return $false }
    $b = Read-Bytes ($Ptr + 0x7c) 0x3c
    if ($null -eq $b) { return $false }
    $age = U16 $b 0
    $id  = U32 $b (0xb4 - 0x7c)
    $id -gt 0 -and $id -lt 200000000 -and $age -gt 14 -and $age -lt 66
}

# ---- Offset catalog ----------------------------------------------------------
# Scannable = whether to probe nearby bytes when this offset fails.
# Flag fields (Min=0 Max=1) are not scannable — any byte in a struct will
# accidentally pass a 0/1 range check, producing useless noise.
$Catalog = @(
    # Core identity (high-confidence anchors)
    @{ Name='OOTP27_PLAYER_ID_OFFSET';                Off=0x0b4; Typ='u32'; Min=1;    Max=199999999; Scannable=1; Desc='player id' }
    @{ Name='OOTP27_PLAYER_AGE_OFFSET';               Off=0x07c; Typ='u16'; Min=15;   Max=65;        Scannable=1; Desc='age' }
    @{ Name='OOTP27_PLAYER_NATION_ID_OFFSET';         Off=0x06c; Typ='u32'; Min=1;    Max=1000;      Scannable=1; Desc='nation id' }
    # Team / league assignments
    @{ Name='OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET'; Off=0x048; Typ='u32'; Min=0;    Max=100000;    Scannable=1; Desc='current league id' }
    @{ Name='OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET';Off=0x04c; Typ='u32'; Min=0;    Max=100000;    Scannable=1; Desc='original league id' }
    @{ Name='OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET';    Off=0x054; Typ='u32'; Min=0;    Max=100000;    Scannable=1; Desc='loan league id' }
    @{ Name='OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET';   Off=0x058; Typ='u32'; Min=0;    Max=100000;    Scannable=1; Desc='current team id' }
    @{ Name='OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET';  Off=0x05c; Typ='u32'; Min=0;    Max=100000;    Scannable=1; Desc='original team id' }
    @{ Name='OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET';    Off=0x060; Typ='u32'; Min=0;    Max=100000;    Scannable=1; Desc='active team id' }
    @{ Name='OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET';      Off=0x068; Typ='u32'; Min=0;    Max=100000;    Scannable=1; Desc='loan team id' }
    # Binary status flags (not scannable — 0/1 range matches any byte)
    @{ Name='OOTP27_PLAYER_RETIRED_FLAG_OFFSET';      Off=0x040; Typ='u8';  Min=0;    Max=1;         Scannable=0; Desc='retired flag' }
    @{ Name='OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET';  Off=0x842; Typ='u8';  Min=0;    Max=1;         Scannable=0; Desc='loan active flag' }
    @{ Name='OOTP27_PLAYER_DFA_FLAG_OFFSET';          Off=0x85b; Typ='u8';  Min=0;    Max=1;         Scannable=0; Desc='DFA flag' }
    @{ Name='OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET';   Off=0x83b; Typ='u8';  Min=0;    Max=1;         Scannable=0; Desc='restricted flag' }
    @{ Name='OOTP27_PLAYER_INJURY_ACTIVE_OFFSET';     Off=0x874; Typ='u8';  Min=0;    Max=1;         Scannable=0; Desc='injury active flag' }
    @{ Name='OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET';   Off=0x896; Typ='u8';  Min=0;    Max=1;         Scannable=0; Desc='military active flag' }
    @{ Name='OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET'; Off=0x83c; Typ='u8'; Min=0; Max=1;     Scannable=0; Desc='secondary restricted flag' }
    @{ Name='OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET';   Off=0xf69; Typ='u8';  Min=0;    Max=1;         Scannable=0; Desc='loan dirty flag' }
    # Position and ratings
    @{ Name='OOTP27_PLAYER_POSITION_GROUP_OFFSET';    Off=0xc78; Typ='u8';  Min=1;    Max=10;        Scannable=1; Desc='position group (1=P..10=DH)' }
    @{ Name='OOTP27_PLAYER_POSITION_ROLE_OFFSET';     Off=0xc79; Typ='u8';  Min=0;    Max=20;        Scannable=1; Desc='position role' }
    @{ Name='OOTP27_PLAYER_OVERALL_VALUE_OFFSET';     Off=0xcd2; Typ='i16'; Min=-200; Max=5000;      Scannable=1; Desc='overall value' }
    @{ Name='OOTP27_PLAYER_TALENT_VALUE_OFFSET';      Off=0xcd4; Typ='i16'; Min=-200; Max=5000;      Scannable=1; Desc='talent value' }
    @{ Name='OOTP27_PLAYER_RATINGS_VALUE_OFFSET';     Off=0xcd6; Typ='i16'; Min=-200; Max=5000;      Scannable=1; Desc='ratings value' }
    @{ Name='OOTP27_PLAYER_CAREER_VALUE_OFFSET';      Off=0xcd8; Typ='i16'; Min=-200; Max=5000;      Scannable=1; Desc='career value' }
    # Contract
    @{ Name='OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET'; Off=0x8a8; Typ='u8'; Min=0;  Max=10;         Scannable=1; Desc='contract level' }
    @{ Name='OOTP27_PLAYER_CONTRACT_STATUS_OFFSET';   Off=0x8d0; Typ='u32'; Min=0;   Max=20;         Scannable=1; Desc='contract status' }
    # Draft
    @{ Name='OOTP27_PLAYER_DRAFT_CLASS_OFFSET';       Off=0xdb1; Typ='u8';  Min=0;   Max=4;          Scannable=1; Desc='draft class' }
    @{ Name='OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET';   Off=0xd70; Typ='u32'; Min=0;   Max=100000;     Scannable=1; Desc='draft league id' }
)

# ---- Candidate search --------------------------------------------------------
function Find-Candidates($Samples, [int]$ExpOff, [string]$Typ, [long]$Min, [long]$Max) {
    $width  = Get-TypeWidth $Typ
    $lo     = [int][Math]::Max(0, $ExpOff - $ScanRadius)
    $hi     = [int][Math]::Min(0x1800 - $width, $ExpOff + $ScanRadius)
    $start  = [int]([Math]::Floor($lo / $width) * $width)
    $end    = [int]([Math]::Floor($hi / $width) * $width)

    $results = [System.Collections.Generic.List[object]]::new()
    for ($off = $start; $off -le $end; $off += $width) {
        if ($off -eq $ExpOff) { continue }
        $hits     = 0
        $eligible = 0
        foreach ($s in $Samples) {
            if ($off + $width -gt $s.Bytes.Length) { continue }
            $eligible++
            $v = Read-Field $s.Bytes $off $Typ
            if ($null -ne $v -and [long]$v -ge $Min -and [long]$v -le $Max) { $hits++ }
        }
        if ($eligible -gt 0 -and $hits -gt 0) {
            $results.Add([pscustomobject]@{ Offset = $off; Hits = $hits; Eligible = $eligible }) | Out-Null
        }
    }
    $results | Sort-Object { $_.Hits } -Descending | Select-Object -First 3
}

try {
    # ---- Locate global database ----------------------------------------------
    Write-Host "Locating OOTP global database..." -NoNewline
    $proc   = Get-Process -Id $ProcessId
    $base   = [uint64]$proc.MainModule.BaseAddress.ToInt64()
    $pe     = [IO.File]::ReadAllBytes($proc.MainModule.FileName)
    $peOff  = [BitConverter]::ToInt32($pe, 0x3c)
    $secCnt = [BitConverter]::ToUInt16($pe, $peOff + 6)
    $optSz  = [BitConverter]::ToUInt16($pe, $peOff + 20)
    $secBase = $peOff + 24 + $optSz

    $globalPtr = [uint64]0
    $globalRva = 0

    for ($si = 0; $si -lt $secCnt -and $globalPtr -eq 0; $si++) {
        $so   = $secBase + 40 * $si
        $vSz  = [BitConverter]::ToUInt32($pe, $so + 8)
        $vA   = [BitConverter]::ToUInt32($pe, $so + 12)
        $char = [BitConverter]::ToUInt32($pe, $so + 36)
        # readable + writable, not executable
        if (($char -band 0xC0000000) -ne 0xC0000000 -or ($char -band 0x20000000) -ne 0) { continue }
        if ($vSz -lt 8) { continue }
        $data = Read-Bytes ($base + [uint64]$vA) ([int]$vSz)
        if ($null -eq $data) { continue }
        for ($off = 0; $off -le $data.Length - 8; $off += 8) {
            $v = U64 $data $off
            if ($v -lt 0x10000 -or $v -gt 0x7FFFFFFFFFFF) { continue }
            if (Test-GlobalDb $v) {
                $globalPtr = $v
                $globalRva = $vA + $off
                break
            }
        }
    }
    if ($globalPtr -eq 0) { throw "Global database not found in .data sections" }
    Write-Host ("  found (rva=0x{0:X})" -f $globalRva)

    # ---- Locate player vector ------------------------------------------------
    Write-Host "Locating player vector..." -NoNewline
    $bestVec = $null
    foreach ($off in (0x20..0x500 | Where-Object { ($_ -band 7) -eq 0 })) {
        $slot = Read-Bytes ($globalPtr + [uint64]$off) 0x10
        if ($null -eq $slot) { continue }
        $vec   = U64 $slot 0
        $count = I32 $slot 0x0c
        if ($vec -eq 0 -or $count -le 0 -or $count -gt 300000) { continue }
        $sample = [Math]::Min($count, 500)
        $ptrs   = Read-Bytes $vec ($sample * 8)
        if ($null -eq $ptrs) { continue }
        $hits = 0
        for ($i = 0; $i -lt $sample; $i++) { if (Test-Player (U64 $ptrs ($i * 8))) { $hits++ } }
        if ($null -eq $bestVec -or $hits -gt $bestVec.Hits) {
            $bestVec = [pscustomobject]@{ Off=$off; Vec=$vec; Count=$count; Hits=$hits; Sample=$sample }
        }
    }
    if ($null -eq $bestVec -or $bestVec.Hits -le 0) { throw "Player vector not found" }
    Write-Host ("  found (db+0x{0:X}, {1} players)" -f $bestVec.Off, $bestVec.Count)

    # ---- Sample players ------------------------------------------------------
    Write-Host "Sampling $SampleCount players..." -NoNewline
    $allPtrs = Read-Bytes $bestVec.Vec ($bestVec.Count * 8)
    $samples = [System.Collections.Generic.List[object]]::new()
    $checked = 0
    for ($i = 0; $i -lt $bestVec.Count -and $samples.Count -lt $SampleCount; $i++) {
        $checked++
        $ptr = U64 $allPtrs ($i * 8)
        if (-not (Test-Player $ptr)) { continue }
        $b = Read-Bytes $ptr 0x1800
        if ($null -eq $b) { continue }
        $samples.Add([pscustomobject]@{ Ptr=$ptr; Bytes=$b }) | Out-Null
    }
    if ($samples.Count -lt 10) { throw "Too few valid players found ($($samples.Count)); is the game in a loaded save?" }
    Write-Host "  $($samples.Count) sampled (checked $checked)"
    Write-Host ""

    # ---- Validate each offset ------------------------------------------------
    $passCount = 0
    $failCount = 0
    $warnCount = 0
    $updates   = [System.Collections.Generic.List[string]]::new()

    Write-Host ("  {0,-55} {1,-6} {2,-4} {3}" -f "Offset name", "hex", "type", "result")
    Write-Host ("  " + "-" * 90)

    foreach ($entry in $Catalog) {
        $width    = Get-TypeWidth $entry.Typ
        $hits     = 0
        $eligible = 0

        foreach ($s in $samples) {
            if ($entry.Off + $width -gt $s.Bytes.Length) { continue }
            $eligible++
            $v = Read-Field $s.Bytes $entry.Off $entry.Typ
            if ($null -ne $v -and [long]$v -ge [long]$entry.Min -and [long]$v -le [long]$entry.Max) { $hits++ }
        }

        $rate   = if ($eligible -gt 0) { $hits / $eligible } else { 0.0 }
        $pct    = "{0}/{1}" -f $hits, $eligible
        $hexOff = "0x{0:x3}" -f $entry.Off

        if ($rate -ge $MinPassRate) {
            $passCount++
            Write-Host ("  PASS  {0,-50} {1}  {2,-4}  {3}" -f $entry.Name, $hexOff, $entry.Typ, $pct) -ForegroundColor Green
        } elseif ($rate -ge 0.50) {
            $warnCount++
            Write-Host ("  WARN  {0,-50} {1}  {2,-4}  {3}  ({4})" -f $entry.Name, $hexOff, $entry.Typ, $pct, $entry.Desc) -ForegroundColor Yellow
        } else {
            $failCount++
            Write-Host ("  FAIL  {0,-50} {1}  {2,-4}  {3}  ({4})" -f $entry.Name, $hexOff, $entry.Typ, $pct, $entry.Desc) -ForegroundColor Red

            if ($entry.Scannable -and $ScanRadius -gt 0) {
                $candidates = Find-Candidates $samples $entry.Off $entry.Typ $entry.Min $entry.Max
                if ($candidates.Count -gt 0) {
                    $best = $candidates[0]
                    $bestHex = "0x{0:x3}" -f $best.Offset
                    Write-Host ("        candidates: " + (($candidates | ForEach-Object { "0x{0:x3} ({1}/{2})" -f $_.Offset, $_.Hits, $_.Eligible }) -join "  ")) -ForegroundColor Yellow
                    $updates.Add(("-#define {0,-52} 0x{1:x}u" -f $entry.Name, $entry.Off)) | Out-Null
                    $updates.Add(("+#define {0,-52} 0x{1:x}u  // candidate ({2}/{3})" -f $entry.Name, $best.Offset, $best.Hits, $best.Eligible)) | Out-Null
                } else {
                    Write-Host "        no candidates found in scan range" -ForegroundColor Yellow
                    $updates.Add(("?#define {0,-52} 0x{1:x}u  // FAILED, no candidate found in +/-0x{2:x}" -f $entry.Name, $entry.Off, $ScanRadius)) | Out-Null
                }
            }
        }
    }

    # ---- Summary -------------------------------------------------------------
    Write-Host ""
    Write-Host ("Summary: {0} PASS  {1} WARN  {2} FAIL" -f $passCount, $warnCount, $failCount)

    if ($updates.Count -gt 0) {
        Write-Host ""
        Write-Host "Suggested ootp_offsets.h changes:"
        foreach ($line in $updates) { Write-Host "  $line" }
    }

    if ($failCount -eq 0 -and $warnCount -eq 0) {
        Write-Host ""
        Write-Host "All offsets verified against OOTP PID $ProcessId." -ForegroundColor Green
        exit 0
    } elseif ($failCount -eq 0) {
        exit 0
    } else {
        exit 1
    }
}
finally {
    if ($hProcess -ne [IntPtr]::Zero) { [KboVerOff]::CloseHandle($hProcess) | Out-Null }
}
