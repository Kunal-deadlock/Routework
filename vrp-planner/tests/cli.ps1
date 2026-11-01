$ErrorActionPreference = 'Stop'
# Run from vrp-planner. This tests the actual executable and emitted CSV files.
$exe = Join-Path (Get-Location) 'vrp.exe'
function Run-Case([int]$code, [string[]]$cliArgs) {
    & $exe @cliArgs
    if ($LASTEXITCODE -ne $code) { throw "Expected exit $code, got $LASTEXITCODE for $cliArgs" }
}
Run-Case 0 @('gen','--n','8','--seed','42','--out','results/cli/input.csv')
Run-Case 0 @('solve','--input','results/cli/input.csv','--vehicles','1','--capacity','30','--algo','exact','--routes','results/cli/routes.csv','--svg','results/cli/routes.svg')
Run-Case 0 @('compare','--input','results/cli/input.csv','--vehicles','1','--capacity','30')
Run-Case 1 @('solve','--input','results/cli/input.csv','--algo','unknown')
Run-Case 1 @('solve','--input','results/cli/input.csv','--vehicles','0')
Run-Case 1 @('solve','--input','results/cli/input.csv','--capacity','2')
Run-Case 1 @('gen','--n','-1')
Run-Case 1 @('gen','--n','1','--n','2')
Run-Case 4 @('solve','--input','results/cli/input.csv','--algo','bt','--node-limit','0','--routes','results/cli/limited.csv','--svg','results/cli/limited.svg')
Run-Case 0 @('gen','--n','16','--out','results/cli/big.csv')
Run-Case 2 @('solve','--input','results/cli/big.csv','--vehicles','4','--capacity','20','--algo','exact')
Run-Case 0 @('bench','--experiments','E1,E2,E3,E4,E6,E7,E8','--sizes','5,8','--runs','2','--node-limit','10000','--time-limit-ms','200','--out','results/cli/results.csv')
$rows = @(Import-Csv results/cli/results.csv)
if ($rows.Count -ne 152) { throw 'Unexpected experiment row count' }
if (@($rows | Where-Object { $_.gap_pct -ne '' -and [double]$_.gap_pct -lt -0.000001 }).Count) { throw 'Negative optimality gap' }
if (@($rows | Where-Object { $_.mst_ratio -ne '' -and [double]$_.mst_ratio -lt 0.999999 }).Count) { throw 'MST violation' }
if (@($rows | Where-Object { $_.status -eq 'limit' -and $_.optimal -eq '1' }).Count) { throw 'Search limit claimed optimality' }
foreach ($path in Get-ChildItem results/cli/*.svg) { $xml = [xml](Get-Content -LiteralPath $path.FullName -Raw); if ($xml.DocumentElement.LocalName -ne 'svg') {throw 'Invalid SVG'} }
'PASS CLI exit codes, exports, 152 benchmark rows, gaps, MST ratios, SVG XML'
