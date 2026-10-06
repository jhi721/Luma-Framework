# Offline regression for the d3d9 VA-fix proxy, fix on, against the Windows d3d9 runtime and dgVoodoo (VRAM 4096,
# plus a VRAM 1024 run for the cap warning), and fix off (logging only: the hooks still run). Every run gets its own
# folder and all start at once; a run that takes longer than -TimeoutSeconds is killed and fails. A run passes when
# d3d9_pool_test exits 0, every readback matches, the D3D9 results match the native ones and no upload failed.
# Build first with build.cmd and Tests\build.cmd.
# Usage: pwsh -File run_tests.ps1 -DgVoodoo <folder with dgVoodoo's D3D9.dll and dgVoodoo.conf>
param([Parameter(Mandatory)][string]$DgVoodoo, [int]$TimeoutSeconds = 60)
$ErrorActionPreference = "Stop"
$bin = Join-Path $PSScriptRoot "..\..\..\Binaries\Win32-Tools\D3D9 VA Fix"
$env:D3D9_MEMLOG_MS = "300"

# Each run gets a folder with the proxy as d3d9.dll and the runtime as d3d9_chain.dll.
function New-RuntimeFolder([string]$name, [string]$chain, [string]$conf, [int]$vram, [bool]$fix) {
    $dir = Join-Path $bin "tests\$name"
    New-Item -ItemType Directory -Force $dir | Out-Null
    Copy-Item (Join-Path $bin "d3d9.dll"), (Join-Path $bin "d3d9_pool_test.exe") $dir -Force
    Copy-Item $chain (Join-Path $dir "d3d9_chain.dll") -Force
    if ($conf) {
        (Get-Content $conf) -replace '^VRAM\s*=.*$', "VRAM                                = $vram" |
            Set-Content (Join-Path $dir "dgVoodoo.conf")
    }
    New-Item -ItemType File -Force (Join-Path $dir "d3d9_memlog.on") | Out-Null
    $fixFlag = Join-Path $dir "d3d9_vafix.on"
    if ($fix) { New-Item -ItemType File -Force $fixFlag | Out-Null } else { Remove-Item $fixFlag -ErrorAction SilentlyContinue }
    Set-Content (Join-Path $dir "d3d9_memlog.log") ""
    return $dir
}

$runs = @(
    @{ Name = "scenarios"; Args = @("managed", "1", "64", "argb", "doublelock", "getdesc", "surfacelast", "threads=4", "redevice") },
    @{ Name = "verify texture"; Args = @("managed", "32", "1024", "argb", "relock", "verify") },
    @{ Name = "verify surface"; Args = @("managed", "32", "1024", "argb", "surface", "relock", "verify") },
    @{ Name = "verify evictions"; Args = @("managed", "300", "1024", "argb", "mixed", "verify") },
    @{ Name = "churn"; Args = @("managed", "128", "2048", "relock", "churn=8") }
)
# Creates must fail at the cap, with the warning.
$capRun = @{ Name = "VRAM cap"; Args = @("managed", "256", "2048"); Cap = $true }
# Fix off: the surface LockRect hook on runtime pools ("verify" reads DEFAULT copies back, so fix on only)
$fixOffRuns = @($runs[0], @{ Name = "surface locks"; Args = @("managed", "32", "1024", "argb", "surface", "relock") })
$windows = "$env:SystemRoot\SysWOW64\d3d9.dll"
$dgvDll = Join-Path $DgVoodoo "D3D9.dll"
$dgvConf = Join-Path $DgVoodoo "dgVoodoo.conf"
$runtimes = @(
    @{ Name = "windows"; Chain = $windows; Conf = $null; Vram = 0; Fix = $true; Runs = $runs },
    @{ Name = "dgvoodoo"; Chain = $dgvDll; Conf = $dgvConf; Vram = 4096; Fix = $true; Runs = $runs },
    @{ Name = "dgvoodoo_vram1024"; Chain = $dgvDll; Conf = $dgvConf; Vram = 1024; Fix = $true; Runs = @($capRun) },
    @{ Name = "windows_fixoff"; Chain = $windows; Conf = $null; Vram = 0; Fix = $false; Runs = $fixOffRuns },
    @{ Name = "dgvoodoo_fixoff"; Chain = $dgvDll; Conf = $dgvConf; Vram = 4096; Fix = $false; Runs = $fixOffRuns }
)
$counterPattern = 'uploads (\d+) .*failed (\d+),.*hit (\d+) miss (\d+) busy (\d+) evicted (\d+) \| views top-down (\d+) os (\d+), staging arena (\d+) top-down (\d+) runtime (\d+).*surface served (\d+), stale surfaces (\d+), lock failures (\d+)'
$names = "uploads", "upload_failed", "pool_hit", "pool_miss", "pool_busy", "pool_evicted", "views_topdown", "views_os",
    "staging_arena", "staging_topdown", "staging_runtime", "surface_served", "stale_surfaces", "lock_failures"
