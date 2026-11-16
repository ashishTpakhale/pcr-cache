$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$compiler = (Get-Command g++ -CommandType Application | Select-Object -First 1).Source
$env:Path = (Split-Path $compiler) + ';' + $env:Path
$flags = '-std=c++20','-O3','-Wall','-Wextra','-Wpedantic'
& $compiler @flags bench.cpp -o bench.exe
if ($LASTEXITCODE) { throw 'Compilation failed' }
$cpu = (Get-Item 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0').GetValue('ProcessorNameString')
@($cpu, [Environment]::OSVersion.VersionString, (& $compiler --version)[0], ($flags -join ' '),
  (Get-FileHash bench.cpp).Hash, (Get-FileHash cache.hpp).Hash) | Set-Content environment.txt
# cmd redirection preserves native stdout and stderr on Windows PowerShell 5 and pwsh.
& "$env:SystemRoot\System32\cmd.exe" /d /c ".\bench.exe > replay.tmp 2> timing.txt"
if ($LASTEXITCODE) { throw 'Replay or self-test failed' }
$rows = Import-Csv replay.tmp
if ($rows.Count -ne 630) { throw 'Incomplete matrix' }
foreach ($pair in ($rows | Group-Object workload,seed)) {
    if (@($pair.Group.trace_hash | Sort-Object -Unique).Count -ne 1) { throw 'Unpaired traces' }
}
function Median($v) {
    $ordered = @($v | Sort-Object)
    $ordered[[int][math]::Floor($ordered.Count/2)]
}
$summary = foreach ($group in ($rows | Group-Object budget,workload,policy)) {
    $first = $group.Group[0]
    $seedStats = foreach ($g in ($group.Group | Group-Object seed)) {
        if ($g.Count -ne 3 -or @($g.Group.hits | Sort-Object -Unique).Count -ne 1) { throw 'Repeat mismatch' }
        $r = $g.Group[0]
        [pscustomobject]@{
            hit = 100.0 * [long]$r.hits / [long]$r.requests
            rate = Median @($g.Group | ForEach-Object { 1000.0 * [long]$_.requests / [long]$_.elapsed_ns })
            ns = Median @($g.Group | ForEach-Object { [long]$_.p99_ns })
            ticks = Median @($g.Group | ForEach-Object { [long]$_.p99_ticks })
        }
    }
    if ($seedStats.Count -ne 5) { throw 'Missing seeds' }
    [pscustomobject]@{
        KiB = [int]$first.budget/1024
        workload = $first.workload
        policy = $first.policy
        slots = 16 * [int]$first.sets
        hit_pct = [math]::Round(($seedStats.hit | Measure-Object -Average).Average,3)
        Mops = [math]::Round((Median $seedStats.rate),2)
        min_Mops = [math]::Round(($seedStats.rate | Measure-Object -Minimum).Minimum,2)
        max_Mops = [math]::Round(($seedStats.rate | Measure-Object -Maximum).Maximum,2)
        p99_ns = Median $seedStats.ns
        p99_ticks = Median $seedStats.ticks
    }
}
Move-Item -Force replay.tmp raw.csv
$summary | Export-Csv -NoTypeInformation summary.csv
$lines = @('# Measured cache replay results','','Mean hit percentage over seeds. Timing columns use the median repetition per seed, then the median across seeds. The range covers the five seed-level throughput medians. See README.md for protocol and scope.','',
'| KiB | Workload | Policy | Slots | Hit % | Mreq/s | Range (Mreq/s) | p99 ns | p99 TSC ticks |',
'|---:|---|---|---:|---:|---:|---:|---:|---:|')
foreach ($r in $summary) {
    $lines += "| $($r.KiB) | $($r.workload) | $($r.policy) | $($r.slots) | $($r.hit_pct) | $($r.Mops) | $($r.min_Mops) to $($r.max_Mops) | $($r.p99_ns) | $($r.p99_ticks) |"
}
$lines | Set-Content RESULTS.md
Write-Output "Verified $($rows.Count) runs. Results in RESULTS.md."


