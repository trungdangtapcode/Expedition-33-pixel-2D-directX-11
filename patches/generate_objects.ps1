Add-Type -AssemblyName System.Drawing

$width = 192   # 3 columns of 64px
$height = 256  # 4 rows of 64px

$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.Color]::Transparent)

$black = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::Black)
$brown = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::SaddleBrown)
$lightBrown = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::BurlyWood)
$darkBrown = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 60, 30, 10))
$red = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::Firebrick)
$gray = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::DarkGray)
$cyan = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::LightCyan)
$pen = [System.Drawing.Pens]::Black

# === LARGE HOUSE (192x192) ===
$hx = 0
$hy = 0
# Base Walls (spans bottom 128px)
$graphics.FillRectangle($lightBrown, $hx + 16, $hy + 96, 160, 96)
$graphics.DrawRectangle($pen, $hx + 16, $hy + 96, 160, 96)
# Wood struts
$graphics.FillRectangle($darkBrown, $hx + 16, $hy + 144, 160, 8)
$graphics.FillRectangle($darkBrown, $hx + 16, $hy + 96, 16, 96)
$graphics.FillRectangle($darkBrown, $hx + 160, $hy + 96, 16, 96)

# Door
$graphics.FillRectangle($brown, $hx + 80, $hy + 144, 32, 48)
$graphics.DrawRectangle($pen, $hx + 80, $hy + 144, 32, 48)
# Window Left
$graphics.FillRectangle($cyan, $hx + 40, $hy + 110, 24, 24)
$graphics.DrawRectangle($pen, $hx + 40, $hy + 110, 24, 24)
$graphics.DrawLine($pen, $hx + 52, $hy + 110, $hx + 52, $hy + 134)
$graphics.DrawLine($pen, $hx + 40, $hy + 122, $hx + 64, $hy + 122)
# Window Right
$graphics.FillRectangle($cyan, $hx + 128, $hy + 110, 24, 24)
$graphics.DrawRectangle($pen, $hx + 128, $hy + 110, 24, 24)
$graphics.DrawLine($pen, $hx + 140, $hy + 110, $hx + 140, $hy + 134)
$graphics.DrawLine($pen, $hx + 128, $hy + 122, $hx + 152, $hy + 122)

# Roof (spans top 96px)
$roofPts = [System.Drawing.Point[]](
    [System.Drawing.Point]::new($hx + 0, $hy + 96),
    [System.Drawing.Point]::new($hx + 96, $hy + 16),
    [System.Drawing.Point]::new($hx + 192, $hy + 96)
)
$graphics.FillPolygon($red, $roofPts)
$graphics.DrawPolygon($pen, $roofPts)


# === TABLE (x: 0, y: 192) ===
$tx = 0
$ty = 192
$graphics.FillRectangle($brown, $tx + 10, $ty + 24, 44, 12)
$graphics.DrawRectangle($pen, $tx + 10, $ty + 24, 44, 12)
$graphics.FillRectangle($brown, $tx + 16, $ty + 36, 6, 20)
$graphics.DrawRectangle($pen, $tx + 16, $ty + 36, 6, 20)
$graphics.FillRectangle($brown, $tx + 42, $ty + 36, 6, 20)
$graphics.DrawRectangle($pen, $tx + 42, $ty + 36, 6, 20)

# === ROCK (x: 64, y: 192) ===
$rx = 64
$ry = 192
$graphics.FillEllipse($gray, $rx + 12, $ry + 28, 40, 28)
$graphics.DrawEllipse($pen, $rx + 12, $ry + 28, 40, 28)
$graphics.FillEllipse($gray, $rx + 8, $ry + 36, 24, 20)
$graphics.DrawEllipse($pen, $rx + 8, $ry + 36, 24, 20)
$graphics.FillEllipse($gray, $rx + 32, $ry + 32, 24, 24)
$graphics.DrawEllipse($pen, $rx + 32, $ry + 32, 24, 24)

$path = Join-Path (Get-Location).Path "assets\environments\overworld_objects.png"
$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)

$graphics.Dispose()
$bitmap.Dispose()

Write-Host "Generated massive 192x192 House inside overworld_objects.png"
