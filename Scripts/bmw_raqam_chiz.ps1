# Scripts/mashina_raqam_chiz.ps1 uslubida bitta plita (512x128): Saved/MashinaRaqam/T_Raqam_BMW.png
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# BMW M5 (2 ta, turgan): chiroyli raqam - ikkala mashinaning old va orqa plitasida. 131-son qaror formati.
$numbers = @('01 S 100 SS')
if ($numbers -notmatch '^\d{2} [A-Z] \d{3} [A-Z]{2}$') { throw 'Davlat raqami formati xato.' }

$output = Join-Path (Split-Path $PSScriptRoot) 'Saved/MashinaRaqam'
New-Item -Path $output -ItemType Directory -Force | Out-Null
$image = [Drawing.Bitmap]::new(512, 128)
$graphics = [Drawing.Graphics]::FromImage($image)
$graphics.SmoothingMode = 'AntiAlias'
$plateFont = [Drawing.Font]::new('Bahnschrift SemiCondensed', 67, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$regionFont = [Drawing.Font]::new('Bahnschrift SemiCondensed', 60, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$uzFont = [Drawing.Font]::new('Bahnschrift', 29, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$white = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#f2f3ed'))
$ink = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#101216'))
$blue = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#0099b5'))
$green = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#1eb53a'))
$red = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#ce1126'))
$border = [Drawing.Pen]::new($ink.Color, 3.5)
function Draw-CenteredText($text, $font, $brush, $centerX, $centerY) {
    $glyphs = [Drawing.Drawing2D.GraphicsPath]::new([Drawing.Drawing2D.FillMode]::Winding)
    $glyphs.AddString($text, $font.FontFamily, [int]$font.Style, $font.Size,
        [Drawing.PointF]::Empty, [Drawing.StringFormat]::GenericTypographic)
    $bounds = $glyphs.GetBounds()
    $offset = [Drawing.Drawing2D.Matrix]::new()
    $offset.Translate($centerX - $bounds.X - $bounds.Width / 2, $centerY - $bounds.Y - $bounds.Height / 2)
    $glyphs.Transform($offset)
    $graphics.FillPath($brush, $glyphs)
    $offset.Dispose()
    $glyphs.Dispose()
}

for ($i = 0; $i -lt $numbers.Count; $i++) {
    $graphics.ResetTransform()
    $graphics.TranslateTransform(512 * ($i % 4), 128 * [Math]::Floor($i / 4))
    $graphics.FillRectangle($white, 0, 0, 512, 128)
    $outline = [Drawing.Drawing2D.GraphicsPath]::new()
    $outline.AddArc(20, 16, 16, 16, 180, 90)
    $outline.AddArc(476, 16, 16, 16, 270, 90)
    $outline.AddArc(476, 96, 16, 16, 0, 90)
    $outline.AddArc(20, 96, 16, 16, 90, 90)
    $outline.CloseFigure()
    $graphics.DrawPath($border,$outline)
    $outline.Dispose()
    $graphics.DrawLine($border, 112, 17, 112, 110)
    Draw-CenteredText $numbers[$i].Substring(0,2) $regionFont $ink 66 64
    Draw-CenteredText $numbers[$i].Substring(3) $plateFont $ink 270.5 64

    $graphics.FillRectangle($blue, 431, 32, 52, 9)
    $graphics.FillRectangle($red, 431, 41, 52, 1)
    $graphics.FillRectangle([Drawing.Brushes]::White, 431, 42, 52, 8)
    $graphics.FillRectangle($red, 431, 50, 52, 1)
    $graphics.FillRectangle($green, 431, 51, 52, 9)
    $graphics.FillEllipse([Drawing.Brushes]::White, 434, 33, 7, 7)
    $graphics.FillEllipse($blue, 436, 32, 6, 7)
    for ($row = 0; $row -lt 3; $row++) {
        for ($star = 0; $star -lt (3 + $row); $star++) {
            $graphics.FillEllipse([Drawing.Brushes]::White, 455 - 3 * $row + 3 * $star, 33 + 2.6 * $row, 1.35, 1.35)
        }
    }
    Draw-CenteredText 'UZ' $uzFont $blue 457 80
}
$image.Save((Join-Path $output 'T_Raqam_BMW.png'), [Drawing.Imaging.ImageFormat]::Png)
@($border, $red, $green, $blue, $ink, $white, $uzFont, $regionFont, $plateFont, $graphics, $image) | ForEach-Object { $_.Dispose() }
