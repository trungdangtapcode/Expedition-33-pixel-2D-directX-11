Add-Type -AssemblyName System.Drawing

$width = 512   # 8 tiles (64x64 each)
$height = 64
$tileSize = 64

$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.Color]::Black)

# Grass Colors
$grassBase = [System.Drawing.Color]::FromArgb(255, 34, 139, 34)       # Forest Green
$grassDark = [System.Drawing.Color]::FromArgb(255, 0, 100, 0)         # Dark Green
$grassHighlight = [System.Drawing.Color]::FromArgb(255, 50, 205, 50)  # Lime Green

# Dirt Colors
$dirtBase = [System.Drawing.Color]::FromArgb(255, 139, 69, 19)        # Saddle Brown
$dirtDark = [System.Drawing.Color]::FromArgb(255, 101, 67, 33)        # Dark Brown
$dirtLight = [System.Drawing.Color]::FromArgb(255, 205, 133, 63)       # Peru

$rand = New-Object System.Random

for ($t = 0; $t -lt 8; $t++) {
    $xOffset = $t * $tileSize
    $isDirt = ($t -ge 4) # tiles 0-3 are grass, 4-7 are dirt
    
    $baseColor = if ($isDirt) { $dirtBase } else { $grassBase }
    $darkColor = if ($isDirt) { $dirtDark } else { $grassDark }
    $lightColor = if ($isDirt) { $dirtLight } else { $grassHighlight }
    
    # Fill Base
    $brush = New-Object System.Drawing.SolidBrush($baseColor)
    $graphics.FillRectangle($brush, $xOffset, 0, $tileSize, $tileSize)
    $brush.Dispose()
    
    # Draw Pixel Noise / Square Features
    for ($px = 0; $px -lt $tileSize; $px += 8) {
        for ($py = 0; $py -lt $tileSize; $py += 8) {
            $r = $rand.Next(0, 100)
            if ($r -lt 25) {
                $b = New-Object System.Drawing.SolidBrush($darkColor)
                $graphics.FillRectangle($b, $xOffset + $px, $py, 8, 8)
                $b.Dispose()
            } elseif ($r -gt 85) {
                $b = New-Object System.Drawing.SolidBrush($lightColor)
                $graphics.FillRectangle($b, $xOffset + $px, $py, 8, 8)
                $b.Dispose()
            }
        }
    }
    
    # Draw A perfectly square border on the tile to define the grid naturally
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(80, 0, 0, 0))
    $graphics.DrawRectangle($pen, $xOffset, 0, $tileSize - 1, $tileSize - 1)
    $pen.Dispose()
}

$path = Join-Path (Get-Location).Path "assets\environments\overworld_tiles.png"
$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)

$graphics.Dispose()
$bitmap.Dispose()

Write-Host "Generated assets\environments\overworld_tiles.png"
