param(
    [string]$Exe = "$PSScriptRoot/../build-windows/Release/microvoxels.exe",
    [ValidateSet('garden', 'test')][string]$Scene = 'garden',
    [ValidateRange(160, 4096)][int]$Width = 1920,
    [ValidateRange(120, 2160)][int]$Height = 1080,
    [ValidateRange(60, 10000)][int]$Frames = 240,
    [ValidateRange(1, 9)][int]$Repeats = 3,
    [ValidateRange(1, 500)][double]$CacheMs = 100,
    [string]$Output = "$PSScriptRoot/../profiles/voxel-cache-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$null = New-Item -ItemType Directory -Force -Path $Output
$Output = (Resolve-Path $Output).Path
$runs = @()
$reference = $null
for ($round = 0; $round -lt $Repeats; ++$round) {
    $order = if ($round % 2) { @('on', 'off') } else { @('off', 'on') }
    foreach ($mode in $order) {
        $path = Join-Path $Output "$mode-$round.json"
        # Visible window avoids deliberately occluding the hardware workload.
        # Finite runs advance source animation by exactly 1/60 s per frame.
        $demoArgs = @('--scene', $Scene, '--mode', '1', '--no-ui', '--no-vsync',
            '--width', "$Width", '--height', "$Height", '--frames', "$Frames",
            '--cache-ms', $CacheMs.ToString([Globalization.CultureInfo]::InvariantCulture),
            '--report', $path)
        if ($mode -eq 'off') { $demoArgs += '--no-voxel-cache' }
        Write-Host "Round $($round + 1)/${Repeats}: cache $mode"
        & $Exe @demoArgs
        if ($LASTEXITCODE -ne 0) { throw "Cache benchmark failed: exit $LASTEXITCODE" }
        $r = Get-Content -Raw $path | ConvertFrom-Json
        if (!$r.timestamp_supported -or !$r.timed_frames_after_warmup) {
            throw 'No valid GPU timestamps.'
        }
        if (!$r.unique_voxels -or $r.dropped_hits -or $r.dropped_root_history_samples -or $r.cache_dropped_voxels) {
            throw 'Empty or overflowing cloud; use coarser settings.'
        }
        if ($mode -eq 'on' -and $r.cache_capacity_resets) {
            throw 'Cloud exceeded the cache capacity; use a lower source resolution for this comparison.'
        }
        $signature = "$($r.samples)/$($r.hits)/$($r.triangles)"
        if ($null -eq $reference) { $reference = $signature }
        if ($signature -ne $reference) { throw 'Source inputs differed between cache runs.' }
        $runs += [pscustomobject]@{
            Mode=$mode; Round=$round; Device=$r.device; DeviceType=$r.device_type
            PresentMode=$r.present_mode; GenerationMs=[double]$r.gpu_generation_ms
            CacheResolveMs=[double]$r.average_gpu_timings.cache_resolve_ms
            DrawMs=[double]$r.final_voxel_draw_ms; GpuFrameMs=[double]$r.gpu_frame_ms
            MeanRetainedCubes=[double]$r.cache_retained_total / $Frames
            Report=$path
        }
    }
}
function Median($values) {
    $sorted = @($values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}
$summary = foreach ($mode in @('off', 'on')) {
    $matching = @($runs | Where-Object Mode -eq $mode)
    [pscustomobject]@{
        Cache=$mode; GenerationMs=(Median $matching.GenerationMs)
        CacheResolveMs=(Median $matching.CacheResolveMs); DrawMs=(Median $matching.DrawMs)
        GpuFrameMs=(Median $matching.GpuFrameMs); MeanRetainedCubes=(Median $matching.MeanRetainedCubes)
    }
}
$summary | Format-Table -AutoSize
[pscustomobject]@{
    CacheMs=$CacheMs; Scene=$Scene; Width=$Width; Height=$Height; Frames=$Frames; Repeats=$Repeats
    Summary=$summary; Runs=$runs
} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $Output 'benchmark-summary.json')
Write-Host "Reports: $Output. GPU timings exclude presentation and CPU overhead."
if (@($runs | Where-Object PresentMode -eq 'fifo').Count) {
    Write-Host 'FIFO fallback may limit application FPS to screen refresh.'
}
if ($runs[0].DeviceType -eq 'cpu') {
    Write-Host 'Software Vulkan: these results do not establish hardware GPU performance.'
}
