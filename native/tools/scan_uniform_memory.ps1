param(
    [int]$ProcessId,
    [string]$ReferenceCsv,
    [UInt64]$GlobalPtr = 0,
    [int]$VectorOffset = -1
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class KboScanMemoryNative {
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern IntPtr OpenProcess(UInt32 access, bool inherit, UInt32 pid);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool ReadProcessMemory(IntPtr h, UInt64 addr, byte[] buf, UIntPtr size, out UIntPtr read);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool CloseHandle(IntPtr h);
}
"@

$hProcess = [KboScanMemoryNative]::OpenProcess(0x0410, $false, [uint32]$ProcessId)
if ($hProcess -eq [IntPtr]::Zero) {
    throw "OpenProcess failed for PID $ProcessId"
}

function Read-Bytes([UInt64]$Address, [int]$Size) {
    if ($Address -lt 0x10000 -or $Size -le 0) { return $null }
    $buffer = New-Object byte[] $Size
    [UIntPtr]$read = [UIntPtr]::Zero
    if (-not [KboScanMemoryNative]::ReadProcessMemory($hProcess, $Address, $buffer, [UIntPtr]::new([uint64]$Size), [ref]$read)) {
        return $null
    }
    if ($read.ToUInt64() -ne [uint64]$Size) { return $null }
    return $buffer
}

function U16($Bytes, [int]$Offset) { [BitConverter]::ToUInt16($Bytes, $Offset) }
function U32($Bytes, [int]$Offset) { [BitConverter]::ToUInt32($Bytes, $Offset) }
function I32($Bytes, [int]$Offset) { [BitConverter]::ToInt32($Bytes, $Offset) }
function U64($Bytes, [int]$Offset) { [BitConverter]::ToUInt64($Bytes, $Offset) }

function Test-GlobalDatabase([UInt64]$Candidate) {
    $db = Read-Bytes $Candidate 0xb0
    if ($null -eq $db) { return $false }
    $teamCount = I32 $db 0x9c
    if ($teamCount -lt 2 -or $teamCount -gt 500) { return $false }
    $teamVector = U64 $db 0x90
    if ($teamVector -eq 0) { return $false }

    $checks = [Math]::Min($teamCount, 5)
    $teamPointers = Read-Bytes $teamVector ($checks * 8)
    if ($null -eq $teamPointers) { return $false }

    for ($i = 0; $i -lt $checks; $i++) {
        $teamPtr = U64 $teamPointers ($i * 8)
        if ($teamPtr -eq 0) { return $false }
        $team = Read-Bytes $teamPtr 0x4460
        if ($null -eq $team) { return $false }
        $teamId = U32 $team 0x4450
        $leagueId = U32 $team 0x120
        if ($teamId -eq 0 -or $teamId -gt 100000 -or $leagueId -eq 0 -or $leagueId -gt 100000) {
            return $false
        }
    }
    return $true
}

function Test-PlayerPointer([UInt64]$PlayerPtr) {
    if ($PlayerPtr -eq 0) { return $false }
    $bytes = Read-Bytes ($PlayerPtr + 0x7c) 0x3c
    if ($null -eq $bytes) { return $false }
    $age = U16 $bytes 0
    $playerId = U32 $bytes (0xb4 - 0x7c)
    return ($playerId -gt 0 -and $playerId -lt 200000000 -and $age -lt 80)
}

try {
    $global = [uint64]$GlobalPtr
    $globalSection = "provided"
    $globalRva = 0
    if ($global -ne 0) {
        if (-not (Test-GlobalDatabase $global)) {
            throw "provided global database pointer failed validation"
        }
    } else {
        $process = Get-Process -Id $ProcessId
        $base = [uint64]$process.MainModule.BaseAddress.ToInt64()
        $exePath = $process.MainModule.FileName
        $pe = [IO.File]::ReadAllBytes($exePath)
        $peOffset = [BitConverter]::ToInt32($pe, 0x3c)
        $sectionCount = [BitConverter]::ToUInt16($pe, $peOffset + 6)
        $optionalHeaderSize = [BitConverter]::ToUInt16($pe, $peOffset + 20)
        $sectionBase = $peOffset + 24 + $optionalHeaderSize

        $sections = @()
        for ($i = 0; $i -lt $sectionCount; $i++) {
            $sectionOffset = $sectionBase + 40 * $i
            $name = [Text.Encoding]::ASCII.GetString($pe, $sectionOffset, 8).Trim([char]0)
            $virtualSize = [BitConverter]::ToUInt32($pe, $sectionOffset + 8)
            $virtualAddress = [BitConverter]::ToUInt32($pe, $sectionOffset + 12)
            $characteristics = [BitConverter]::ToUInt32($pe, $sectionOffset + 36)
            $read = ($characteristics -band 0x40000000) -ne 0
            $write = ($characteristics -band 0x80000000) -ne 0
            $execute = ($characteristics -band 0x20000000) -ne 0
            if ($read -and $write -and -not $execute -and $virtualSize -ge 8) {
                $sections += [pscustomobject]@{
                    Name = $name
                    VA = $virtualAddress
                    Size = $virtualSize
                }
            }
        }

        $globalSection = $null
        foreach ($section in $sections) {
            $data = Read-Bytes ($base + [uint64]$section.VA) ([int]$section.Size)
            if ($null -eq $data) { continue }
            for ($offset = 0; $offset -le $data.Length - 8; $offset += 8) {
                $value = U64 $data $offset
                if ($value -lt 0x10000 -or $value -gt 0x7FFFFFFFFFFF) { continue }
                if (Test-GlobalDatabase $value) {
                    $global = $value
                    $globalSection = $section.Name
                    $globalRva = $section.VA + $offset
                    break
                }
            }
            if ($global -ne 0) { break }
        }
        if ($global -eq 0) { throw "global database not found" }
    }

    $bestVector = $null
    $vectorOffsets = if ($VectorOffset -ge 0) { @($VectorOffset) } else { @(0x20..0x500 | Where-Object { ($_ -band 7) -eq 0 }) }
    foreach ($offset in $vectorOffsets) {
        $slot = Read-Bytes ($global + [uint64]$offset) 0x10
        if ($null -eq $slot) { continue }
        $vector = U64 $slot 0
        $count = I32 $slot 0x0c
        if ($vector -eq 0 -or $count -le 0 -or $count -gt 200000) { continue }

        $sampleCount = [Math]::Min($count, 2000)
        $pointers = Read-Bytes $vector ($sampleCount * 8)
        if ($null -eq $pointers) { continue }
        $matches = 0
        for ($i = 0; $i -lt $sampleCount; $i++) {
            if (Test-PlayerPointer (U64 $pointers ($i * 8))) {
                $matches++
            }
        }
        if ($null -eq $bestVector -or $matches -gt $bestVector.Matches) {
            $bestVector = [pscustomobject]@{
                Offset = $offset
                Vector = $vector
                Count = $count
                Matches = $matches
                Sample = $sampleCount
            }
        }
    }
    if ($null -eq $bestVector -or $bestVector.Matches -le 0) {
        throw "player vector not found"
    }

    $expected = @{}
    Import-Csv $ReferenceCsv | ForEach-Object {
        if ($_.id -match '^\d+$' -and $_.new_uniform_number -match '^\d+$') {
            $expected[[uint32]$_.id] = [int]$_.new_uniform_number
        }
    }

    $pointerCount = [Math]::Min($bestVector.Count, 30000)
    $allPointers = Read-Bytes ([uint64]$bestVector.Vector) ($pointerCount * 8)
    $samples = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $pointerCount; $i++) {
        $playerPtr = U64 $allPointers ($i * 8)
        if (-not (Test-PlayerPointer $playerPtr)) { continue }
        $header = Read-Bytes ($playerPtr + 0x7c) 0x3c
        $playerId = U32 $header (0xb4 - 0x7c)
        if (-not $expected.ContainsKey($playerId)) { continue }
        $playerBytes = Read-Bytes $playerPtr 0x1800
        if ($null -eq $playerBytes) { continue }
        $samples.Add([pscustomobject]@{
            Id = $playerId
            Number = [int]$expected[$playerId]
            Bytes = $playerBytes
            Ptr = $playerPtr
        }) | Out-Null
    }

    if ($samples.Count -lt 20) {
        throw "not enough matched samples: $($samples.Count)"
    }

    $byteScores = New-Object System.Collections.Generic.List[object]
    $u16Scores = New-Object System.Collections.Generic.List[object]
    $u32Scores = New-Object System.Collections.Generic.List[object]
    for ($offset = 0; $offset -lt 0x1800; $offset++) {
        $hits = 0
        $eligible = 0
        $distinct = @{}
        foreach ($sample in $samples) {
            if ($sample.Number -ge 0 -and $sample.Number -le 255) {
                $eligible++
                if ($sample.Bytes[$offset] -eq $sample.Number) {
                    $hits++
                    $distinct[$sample.Number] = 1
                }
            }
        }
        if ($hits -gt 0) {
            $byteScores.Add([pscustomobject]@{ Type = 'u8'; Offset = $offset; Hits = $hits; Eligible = $eligible; Distinct = $distinct.Count }) | Out-Null
        }

        if ($offset -le 0x17fe) {
            $hits = 0
            $eligible = 0
            $distinct = @{}
            foreach ($sample in $samples) {
                if ($sample.Number -ge 0 -and $sample.Number -le 65535) {
                    $eligible++
                    if ((U16 $sample.Bytes $offset) -eq $sample.Number) {
                        $hits++
                        $distinct[$sample.Number] = 1
                    }
                }
            }
            if ($hits -gt 0) {
                $u16Scores.Add([pscustomobject]@{ Type = 'u16'; Offset = $offset; Hits = $hits; Eligible = $eligible; Distinct = $distinct.Count }) | Out-Null
            }
        }

        if ($offset -le 0x17fc) {
            $hits = 0
            $eligible = 0
            $distinct = @{}
            foreach ($sample in $samples) {
                if ($sample.Number -ge 0) {
                    $eligible++
                    if ((U32 $sample.Bytes $offset) -eq [uint32]$sample.Number) {
                        $hits++
                        $distinct[$sample.Number] = 1
                    }
                }
            }
            if ($hits -gt 0) {
                $u32Scores.Add([pscustomobject]@{ Type = 'u32'; Offset = $offset; Hits = $hits; Eligible = $eligible; Distinct = $distinct.Count }) | Out-Null
            }
        }
    }

    "global section=$globalSection rva=0x{0:X} ptr=0x{1:X}" -f $globalRva, $global
    "player_vector offset=0x{0:X} vector=0x{1:X} count={2} matches={3}/{4}" -f $bestVector.Offset, $bestVector.Vector, $bestVector.Count, $bestVector.Matches, $bestVector.Sample
    "matched_samples=$($samples.Count)"
    "top_u8"
    $byteScores | Sort-Object @{ Expression = 'Hits'; Descending = $true }, @{ Expression = 'Distinct'; Descending = $true } |
        Select-Object -First 20 | ForEach-Object { "{0} offset=0x{1:X3} hits={2}/{3} distinct={4}" -f $_.Type, $_.Offset, $_.Hits, $_.Eligible, $_.Distinct }
    "top_u16"
    $u16Scores | Sort-Object @{ Expression = 'Hits'; Descending = $true }, @{ Expression = 'Distinct'; Descending = $true } |
        Select-Object -First 20 | ForEach-Object { "{0} offset=0x{1:X3} hits={2}/{3} distinct={4}" -f $_.Type, $_.Offset, $_.Hits, $_.Eligible, $_.Distinct }
    "top_u32"
    $u32Scores | Sort-Object @{ Expression = 'Hits'; Descending = $true }, @{ Expression = 'Distinct'; Descending = $true } |
        Select-Object -First 20 | ForEach-Object { "{0} offset=0x{1:X3} hits={2}/{3} distinct={4}" -f $_.Type, $_.Offset, $_.Hits, $_.Eligible, $_.Distinct }
}
finally {
    if ($hProcess -ne [IntPtr]::Zero) {
        [KboScanMemoryNative]::CloseHandle($hProcess) | Out-Null
    }
}