# What both runtimes return natively for a second lock of a locked level (via the texture and its surface).
$nativeDoubleLock = "doublelock: first 0x00000000, second 0x8876086C, via surface 0x8876086C, other level 0x00000000"

$started = foreach ($runtime in $runtimes) {
    foreach ($run in $runtime.Runs) {
        $dir = New-RuntimeFolder "$($runtime.Name)_$($run.Name -replace ' ', '_')" $runtime.Chain $runtime.Conf $runtime.Vram $runtime.Fix
        $info = [Diagnostics.ProcessStartInfo]::new((Join-Path $dir "d3d9_pool_test.exe"), (@("`"$(Join-Path $dir 'd3d9.dll')`"") + $run.Args) -join " ")
        $info.WorkingDirectory = $dir
        $info.RedirectStandardOutput = $true
        $info.UseShellExecute = $false
        $process = [Diagnostics.Process]::Start($info)
        @{ Name = "$($runtime.Name) / $($run.Name)"; Run = $run; Dir = $dir; Process = $process; Output = $process.StandardOutput.ReadToEndAsync() }
    }
}

$failed = 0
$deadline = [DateTime]::Now.AddSeconds($TimeoutSeconds)
foreach ($test in $started) {
    $problems = @()
    $remaining = [Math]::Max(0, [int]($deadline - [DateTime]::Now).TotalMilliseconds)
    if (-not $test.Process.WaitForExit($remaining)) {
        $test.Process.Kill()
        $test.Process.WaitForExit()
        $problems += "timeout after $TimeoutSeconds s"
    }
    elseif ($test.Process.ExitCode -ne 0) { $problems += "d3d9_pool_test exit $($test.Process.ExitCode)" }
    $output = @($test.Output.Result -split "`r?`n" | Where-Object { $_ })
    $log = @(Get-Content (Join-Path $test.Dir "d3d9_memlog.log"))
    foreach ($line in $output) {
        if ($line -match '(\d+) (mismatching|bad|errors)\b' -and [int]$Matches[1] -ne 0) { $problems += $line }
        if ($line -match 'readback failed|^staging \d+ failed') { $problems += $line }
        if ($line -match 'Release -> (\d+)' -and [int]$Matches[1] -ne 0) { $problems += $line }
        if ($line -match '^surfacelast: (\d+)/(\d+) .* (\d+)/(\d+)' -and ($Matches[1] -ne $Matches[2] -or $Matches[3] -ne $Matches[4])) { $problems += $line }
        if ($line -like "doublelock: first*" -and $line -ne $nativeDoubleLock) { $problems += $line }
    }
    $createFailed = @($output | Where-Object { $_ -match '^CreateTexture \d+ failed' })
    if ($test.Run.Cap) {
        if (-not $createFailed) { $problems += "no create failed at the VRAM cap" }
        if (-not ($log | Select-String "WARNING DEFAULT resources")) { $problems += "no VRAM cap warning" }
    }
    elseif ($createFailed) { $problems += $createFailed[0] }
    $total = @{}
    foreach ($line in $log) {
        if ($line -match $counterPattern) {
            for ($k = 0; $k -lt $names.Count; $k++) { $total[$names[$k]] += [int64]$Matches[$k + 1] }
        }
    }
    if ($total["upload_failed"]) { $problems += "upload_failed=$($total['upload_failed'])" }
    "=== $($test.Name): $(if ($problems) { 'FAILED' } else { 'ok' })"
    $output | Select-String "verify: \d+ levels|surfacelast|threads:" | ForEach-Object { "  " + $_.Line }
    "  counters: " + (($names | ForEach-Object { "$_=$($total[$_])" }) -join " ")
    $log | Select-String "user-memory|WARNING|Create.* failed 0x" | Select-Object -First 3 | ForEach-Object { "  log: " + $_.Line }
    if ($problems) {
        $failed++
        $problems | Select-Object -First 6 | ForEach-Object { "  FAIL $_" }
    }
    else {
        Remove-Item $test.Dir -Recurse -Force # failed runs keep their folders for diagnosis
    }
}
if ($failed) { "FAILED ($failed)"; exit 1 } else { "PASSED" }
