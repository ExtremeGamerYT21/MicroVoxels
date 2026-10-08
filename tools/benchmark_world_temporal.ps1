param(
    [string]$Exe = "$PSScriptRoot/../build-windows/Release/microvoxels.exe",
    [int]$Width = 1280, [int]$Height = 720, [int]$Frames = 120,
    [ValidateRange(1, 9)][int]$Repeats = 3,
    [string]$Output = "$PSScriptRoot/../profiles/world-temporal-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$null = New-Item -ItemType Directory -Force -Path $Output
$Output = (Resolve-Path $Output).Path
$runs = @()
for ($round = 0; $round -lt $Repeats; ++$round) {
    $configs = @('sphere-history', 'sphere-current', 'raster-history')
    if ($round % 2) { [array]::Reverse($configs) }
    foreach ($config in $configs) {
        $source = if ($config.StartsWith('sphere')) { 'sphere' } else { 'raster' }
        $path = Join-Path $Output "$config-$round.json"
        $demoArgs = @('--source', $source, '--taa', '--scene', 'garden', '--mode', '1',
            '--width', "$Width", '--height', "$Height", '--frames', "$Frames",
            '--no-ui', '--no-vsync', '--cache-ms', '200', '--report', $path)
        if ($config -eq 'sphere-current') { $demoArgs += '--no-voxel-cache' }
        Write-Host "Round $($round + 1)/${Repeats}: $config"
        & $Exe @demoArgs
        if ($LASTEXITCODE -ne 0) { throw "Demo failed: exit $LASTEXITCODE" }
        $r = Get-Content -Raw $path | ConvertFrom-Json
        if (!$r.timestamp_supported -or !$r.timed_frames_after_warmup -or !$r.unique_voxels) {
            throw 'Empty cloud or no valid GPU timings.'
        }
        if ($r.dropped_hits -or $r.dropped_root_history_samples -or $r.cache_dropped_voxels -or $r.cache_capacity_resets) {
            throw 'Voxel/history capacity exceeded; lower source resolution for this comparison.'
        }
        $runs += [pscustomobject]@{
            Config=$config; Round=$round; Device=$r.device; DeviceType=$r.device_type
            SourceMs=[double]$r.average_gpu_timings.source_and_shadow_ms
            GenerationMs=[double]$r.gpu_generation_ms; HistoryMs=[double]$r.average_gpu_timings.cache_resolve_ms
            DrawMs=[double]$r.final_voxel_draw_ms; GpuFrameMs=[double]$r.gpu_frame_ms
            ExhaustedRays=$r.source_raymarch_exhausted_total; Report=$path
        }
    }
}
function Median($values) {
    $sorted = @($values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}
$summary = foreach ($config in @('sphere-current', 'sphere-history', 'raster-history')) {
    $matching = @($runs | Where-Object Config -eq $config)
    [pscustomobject]@{
        Config=$config; SourceMs=(Median $matching.SourceMs)
        GenerationMs=(Median $matching.GenerationMs); HistoryMs=(Median $matching.HistoryMs)
        DrawMs=(Median $matching.DrawMs); GpuFrameMs=(Median $matching.GpuFrameMs)
    }
}
$summary | Format-Table -AutoSize
[pscustomobject]@{Summary=$summary; Runs=$runs} | ConvertTo-Json -Depth 6 |
    Set-Content -Encoding UTF8 (Join-Path $Output 'benchmark-summary.json')
Write-Host "Reports: $Output. GPU frame time excludes presentation and CPU overhead."
if ($runs[0].DeviceType -eq 'cpu') { Write-Host 'Software Vulkan; hardware GPU performance remains unmeasured.' }
