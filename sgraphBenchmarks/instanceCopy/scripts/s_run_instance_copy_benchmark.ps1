[CmdletBinding()]
param(
    [string]$BenchmarkPath,
    [ValidateSet('Heavy', 'Standard', 'Count')]
    [string]$Suite = 'Heavy',
    [ValidateRange(1, 10)]
    [int]$Count = 1
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($BenchmarkPath))
{
    $BenchmarkPath = Join-Path $PSScriptRoot `
        '..\..\..\build\64\release\benchmarks\sgraphInstanceCopyBenchmark.exe'
}
$benchmark = (Resolve-Path -LiteralPath $BenchmarkPath).Path
$results = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\results'))
New-Item -ItemType Directory -Path $results -Force | Out-Null

$date = Get-Date -Format 'yyyyMMdd'
$sequence = 1
do
{
    $output = Join-Path $results "$date-$sequence"
    $sequence++
}
while (Test-Path -LiteralPath $output)
New-Item -ItemType Directory -Path $output | Out-Null

$arguments = switch ($Suite)
{
    'Heavy' { @('--suite-heavy', $output) }
    'Standard' { @('--suite', $output) }
    'Count' { @('--suite-count', $output, $Count) }
}

$completed = $false
try
{
    & $benchmark @arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "基准运行失败，退出码：$LASTEXITCODE"
    }

    $jsonPath = Join-Path $output 'benchmark.json'
    $reportPath = Join-Path $output 'report.html'
    & (Join-Path $PSScriptRoot 's_instance_copy_report_html.ps1') `
        -JsonPath $jsonPath `
        -OutputPath $reportPath

    Remove-Item -LiteralPath (Join-Path $output 'raw') -Recurse
    Remove-Item -LiteralPath $jsonPath
    $completed = $true
    Write-Output "测试集：$(Join-Path $output 'dataset.brep')"
    Write-Output "测试报告：$reportPath"
}
finally
{
    if (!$completed -and (Test-Path -LiteralPath $output))
    {
        Remove-Item -LiteralPath $output -Recurse -Force
    }
}
