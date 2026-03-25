param(
    [string]$ProjectRoot  = "D:\LYH\UE",
    [string]$ProjectFile  = "D:\LYH\UE\W0\W0.uproject",
    [string]$Target       = "W0Editor",
    [string]$Configuration = "Development"
)

$ErrorActionPreference = "Continue"
$buildBat = Join-Path $ProjectRoot "Engine\Build\BatchFiles\Build.bat"

if (-not (Test-Path $buildBat)) {
    Write-Error "Build.bat not found: $buildBat"
    exit 1
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "[UECLI] Building $Target $Configuration" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

$buildOutput = & $buildBat $Target Win64 $Configuration $ProjectFile -WaitMutex -NoHotReloadFromIDE 2>&1
$exitCode = $LASTEXITCODE

# Output everything
$buildOutput | ForEach-Object { Write-Host $_ }

# Extract and summarize errors
$errors = $buildOutput | Where-Object { $_ -match "error\s*(C|LNK)\d+" }
$warnings = $buildOutput | Where-Object { $_ -match "warning\s*(C|LNK)\d+" }

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
if ($exitCode -eq 0) {
    Write-Host "[UECLI] BUILD SUCCEEDED" -ForegroundColor Green
} else {
    Write-Host "[UECLI] BUILD FAILED (exit code: $exitCode)" -ForegroundColor Red
    if ($errors.Count -gt 0) {
        Write-Host ""
        Write-Host "[UECLI] Errors ($($errors.Count)):" -ForegroundColor Red
        $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    }
}
if ($warnings.Count -gt 0) {
    Write-Host "[UECLI] Warnings ($($warnings.Count)):" -ForegroundColor Yellow
    $warnings | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
}
Write-Host "============================================" -ForegroundColor Cyan

exit $exitCode
