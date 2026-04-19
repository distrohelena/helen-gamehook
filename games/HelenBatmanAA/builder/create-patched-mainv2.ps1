$extractedPath = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\MainV2-extracted.gfx'
$deltaPath = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\MainV2-graphics-options.delta'
$outputPath = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\MainV2-patched.gfx'

$extracted = [System.IO.File]::ReadAllBytes($extractedPath)
Write-Output "Extracted MainV2: $($extracted.Length) bytes"

$reader = New-Object System.IO.BinaryReader([System.IO.File]::OpenRead($deltaPath))
$changeCount = $reader.ReadInt32()
Write-Output "Delta changes: $changeCount"

$result = [byte[]]$extracted.Clone()

for ($i = 0; $i -lt $changeCount; $i++) {
    $offset = $reader.ReadInt32()
    $length = $reader.ReadInt32()
    $data = $reader.ReadBytes($length)
    
    if ($offset + $length -gt $result.Length) {
        [Array]::Resize([ref]$result, $offset + $length)
    }
    
    [Array]::Copy($data, 0, $result, $offset, $length)
}

$reader.Close()

[System.IO.File]::WriteAllBytes($outputPath, $result)
Write-Output "Patched MainV2 saved: $($result.Length) bytes"
