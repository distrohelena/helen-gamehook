param(
    [string]$ExecutablePath = 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
)
$ErrorActionPreference = 'Stop'
$bytes = [IO.File]::ReadAllBytes($ExecutablePath)
# These are file offsets, not process addresses. This script never opens a process for writing.
$contracts = @(
    @{ Name = 'clear movie-owned result before dispatch'; Offset = 0x15FD997; Hex = '8B7C241481C7DC0900008BCFE8A886F7FFC60700' },
    @{ Name = 'handler lookup and four-argument virtual call'; Offset = 0x15FD9CA; Hex = '8B89D40000008B118B520453508B442418508B44242050FFD2' },
    @{ Name = 'copy movie-owned result back to ActionScript'; Offset = 0x15FD9E3; Hex = '8B4E0457E89487F7FF' },
    @{ Name = 'boolean copy payload at offset four'; Offset = 0x15761AF; Hex = '8A4F045F884E045E83C40CC20400' },
    @{ Name = 'converted argument stride is sixteen bytes'; Offset = 0x15FD98A; Hex = '8BC583C7103BC372CD' },
    @{ Name = 'converted numeric argument is type three with double at offset eight'; Offset = 0x1552FCA; Hex = '8B54241052E84C370200DD5E085FC706030000005E5BC20C00' },
    @{ Name = 'numeric return copy uses double at offset four'; Offset = 0x15761CF; Hex = 'DD47045FDD5E045E83C40CC20400' },
    @{ Name = 'return types two and three select boolean and double copy branches'; Offset = 0x1576284; Hex = 'AF619701CF619701' },
    @{ Name = 'uncoerced GAS number tags three and four convert to numeric arguments'; Offset = 0x15530E8; Hex = '792F9501842F95017D2F95018B2F95018B2F9501' }
)
foreach ($contract in $contracts) {
    $length = $contract.Hex.Length / 2
    if ($bytes.Length -lt $contract.Offset + $length) { throw "Truncated executable: $($contract.Name)" }
    $actual = [BitConverter]::ToString($bytes, $contract.Offset, $length).Replace('-', '')
    if ($actual -cne $contract.Hex) { throw "Unverified executable contract: $($contract.Name), found $actual" }
    Write-Output "MATCH: $($contract.Name)"
}
Write-Output ('Executable SHA256: ' + (Get-FileHash -LiteralPath $ExecutablePath -Algorithm SHA256).Hash)
Write-Output 'STATIC_DISPATCH_CONTRACT_PASS (not a live hook test)'
