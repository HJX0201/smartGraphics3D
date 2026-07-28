param(
    [Parameter(Mandatory = $true)]
    [string]$JsonPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$report = Get-Content -LiteralPath $JsonPath -Raw | ConvertFrom-Json

function Format-MiB([double]$bytes)
{
    return '{0:N2}' -f ($bytes / 1MB)
}

function Format-Number([double]$value)
{
    return '{0:N2}' -f $value
}

function Format-Percent([double]$before, [double]$after)
{
    if ($before -eq 0)
    {
        return '—'
    }
    return '{0:N2}%' -f ((1.0 - $after / $before) * 100.0)
}

$summaryRows = @()
$counts = @($report.summary | Select-Object -ExpandProperty count -Unique | Sort-Object)
foreach ($count in $counts)
{
    $independent = $report.summary |
        Where-Object { $_.count -eq $count -and $_.mode -eq 'independent' } |
        Select-Object -First 1
    $shared = $report.summary |
        Where-Object { $_.count -eq $count -and $_.mode -eq 'shared' } |
        Select-Object -First 1
    foreach ($entry in @($independent, $shared))
    {
        $mode = if ($entry.mode -eq 'independent') { '普通复制' } else { '共享实例' }
        $summaryRows += @"
<tr>
  <td>$count</td><td>$mode</td>
  <td>$(Format-Number $entry.creationMs)</td>
  <td>$(Format-Number $entry.firstDisplayMs)</td>
  <td>$(Format-Number $entry.redraw30Ms)</td>
  <td>$(Format-MiB $entry.privateBytes)</td>
  <td>$(Format-MiB $entry.workingSetBytes)</td>
  <td>$(Format-MiB $entry.estimatedGpuGeometryBytes)</td>
  <td>$($entry.independentPresentations) / $($entry.sharedPrototypes) / $($entry.connectedInstances)</td>
  <td>$('{0:N0}' -f $entry.renderedTriangles)</td>
</tr>
"@
    }
}

$reductionRows = @()
foreach ($count in $counts)
{
    $independent = $report.summary |
        Where-Object { $_.count -eq $count -and $_.mode -eq 'independent' } |
        Select-Object -First 1
    $shared = $report.summary |
        Where-Object { $_.count -eq $count -and $_.mode -eq 'shared' } |
        Select-Object -First 1
    $reductionRows += @"
<tr>
  <td>$count</td>
  <td>$(Format-Percent $independent.privateBytes $shared.privateBytes)</td>
  <td>$(Format-Percent $independent.workingSetBytes $shared.workingSetBytes)</td>
  <td>$(Format-Percent $independent.estimatedGpuGeometryBytes $shared.estimatedGpuGeometryBytes)</td>
  <td>$(Format-Percent $independent.firstDisplayMs $shared.firstDisplayMs)</td>
  <td>$(Format-Percent $independent.redraw30Ms $shared.redraw30Ms)</td>
</tr>
"@
}

$runRows = foreach ($run in $report.runs)
{
    $mode = if ($run.mode -eq 'independent') { '普通复制' } else { '共享实例' }
    @"
<tr>
  <td>$($run.count)</td><td>$mode</td>
  <td>$(Format-Number $run.firstDisplayMs)</td>
  <td>$(Format-Number $run.redraw30Ms)</td>
  <td>$(Format-MiB $run.privateBytes)</td>
  <td>$(Format-MiB $run.workingSetBytes)</td>
  <td>$(Format-MiB $run.estimatedGpuGeometryBytes)</td>
</tr>
"@
}

$dataset = $report.dataset
$generated = [DateTime]::Parse($report.environment.timestamp).ToLocalTime().ToString('yyyy-MM-dd HH:mm:ss')
$html = @"
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>smartGraphics3D 共享显示实例对比报告</title>
<style>
:root { color-scheme: dark; --bg:#11151b; --panel:#1a2029; --line:#34404f;
  --text:#e7edf5; --muted:#9eabb9; --accent:#55c2ff; --good:#59d99d; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text);
  font:14px/1.55 "Segoe UI","Microsoft YaHei",sans-serif; }
main { max-width:1280px; margin:auto; padding:32px 24px 64px; }
h1 { font-size:30px; margin:0 0 8px; }
h2 { margin:30px 0 12px; font-size:20px; }
p,li { color:var(--muted); }
.tag { display:inline-block; margin:4px 8px 4px 0; padding:4px 10px;
  border:1px solid var(--line); border-radius:20px; color:var(--accent); }
.panel { background:var(--panel); border:1px solid var(--line); border-radius:10px;
  padding:18px; overflow:auto; }
.result { color:var(--good); font-size:18px; font-weight:600; }
table { border-collapse:collapse; width:100%; min-width:900px; }
th,td { padding:9px 10px; border-bottom:1px solid var(--line); text-align:right; }
th { color:var(--muted); font-weight:600; }
th:nth-child(2),td:nth-child(2) { text-align:left; }
code { color:#bedfff; word-break:break-all; }
</style>
</head>
<body><main>
<h1>共享显示实例对比报告</h1>
<p>smartGraphics3D / OCCT AIS_ConnectedInteractive / 固定网格质量</p>
<span class="tag">$($report.environment.configuration)</span>
<span class="tag">$($report.environment.os)</span>
<span class="tag">Qt $($report.environment.qtVersion)</span>
<span class="tag">OCCT $($report.environment.occtVersion)</span>
<span class="tag">$generated</span>

<h2>结论</h2>
<div class="panel">
  <p class="result">一个包含 $($dataset.entityCount) 个实体、每份
  $('{0:N0}' -f $dataset.trianglesPerSet) 个三角形的大模型，仅复制 1、2、5、10 次。</p>
  <p>普通复制为每个对象建立独立 AIS_Shape Presentation；共享模式保留一个原型并用
  Connected 实例复用显示几何。两种模式在每个数量档位的展开三角形完全相同。</p>
</div>

<h2>中位数结果</h2>
<div class="panel"><table>
<thead><tr><th>份数</th><th>模式</th><th>创建 ms</th><th>首显 ms</th>
<th>30 帧 ms</th><th>Private MiB</th><th>Working MiB</th>
<th>GPU 估算 MiB</th><th>独立/原型/实例</th><th>展开三角形</th></tr></thead>
<tbody>$($summaryRows -join "`n")</tbody>
</table></div>

<h2>共享模式相对减少</h2>
<div class="panel"><table>
<thead><tr><th>份数</th><th>Private Bytes</th><th>Working Set</th>
<th>GPU 几何估算</th><th>首次显示</th><th>30 帧重绘</th></tr></thead>
<tbody>$($reductionRows -join "`n")</tbody>
</table></div>

<h2>24 次原始运行</h2>
<div class="panel"><table>
<thead><tr><th>份数</th><th>模式</th><th>首显 ms</th><th>30 帧 ms</th>
<th>Private MiB</th><th>Working MiB</th><th>GPU 估算 MiB</th></tr></thead>
<tbody>$($runRows -join "`n")</tbody>
</table></div>

<h2>测试集与口径</h2>
<div class="panel"><ul>
<li>确定性 BREP：<code>$($dataset.path)</code></li>
<li>大小：$('{0:N0}' -f $dataset.sizeBytes) B；SHA-256：
<code>$($dataset.sha256)</code></li>
<li>模型内实体数：$($dataset.entityCount)；每份三角形：
$('{0:N0}' -f $dataset.trianglesPerSet)。</li>
<li>每个模式运行 3 次，每次均为全新进程和真实 Windows/OpenGL 视口，表中取中位数。</li>
<li>基准关闭交互式渐进降质，确保普通与共享模式使用相同固定网格；应用默认行为不变。</li>
<li>Private Bytes/Working Set 来自 Windows 进程计数器；GPU 估算按实际展开三角形和共享
结构计算；OCCT 原始统计保留在 JSON。</li>
</ul></div>
</main></body></html>
"@

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory)
{
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}
[System.IO.File]::WriteAllText($OutputPath, $html, [System.Text.UTF8Encoding]::new($false))
