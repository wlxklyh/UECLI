param(
    [int]$Port = 31111,
    [int]$Timeout = 5000,
    [int]$RetryCount = 3,
    [int]$RetryDelay = 2
)

$ErrorActionPreference = "Continue"
$passed = 0
$failed = 0
$total = 0

function Send-UECLICommand {
    param([string]$Json, [int]$Port, [int]$Timeout)
    try {
        $client = [System.Net.Sockets.TcpClient]::new("127.0.0.1", $Port)
        $stream = $client.GetStream()
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Json)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush()
        $buf = New-Object byte[] 65536
        $stream.ReadTimeout = $Timeout
        $result = ""
        try {
            while (($n = $stream.Read($buf, 0, $buf.Length)) -gt 0) {
                $result += [System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
            }
        } catch {}
        $client.Close()
        return $result
    } catch {
        return $null
    }
}

function Test-Command {
    param([string]$Name, [string]$Json, [string]$ExpectContains)
    $script:total++
    $result = Send-UECLICommand -Json $Json -Port $Port -Timeout $Timeout
    if ($null -eq $result) {
        Write-Host "  FAIL  $Name - connection refused" -ForegroundColor Red
        $script:failed++
        return $false
    }
    if ($result -match $ExpectContains) {
        Write-Host "  PASS  $Name" -ForegroundColor Green
        $script:passed++
        return $true
    } else {
        Write-Host "  FAIL  $Name - unexpected response: $($result.Substring(0, [Math]::Min(200, $result.Length)))" -ForegroundColor Red
        $script:failed++
        return $false
    }
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "[UECLI] Smoke Test -> 127.0.0.1:$Port" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# Wait for server with retry
$serverReady = $false
for ($i = 1; $i -le $RetryCount; $i++) {
    $ping = Send-UECLICommand -Json '{"command":"ping","params":{}}' -Port $Port -Timeout $Timeout
    if ($ping -and $ping -match '"pong"') {
        $serverReady = $true
        break
    }
    if ($i -lt $RetryCount) {
        Write-Host "  Server not ready, retry $i/$RetryCount in ${RetryDelay}s..." -ForegroundColor Yellow
        Start-Sleep -Seconds $RetryDelay
    }
}

if (-not $serverReady) {
    Write-Host ""
    Write-Host "[UECLI] SMOKE FAILED - Server not reachable on port $Port" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[Core]" -ForegroundColor White
Test-Command "ping" '{"command":"ping","params":{}}' '"pong"'
Test-Command "list_tools" '{"command":"list_tools","params":{}}' '"tools"'

Write-Host ""
Write-Host "[Editor]" -ForegroundColor White
Test-Command "get_current_level" '{"command":"get_current_level","params":{}}' '"success"'
Test-Command "get_editor_state" '{"command":"get_editor_state","params":{}}' '"success"'

Write-Host ""
Write-Host "[Asset]" -ForegroundColor White
Test-Command "list_assets /Game" '{"command":"list_assets","params":{"path":"/Game","recursive":false}}' '"success"'

Write-Host ""
Write-Host "[Project]" -ForegroundColor White
Test-Command "get_project_info" '{"command":"get_project_info","params":{}}' '"success"'

Write-Host ""
Write-Host "[Material]" -ForegroundColor White
Test-Command "list_materials" '{"command":"list_materials","params":{"path":"/Game"}}' '"success"'

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
if ($failed -eq 0) {
    Write-Host "[UECLI] SMOKE PASSED ($passed/$total)" -ForegroundColor Green
} else {
    Write-Host "[UECLI] SMOKE FAILED ($passed passed, $failed failed, $total total)" -ForegroundColor Red
}
Write-Host "============================================" -ForegroundColor Cyan

exit $failed
