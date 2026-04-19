# Memory-efficient Frontend patcher
$frontendPath = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\Frontend-uncompressed.upk'
$patchedMainV2Path = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\MainV2-patched.gfx'
$outputPath = 'C:\dev\helenhook\games\HelenBatmanAA\builder\generated\graphics-options-experiment\Frontend-final.upk'

Write-Output "Loading files..."
$frontend = [System.IO.File]::ReadAllBytes($frontendPath)
$patchedMainV2 = [System.IO.File]::ReadAllBytes($patchedMainV2Path)

Write-Output "Frontend: $($frontend.Length) bytes"
Write-Output "Patched MainV2: $($patchedMainV2.Length) bytes"

# Find MainV2 offset
$mainV2Offset = -1
for ($i = 0; $i -lt [Math]::Min($frontend.Length, 200000); $i++) {
    if ($frontend[$i] -eq 0x47 -and $frontend[$i+1] -eq 0x46 -and $frontend[$i+2] -eq 0x58) {
        $fileSize = [System.BitConverter]::ToUInt32($frontend, $i + 4)
        if ($fileSize -gt 100000 -and $fileSize -lt 2000000) {
            $mainV2Offset = $i
            Write-Output "Found MainV2 at offset $mainV2Offset, size=$fileSize"
            break
        }
    }
}

if ($mainV2Offset -eq -1) { 
    Write-Error 'MainV2 not found'
    exit 1 
}

$currentSize = [System.BitConverter]::ToUInt32($frontend, $mainV2Offset + 4)
$sizeDiff = $patchedMainV2.Length - $currentSize
Write-Output "Size diff: $sizeDiff bytes (patched is $([Math]::Abs($sizeDiff)) bytes $((if ($sizeDiff -gt 0) {'larger'} else {'smaller'})))"

# Use streams to avoid loading everything into memory
$reader = New-Object System.IO.BinaryReader([System.IO.File]::OpenRead($frontendPath))
$writer = New-Object System.IO.BinaryWriter([System.IO.File]::OpenWrite($outputPath))

# Copy data before MainV2
Write-Output "Copying data before MainV2..."
$buffer = New-Object byte[] 65536
$bytesToCopy = $mainV2Offset
$bytesCopied = 0
while ($bytesCopied -lt $bytesToCopy) {
    $toRead = [Math]::Min($buffer.Length, $bytesToCopy - $bytesCopied)
    $read = $reader.Read($buffer, 0, $toRead)
    if ($read -le 0) { break }
    $writer.Write($buffer, 0, $read)
    $bytesCopied += $read
}

# Write patched MainV2
Write-Output "Writing patched MainV2..."
$writer.Write($patchedMainV2)

# Skip original MainV2
$reader.BaseStream.Seek($mainV2Offset + $currentSize, [System.IO.SeekOrigin]::Begin) | Out-Null

# Update size in MainV2 header (already done in patched file, but we need to ensure it's correct)
# Actually we already wrote the full patched file, so size is already correct

# Copy remaining data
Write-Output "Copying remaining data..."
$remainingBytes = $reader.BaseStream.Length - $reader.BaseStream.Position
$bytesCopied = 0
while ($bytesCopied -lt $remainingBytes) {
    $toRead = [Math]::Min($buffer.Length, $remainingBytes - $bytesCopied)
    $read = $reader.Read($buffer, 0, $toRead)
    if ($read -le 0) { break }
    $writer.Write($buffer, 0, $read)
    $bytesCopied += $read
}

# Now fix headers
$writer.Flush()
$writer.Close()
$reader.Close()

# Re-open to fix headers
Write-Output "Fixing headers..."
$fileBytes = [System.IO.File]::ReadAllBytes($outputPath)

# Update MainV2 size
[System.BitConverter]::GetBytes([uint32]$patchedMainV2.Length).CopyTo($fileBytes, $mainV2Offset + 4)

# Clear compression flags
[System.BitConverter]::GetBytes([uint32]0).CopyTo($fileBytes, 89)
[System.BitConverter]::GetBytes([int32]0).CopyTo($fileBytes, 93)

# Update PackageSize
[System.BitConverter]::GetBytes($fileBytes.Length).CopyTo($fileBytes, 8)

[System.IO.File]::WriteAllBytes($outputPath, $fileBytes)
Write-Output "Final Frontend saved: $($fileBytes.Length) bytes"
