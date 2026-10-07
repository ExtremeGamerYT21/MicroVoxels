param(
    [string]$Exe = "$PSScriptRoot/../build-windows/Release/microvoxels.exe",
    [ValidateSet('garden', 'test')][string]$Scene = 'garden',
    [ValidateRange(160, 4096)][int]$Width = 1920,
    [ValidateRange(120, 2160)][int]$Height = 1080,
    [ValidateRange(60, 10000)][int]$Frames = 240,
    [ValidateRange(1, 9)][int]$Repeats = 3,
    [ValidateRange(.005, .15)][double]$VoxelSize = .01,
    [ValidateRange(1, 6)][int]$Levels = 5,
    [switch]$PointSplats,
    [switch]$NoAdaptiveLod,
    [switch]$Live,
    [string]$Output = "$PSScriptRoot/../profiles/cube-draw-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$null = New-Item -ItemType Directory -Force -Path $Output
$Output = (Resolve-Path $Output).Path
$configs = @(
    @{Name='legacy-off'; Mesh='legacy'; Cull='off'},
    @{Name='indexed-off'; Mesh='indexed'; Cull='off'},
    @{Name='legacy-back'; Mesh='legacy'; Cull='back'},
    @{Name='indexed-back'; Mesh='indexed'; Cull='back'}
)
$runs = @()
$reference = $null
for ($round = 0; $round -lt $Repeats; ++$round) {
    # Rotate order to reduce warmup/clock bias. Reports retain each raw result.
    for ($j = 0; $j -lt $configs.Count; ++$j) {
        $config = $configs[($j + $round) % $configs.Count]
        $reportPath = Join-Path $Output "$($config.Name)-$round.json"
        $demoArgs = @('--scene', $Scene, '--width', "$Width", '--height', "$Height",
            '--frames', "$Frames", '--time', '1', '--mode', '1', '--hidden', '--no-ui',
            '--voxel-size', $VoxelSize.ToString([Globalization.CultureInfo]::InvariantCulture),
            '--levels', "$Levels", '--cube-mesh', $config.Mesh, '--cube-culling', $config.Cull,
            '--report', $reportPath)
        if (!$Live) { $demoArgs += @('--freeze-after', '8') }
        if ($PointSplats) { $demoArgs += '--point-splats' }
        if ($NoAdaptiveLod) { $demoArgs += '--no-adaptive-lod' }
        Write-Host "Round $($round + 1)/${Repeats}: $($config.Name)"
        & $Exe @demoArgs
        if ($LASTEXITCODE -ne 0) { throw "Demo failed: $($config.Name), exit $LASTEXITCODE" }
        $report = Get-Content -Raw $reportPath | ConvertFrom-Json
        if (!$report.timestamp_supported -or $report.timed_frames_after_warmup -lt 1) {
            throw 'No valid GPU timestamps; cannot compare draw paths.'
        }
        if (!$report.unique_voxels -or $report.dropped_hits -or $report.dropped_root_history_samples) {
            throw 'Empty or overflowing cloud; choose coarser voxel settings for a valid comparison.'
        }
        $signature = "$($report.hits)/$($report.candidate_voxel_writes)/$($report.unique_voxels)/$($report.voxels_per_lod -join ',')"
        if ($null -eq $reference) { $reference = $signature }
        if ($signature -ne $reference) { throw 'Voxel counts changed between draw paths.' }
        $runs += [pscustomobject]@{
            Config=$config.Name; Round=$round; Device=$report.device; DeviceType=$report.device_type
            CubeMs=[double]$report.final_voxel_draw_ms
            RenderMs=[double]$report.average_gpu_timings.final_render_ms
            FrameMs=[double]$report.gpu_frame_ms; Voxels=$report.unique_voxels
            InstanceBytes=$report.used_instance_bytes; Report=$reportPath
        }
    }
}
function Median($values) {
    $sorted = @($values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}
$summary = @()
foreach ($config in $configs) {
    $matching = @($runs | Where-Object Config -eq $config.Name)
    $summary += [pscustomobject]@{
        Config=$config.Name; CubeMs=(Median $matching.CubeMs)
        RenderMs=(Median $matching.RenderMs); FrameMs=(Median $matching.FrameMs)
    }
}
$baseline = $summary[0].RenderMs
foreach ($row in $summary) {
    $change = if ($baseline -gt 0) { 100 * ($row.RenderMs / $baseline - 1) } else { 0 }
    $row | Add-Member -NotePropertyName RenderChangePercent -NotePropertyValue $change
}
$summary | Format-Table -AutoSize
$software = $runs[0].DeviceType -eq 'cpu'
[pscustomobject]@{
    Device=$runs[0].Device; SoftwareVulkan=$software; TimingScope=$(if ($Live) {'live'} else {'frozen'})
    SourceScene=$Scene; Width=$Width; Height=$Height; Frames=$Frames; Repeats=$Repeats
    VoxelSize=$VoxelSize; Levels=$Levels; PointSplats=[bool]$PointSplats
    AdaptiveLod=![bool]$NoAdaptiveLod; Summary=$summary; Runs=$runs
} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $Output 'benchmark-summary.json')
Write-Host "Device: $($runs[0].Device). Reports: $Output"
if ($software) {
    Write-Host 'Software Vulkan: these results do not establish hardware GPU performance.'
} else {
    Write-Host 'Compare RenderMs (includes indexed command setup); negative change is faster.'
}
