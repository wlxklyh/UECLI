param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Command,

    [Parameter(Position=1)]
    [string]$Params = "{}",

    [int]$Port = 31111,
    [int]$Timeout = 10000
)

$json = @{ command = $Command; params = ($Params | ConvertFrom-Json) } | ConvertTo-Json -Depth 10 -Compress

try {
    $client = [System.Net.Sockets.TcpClient]::new("127.0.0.1", $Port)
    $stream = $client.GetStream()
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
    $buf = New-Object byte[] 65536
    $stream.ReadTimeout = $Timeout
    $total = ""
    try {
        while (($n = $stream.Read($buf, 0, $buf.Length)) -gt 0) {
            $total += [System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
        }
    } catch {}
    $client.Close()
    # Pretty print
    $total | ConvertFrom-Json | ConvertTo-Json -Depth 20
} catch {
    Write-Error "Failed to connect to UECLI server on port ${Port}: $_"
    exit 1
}
