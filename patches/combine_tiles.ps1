Add-Type -AssemblyName System.Drawing

$grass = "C:\Users\MY LAPTOP\.gemini\antigravity\brain\a6375de8-0562-4d0f-b660-12540f92c22c\grass_tile_1776801836845.png"
$dirt = "C:\Users\MY LAPTOP\.gemini\antigravity\brain\a6375de8-0562-4d0f-b660-12540f92c22c\dirt_tile_1776801851007.png"
$stone = "C:\Users\MY LAPTOP\.gemini\antigravity\brain\a6375de8-0562-4d0f-b660-12540f92c22c\stone_tile_1776801865576.png"
$outPath = "D:\lab\vscworkplace\directX\assets\environments\overworld_tiles.png"

$bmpOut = New-Object System.Drawing.Bitmap 384, 128
$graphics = [System.Drawing.Graphics]::FromImage($bmpOut)

$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

$imgGrass = [System.Drawing.Image]::FromFile($grass)
$imgDirt = [System.Drawing.Image]::FromFile($dirt)
$imgStone = [System.Drawing.Image]::FromFile($stone)

$graphics.DrawImage($imgGrass, 0, 0, 128, 128)
$graphics.DrawImage($imgDirt, 128, 0, 128, 128)
$graphics.DrawImage($imgStone, 256, 0, 128, 128)

$bmpOut.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)

$graphics.Dispose()
$imgGrass.Dispose()
$imgDirt.Dispose()
$imgStone.Dispose()
$bmpOut.Dispose()

Write-Output "Combined tiles saved successfully!"
