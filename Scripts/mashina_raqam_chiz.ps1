$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# 0–8 turgan mashinalar, 9–31 qatnov. 131-son qaror, 1 va 2-ilovalar.
$numbers = @(
    '01 A 482 KM', '01 K 316 TM', '10 N 427 VA', '01 M 635 XA',
    '01 S 248 AB', '30 A 516 MB', '01 B 461 RA', '01 D 258 YA', '10 F 683 LA',
    '01 H 263 NA', '01 U 524 SB', '01 H 782 KB', '01 T 315 JA',
    '10 B 471 NA', '01 L 638 XA', '01 P 419 MA', '01 V 396 CA',
    '30 C 724 DB', '01 R 836 XA', '01 V 627 LA', '01 X 742 SA',
    '40 A 638 NB', '01 E 267 KB', '01 J 592 AA', '01 F 629 UA',
    '10 Y 384 RA', '01 C 816 TA', '01 G 576 BB', '01 Y 593 KA',
    '60 B 418 DA', '01 Z 375 VA', '01 N 467 EB'
)
if (($numbers | Sort-Object -Unique).Count -ne 32 -or ($numbers -notmatch '^\d{2} [A-Z] \d{3} [A-Z]{2}$')) {
    throw 'Davlat raqami formati yoki takrorlanishi xato.'
}

$output = Join-Path (Split-Path $PSScriptRoot) 'Saved/MashinaRaqam'
New-Item -Path $output -ItemType Directory -Force | Out-Null
$image = [Drawing.Bitmap]::new(2048, 1024)
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
$image.Save((Join-Path $output 'T_Raqam_Atlas.png'), [Drawing.Imaging.ImageFormat]::Png)
@($border, $red, $green, $blue, $ink, $white, $uzFont, $regionFont, $plateFont, $graphics, $image) | ForEach-Object { $_.Dispose() }
