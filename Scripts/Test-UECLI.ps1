param(
    [string]$ProjectRoot  = "D:\LYH\UE",
    [string]$ProjectFile  = "D:\LYH\UE\W0\W0.uproject",
    [string]$TestFilter   = "UECLI",
    [int]$Port            = 31111,
    [switch]$PingOnly
)

$ErrorActionPreference = "Continue"
$editorCmd = Join-Path $ProjectRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

# --- Ping test: just check if the TCP server is alive ---
if ($PingOnly) {
    Write-Host "[UECLI] Ping test -> 127.0.0.1:$Port" -ForegroundColor Cyan
    try {
        $client = [System.Net.Sockets.TcpClient]::new()
        $client.Connect("127.0.0.1", $Port)
        $stream = $client.GetStream()
        $json = '{"command":"ping","params":{}}'
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush()
        $buf = New-Object byte[] 4096
        $stream.ReadTimeout = 5000
        $total = ""
        try {
            while (($n = $stream.Read($buf, 0, $buf.Length)) -gt 0) {
                $total += [System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
            }
        } catch {}
        $client.Close()
        if ($total -match '"pong"') {
            Write-Host "[UECLI] PING OK: $total" -ForegroundColor Green
            exit 0
        } else {
            Write-Host "[UECLI] PING UNEXPECTED: $total" -ForegroundColor Yellow
            exit 1
        }
    } catch {
        Write-Host "[UECLI] PING FAILED: Server not reachable on port $Port" -ForegroundColor Red
        Write-Host "  $_" -ForegroundColor Red
        exit 1
    }
}

# --- Full automation test ---
if (-not (Test-Path $editorCmd)) {
    Write-Error "UnrealEditor-Cmd.exe not found: $editorCmd"
    exit 1
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "[UECLI] Running automation tests: $TestFilter" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

& $editorCmd $ProjectFile `
    "-ExecCmds=Automation RunTests $TestFilter; Quit" `
    -unattended -nop4 -nosplash -nullrhi `
    "-testexit=Automation Test Queue Empty"

$exitCode = $LASTEXITCODE

Write-Host ""
if ($exitCode -eq 0) {
    Write-Host "[UECLI] TESTS PASSED" -ForegroundColor Green
} else {
    Write-Host "[UECLI] TESTS FAILED (exit code: $exitCode)" -ForegroundColor Red
}

exit $exitCode
