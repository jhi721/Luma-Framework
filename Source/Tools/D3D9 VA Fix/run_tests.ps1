# Offline regression for the d3d9 VA-fix proxy, fix on, against the Windows d3d9 runtime and dgVoodoo (VRAM 4096,
# plus a VRAM 1024 run for the cap warning), and fix off (logging only: the hooks still run). Build first with build.cmd and Tests\build.cmd.
# Usage: pwsh -File run_tests.ps1 -DgVoodoo <folder with dgVoodoo's D3D9.dll and dgVoodoo.conf>
param([Parameter(Mandatory)][string]$DgVoodoo)
$ErrorActionPreference = "Stop"
$bin = Join-Path $PSScriptRoot "..\..\..\Binaries\Win32-Tools\D3D9 VA Fix"
$env:D3D9_MEMLOG_MS = "300"

# Each runtime gets a folder with the proxy as d3d9.dll and the runtime as d3d9_chain.dll.
function New-RuntimeFolder([string]$name, [string]$chain, [string]$conf, [int]$vram, [bool]$fix = $true) {
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
    return $dir
}

$runs = @(
    @{ Name = "scenarios"; Args = @("managed", "1", "64", "argb", "doublelock", "getdesc", "surfacelast", "threads=4", "redevice") },
    @{ Name = "verify texture"; Args = @("managed", "32", "1024", "argb", "relock", "verify") },
    @{ Name = "verify surface"; Args = @("managed", "32", "1024", "argb", "surface", "relock", "verify") },
    @{ Name = "verify evictions"; Args = @("managed", "300", "1024", "argb", "mixed", "verify") },
    @{ Name = "churn"; Args = @("managed", "128", "2048", "relock", "churn=8") }
)
$capRun = @{ Name = "VRAM cap"; Args = @("managed", "256", "2048") }
# Fix off: the surface LockRect hook on runtime pools ("verify" reads DEFAULT copies back, so fix on only)
$fixOffRuns = @($runs[0], @{ Name = "surface locks"; Args = @("managed", "32", "1024", "argb", "surface", "relock") })
$folders = @(
    @{ Dir = New-RuntimeFolder "windows" "$env:SystemRoot\SysWOW64\d3d9.dll" $null 0; Runs = $runs },
    @{ Dir = New-RuntimeFolder "dgvoodoo" (Join-Path $DgVoodoo "D3D9.dll") (Join-Path $DgVoodoo "dgVoodoo.conf") 4096; Runs = $runs },
    @{ Dir = New-RuntimeFolder "dgvoodoo_vram1024" (Join-Path $DgVoodoo "D3D9.dll") (Join-Path $DgVoodoo "dgVoodoo.conf") 1024; Runs = @($capRun) },
    @{ Dir = New-RuntimeFolder "windows_fixoff" "$env:SystemRoot\SysWOW64\d3d9.dll" $null 0 $false; Runs = $fixOffRuns },
    @{ Dir = New-RuntimeFolder "dgvoodoo_fixoff" (Join-Path $DgVoodoo "D3D9.dll") (Join-Path $DgVoodoo "dgVoodoo.conf") 4096 $false; Runs = $fixOffRuns }
)
$counterPattern = 'uploads (\d+) .*failed (\d+),.*hit (\d+) miss (\d+) busy (\d+) evicted (\d+) \| views top-down (\d+) os (\d+), staging arena (\d+) top-down (\d+) runtime (\d+).*surface served (\d+), stale surfaces (\d+), lock failures (\d+)'
$names = "uploads", "upload_failed", "pool_hit", "pool_miss", "pool_busy", "pool_evicted", "views_topdown", "views_os",
    "staging_arena", "staging_topdown", "staging_runtime", "surface_served", "stale_surfaces", "lock_failures"

foreach ($folder in $folders) {
    foreach ($run in $folder.Runs) {
        $log = Join-Path $folder.Dir "d3d9_memlog.log"
        Set-Content $log ""
        Write-Output "=== $(Split-Path $folder.Dir -Leaf) / $($run.Name)"
        & (Join-Path $folder.Dir "d3d9_pool_test.exe") (Join-Path $folder.Dir "d3d9.dll") @($run.Args) |
            Select-String "verify: \d+ levels|mismatch|readback failed|doublelock|getdesc|surfacelast|threads:|redevice|Release ->|failed" |
            Select-Object -First 12 | ForEach-Object { "  " + $_.Line }
        if ($LASTEXITCODE -ne 0) { "  FAILED: d3d9_pool_test exit $LASTEXITCODE" }
        $total = @{}
        foreach ($line in Get-Content $log) {
            if ($line -match $counterPattern) {
                for ($k = 0; $k -lt $names.Count; $k++) { $total[$names[$k]] += [int64]$Matches[$k + 1] }
            }
        }
        "  counters: " + (($names | ForEach-Object { "$_=$($total[$_])" }) -join " ")
        Get-Content $log | Select-String "user-memory|WARNING|Create.* failed 0x" | Select-Object -First 3 |
            ForEach-Object { "  log: " + $_.Line }
    }
}
