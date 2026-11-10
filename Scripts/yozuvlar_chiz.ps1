# Bekat soyabonidagi Avstraliya reklamalari va avtobusdagi "Zero emission" o'rniga o'zbekcha plakat/yozuvlar chizadi.
# Kirish: Saved/Yozuvlar/T_bus_shelter02_albedo.png (Scripts/probe_yozuvlar.py chiqaradi).
# Chiqish: Saved/Yozuvlar/out/T_Bekat_Soyabon_UZ.png (4096 atlas), T_Avtobus_Yozuv.png (2048x1024, shaffof fon).
# Keyin: Scripts/yozuv_yangila.py (Unreal'ga import qiladi va qo'llaydi).
# Ishga tushirish: powershell -ExecutionPolicy Bypass -File Scripts\yozuvlar_chiz.ps1
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot
$Dir = Join-Path $Root 'Saved\Yozuvlar'
$Out = Join-Path $Dir 'out'
New-Item -ItemType Directory -Force $Out | Out-Null

function Rang([string]$h, [int]$a = 255) { $c = [System.Drawing.ColorTranslator]::FromHtml($h); [System.Drawing.Color]::FromArgb($a, $c) }
function Shrift([float]$px, [string]$nom = 'Segoe UI', [System.Drawing.FontStyle]$st = 'Bold') { New-Object System.Drawing.Font($nom, $px, $st, [System.Drawing.GraphicsUnit]::Pixel) }
function Mo-yqal($g) { $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'; $g.InterpolationMode = 'HighQualityBicubic' }
# matn markazda, berilgan to'rtburchak ichida (kerak bo'lsa qatorga bo'linadi)
function Yoz($g, [string]$t, $font, $rang, [float]$x, [float]$y, [float]$w, [float]$h, [string]$tekis = 'Center') {
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = $tekis; $sf.LineAlignment = 'Center'
    $b = New-Object System.Drawing.SolidBrush($rang)
    $g.DrawString($t, $font, $b, (New-Object System.Drawing.RectangleF($x, $y, $w, $h)), $sf)
    $b.Dispose(); $sf.Dispose()
}
function Tort($g, $rang, [float]$x, [float]$y, [float]$w, [float]$h) { $b = New-Object System.Drawing.SolidBrush($rang); $g.FillRectangle($b, $x, $y, $w, $h); $b.Dispose() }
function Doira($g, $rang, [float]$cx, [float]$cy, [float]$r) { $b = New-Object System.Drawing.SolidBrush($rang); $g.FillEllipse($b, $cx - $r, $cy - $r, 2 * $r, 2 * $r); $b.Dispose() }
function Yangi([int]$w, [int]$h) { $b = New-Object System.Drawing.Bitmap($w, $h); $g = [System.Drawing.Graphics]::FromImage($b); Mo-yqal $g; @($b, $g) }

# ------------------------------------------------------------ plakatlar (har biri tik turgan holda chiziladi)
function Plakat-Non([int]$W, [int]$H) {
    $b, $g = Yangi $W $H
    $br = New-Object System.Drawing.Drawing2D.LinearGradientBrush((New-Object System.Drawing.Point 0, 0), (New-Object System.Drawing.Point 0, $H), (Rang '#f6a43a'), (Rang '#c8571c'))
    $g.FillRectangle($br, 0, 0, $W, $H); $br.Dispose()
    # tandir non: doira, chekka halqa va markazdagi naqsh nuqtalari
    $cx = $W / 2; $cy = $H * 0.52; $r = $W * 0.34
    Doira $g (Rang '#8a4a17') $cx ($cy + 8) $r
    Doira $g (Rang '#d98f3e') $cx $cy $r
    Doira $g (Rang '#e9b46a') $cx $cy ($r * 0.62)
    for ($i = 0; $i -lt 16; $i++) { $a = $i * [Math]::PI / 8; Doira $g (Rang '#9c5a22') ($cx + [Math]::Cos($a) * $r * 0.36) ($cy + [Math]::Sin($a) * $r * 0.36) ($W * 0.012) }
    Doira $g (Rang '#9c5a22') $cx $cy ($W * 0.015)
    Yoz $g 'ISSIQ NON' (Shrift ($W * 0.15)) ([System.Drawing.Color]::White) 0 ($H * 0.04) $W ($H * 0.17)
    Yoz $g "har kuni ertalab tandirdan" (Shrift ($W * 0.06) 'Segoe UI' 'Regular') (Rang '#fff4dd') 0 ($H * 0.19) $W ($H * 0.08)
    Tort $g (Rang '#1d3f8f') 0 ($H * 0.86) $W ($H * 0.14)
    Yoz $g 'Dehqon bozori · NON rastasi' (Shrift ($W * 0.055)) ([System.Drawing.Color]::White) 0 ($H * 0.86) $W ($H * 0.14)
    $g.Dispose(); $b
}

function Plakat-Hosil([int]$W, [int]$H) {
    $b, $g = Yangi $W $H
    Tort $g (Rang '#2f7d32') 0 0 $W $H
    Tort $g (Rang '#f3efe2') ($W * 0.06) ($H * 0.3) ($W * 0.88) ($H * 0.5)
    # pomidor, bodring, sabzi - sodda shakllar
    Doira $g (Rang '#d93a2b') ($W * 0.3) ($H * 0.55) ($W * 0.13)
    Doira $g (Rang '#e2533f') ($W * 0.5) ($H * 0.6) ($W * 0.11)
    Doira $g (Rang '#3c8c2e') ($W * 0.3) ($H * 0.43) ($W * 0.03)
    $p = New-Object System.Drawing.Pen((Rang '#4f9a2c'), ($W * 0.07)); $p.StartCap = 'Round'; $p.EndCap = 'Round'
    $g.DrawLine($p, ($W * 0.62), ($H * 0.42), ($W * 0.8), ($H * 0.66)); $p.Dispose()
    $p = New-Object System.Drawing.Pen((Rang '#ef8a1f'), ($W * 0.05)); $p.EndCap = 'Triangle'
    $g.DrawLine($p, ($W * 0.72), ($H * 0.4), ($W * 0.62), ($H * 0.7)); $p.Dispose()
    Yoz $g 'YANGI HOSIL' (Shrift ($W * 0.14)) ([System.Drawing.Color]::White) 0 ($H * 0.04) $W ($H * 0.14)
    Yoz $g "to'g'ridan-to'g'ri dalasidan" (Shrift ($W * 0.058) 'Segoe UI' 'Regular') (Rang '#e6f4d9') 0 ($H * 0.17) $W ($H * 0.09)
    Yoz $g 'MEVA-SABZAVOT rastasi' (Shrift ($W * 0.068)) (Rang '#ffe28a') 0 ($H * 0.83) $W ($H * 0.12)
    $g.Dispose(); $b
}

function Plakat-Bozor([int]$W, [int]$H) {
    $b, $g = Yangi $W $H
    Tort $g (Rang '#1d3f8f') 0 0 $W $H
    # kafel naqshi (bozor darvozasidagi kabi oq-ko'k)
    for ($y = 0; $y -lt $H * 0.12; $y += $W / 16) { for ($x = 0; $x -lt $W; $x += $W / 16) { Doira $g (Rang '#ffffff' 70) ($x + $W / 32) ($y + $W / 32) ($W / 90) } }
    for ($y = $H * 0.88; $y -lt $H; $y += $W / 16) { for ($x = 0; $x -lt $W; $x += $W / 16) { Doira $g (Rang '#ffffff' 70) ($x + $W / 32) ($y + $W / 32) ($W / 90) } }
    Yoz $g 'DEHQON' (Shrift ($W * 0.17)) ([System.Drawing.Color]::White) 0 ($H * 0.13) $W ($H * 0.13)
    Yoz $g 'BOZORI' (Shrift ($W * 0.17)) ([System.Drawing.Color]::White) 0 ($H * 0.25) $W ($H * 0.13)
    # meva savati
    Doira $g (Rang '#e8b23a') ($W * 0.5) ($H * 0.56) ($W * 0.2)
    Doira $g (Rang '#d93a2b') ($W * 0.4) ($H * 0.5) ($W * 0.09)
    Doira $g (Rang '#7cb342') ($W * 0.58) ($H * 0.48) ($W * 0.085)
    Doira $g (Rang '#ef8a1f') ($W * 0.5) ($H * 0.58) ($W * 0.09)
    Tort $g (Rang '#8d5a2b') ($W * 0.27) ($H * 0.58) ($W * 0.46) ($H * 0.1)
    Yoz $g 'Xush kelibsiz!' (Shrift ($W * 0.085)) (Rang '#ffe28a') 0 ($H * 0.7) $W ($H * 0.08)
    Yoz $g 'har kuni 07:00 – 19:00' (Shrift ($W * 0.06) 'Segoe UI' 'Regular') ([System.Drawing.Color]::White) 0 ($H * 0.78) $W ($H * 0.07)
    $g.Dispose(); $b
}

function Plakat-Yol([int]$W, [int]$H) {
    $b, $g = Yangi $W $H
    Tort $g (Rang '#ffd31a') 0 0 $W $H
    Yoz $g "YO'LDAN O'TISHDA" (Shrift ($W * 0.088)) (Rang '#1a1a1a') 0 ($H * 0.04) $W ($H * 0.09)
    Yoz $g "EHTIYOT BO'LING!" (Shrift ($W * 0.098)) (Rang '#c62828') 0 ($H * 0.12) $W ($H * 0.09)
    # bola va zebra
    Tort $g (Rang '#3a3a3a') 0 ($H * 0.5) $W ($H * 0.26)
    for ($i = 0; $i -lt 6; $i++) { Tort $g ([System.Drawing.Color]::White) ($W * (0.08 + $i * 0.15)) ($H * 0.52) ($W * 0.08) ($H * 0.22) }
    $cx = $W * 0.5
    Doira $g (Rang '#1d3f8f') $cx ($H * 0.3) ($W * 0.055)
    $p = New-Object System.Drawing.Pen((Rang '#1d3f8f'), ($W * 0.045)); $p.StartCap = 'Round'; $p.EndCap = 'Round'
    $g.DrawLine($p, $cx, ($H * 0.34), $cx, ($H * 0.45))
    $g.DrawLine($p, $cx, ($H * 0.45), ($cx - $W * 0.06), ($H * 0.53))
    $g.DrawLine($p, $cx, ($H * 0.45), ($cx + $W * 0.06), ($H * 0.53))
    $g.DrawLine($p, $cx, ($H * 0.37), ($cx - $W * 0.08), ($H * 0.42))
    $g.DrawLine($p, $cx, ($H * 0.37), ($cx + $W * 0.08), ($H * 0.42))
    $p.Dispose()
    Yoz $g "1. Avval chapga, keyin o'ngga qarang" (Shrift ($W * 0.044) 'Segoe UI' 'Regular') (Rang '#1a1a1a') ($W * 0.05) ($H * 0.78) ($W * 0.92) ($H * 0.06) 'Near'
    Yoz $g "2. Faqat piyodalar o'tish joyidan" (Shrift ($W * 0.044) 'Segoe UI' 'Regular') (Rang '#1a1a1a') ($W * 0.05) ($H * 0.84) ($W * 0.92) ($H * 0.06) 'Near'
    Yoz $g "3. Avtobus to'xtaguncha kuting" (Shrift ($W * 0.044) 'Segoe UI' 'Regular') (Rang '#1a1a1a') ($W * 0.05) ($H * 0.9) ($W * 0.92) ($H * 0.06) 'Near'
    $g.Dispose(); $b
}

function Plakat-Jadval([int]$W, [int]$H) {
    $b, $g = Yangi $W $H
    Tort $g (Rang '#f5f5f2') 0 0 $W $H
    Tort $g (Rang '#1d3f8f') 0 0 $W ($H * 0.12)
    Yoz $g "AVTOBUS`nJADVALI" (Shrift ($W * 0.15)) ([System.Drawing.Color]::White) 0 0 $W ($H * 0.12)
    $qatorlar = @(@('4', 'Chorsu'), @('31', 'Yunusobod'), @('67', 'Sergeli'))
    for ($i = 0; $i -lt 3; $i++) {
        $y = $H * (0.16 + $i * 0.25)
        Tort $g (Rang '#1d3f8f') ($W * 0.08) $y ($W * 0.34) ($W * 0.3)
        Yoz $g $qatorlar[$i][0] (Shrift ($W * 0.18)) ([System.Drawing.Color]::White) ($W * 0.08) $y ($W * 0.34) ($W * 0.3)
        Yoz $g $qatorlar[$i][1] (Shrift ($W * 0.12)) (Rang '#1a1a1a') 0 ($y + $W * 0.32) $W ($W * 0.18)
        Yoz $g "06:00 – 23:00`nhar 10 daqiqada" (Shrift ($W * 0.085) 'Segoe UI' 'Regular') (Rang '#444444') 0 ($y + $W * 0.5) $W ($W * 0.32)
    }
    $g.Dispose(); $b
}

function Plakat-Belgi([int]$W, [int]$H) {
    $b, $g = Yangi $W $H
    Tort $g (Rang '#1d3f8f') 0 0 $W $H
    $g.TranslateTransform($W / 2, $H / 2); $g.RotateTransform(-90)
    Yoz $g 'AVTOBUS' (Shrift ($W * 0.55)) ([System.Drawing.Color]::White) (-$H / 2) (-$W / 2) $H $W
    $g.ResetTransform(); $g.Dispose(); $b
}

# ------------------------------------------------------------ atlasga joylash
$atlas = [System.Drawing.Bitmap]::FromFile((Join-Path $Dir 'T_bus_shelter02_albedo.png'))
$ga = [System.Drawing.Graphics]::FromImage($atlas); Mo-yqal $ga
# x, y, kenglik, balandlik (atlas pikselida), plakat, burish (atlasda plakat tepasi qayerga qaragan)
$joylar = @(
    @(0, 1376, 462, 598, 'Non', 'None'),            # Red Rooster
    @(3212, 1908, 464, 568, 'Hosil', 'None'),       # Vegemite
    @(2482, 2604, 716, 512, 'Bozor', 'Rotate270FlipNone'),   # o'yindagi soyabon: atlasda 90° burilgan (tepasi chapda)
    @(2642, 3248, 484, 702, 'Yol', 'RotateNoneFlipX'),   # Woolworths: soyabonning ikkinchi yuzi - modelda akslangan
    @(198, 598, 196, 784, 'Jadval', 'None'),        # CityGlider jadvali
    @(3866, 2526, 104, 524, 'Belgi', 'None')        # TransLink
)
foreach ($j in $joylar) {
    $w = $j[2]; $h = $j[3]
    $burilgan = $j[5] -match 'Rotate(90|270)'   # 90/270 da plakat eni va bo'yi almashadi
    $pw = if ($burilgan) { $h } else { $w }
    $ph = if ($burilgan) { $w } else { $h }
    $p = & "Plakat-$($j[4])" $pw $ph
    if ($j[5] -ne 'None') { $p.RotateFlip([System.Drawing.RotateFlipType]$j[5]) }
    $ga.DrawImage($p, $j[0], $j[1], $w, $h)
    $p.Dispose()
}
$ga.Dispose()
$atlas.Save((Join-Path $Out 'T_Bekat_Soyabon_UZ.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$atlas.Dispose()

# ------------------------------------------------------------ Toshkent raqami (T_Reg_Plate o'rniga; butun tekstura - plita yuzi)
# Yuridik shaxs raqami: "01 | 465 ABA | bayroq UZ"
$b, $g = Yangi 1024 256
$g.Clear((Rang '#9a9a9a'))
$yo = New-Object System.Drawing.Drawing2D.GraphicsPath
$r = 40
$yo.AddArc(6, 6, $r, $r, 180, 90); $yo.AddArc(1018 - $r, 6, $r, $r, 270, 90); $yo.AddArc(1018 - $r, 250 - $r, $r, $r, 0, 90); $yo.AddArc(6, 250 - $r, $r, $r, 90, 90); $yo.CloseFigure()
$oq = New-Object System.Drawing.SolidBrush((Rang '#f2f2ef')); $g.FillPath($oq, $yo); $oq.Dispose()
$qalam = New-Object System.Drawing.Pen((Rang '#111111'), 12); $g.DrawPath($qalam, $yo)
$g.DrawLine($qalam, 222, 14, 222, 242); $qalam.Dispose()
$raqam = Shrift 148 'Bahnschrift' 'Bold'
# Bahnschrift raqamlari qatorning yuqori qismida turadi - to'rtburchak pastroqqa surilgan
Yoz $g '01' $raqam (Rang '#111111') 14 34 208 240
Yoz $g '465 ABA' $raqam (Rang '#111111') 232 34 640 240
# bayroq: ko'k / oq / yashil, oq yo'l chegaralarida ingichka qizil chiziqlar
$fx = 878; $fy = 40; $fw = 116; $fh = 76
Tort $g (Rang '#1eb5e5') $fx $fy $fw ($fh / 3)
Tort $g (Rang '#ffffff') $fx ($fy + $fh / 3) $fw ($fh / 3)
Tort $g (Rang '#1eb53a') $fx ($fy + 2 * $fh / 3) $fw ($fh / 3)
Tort $g (Rang '#ce1126') $fx ($fy + $fh / 3) $fw 3
Tort $g (Rang '#ce1126') $fx ($fy + 2 * $fh / 3 - 3) $fw 3
Doira $g ([System.Drawing.Color]::White) ($fx + 22) ($fy + 12) 9
Doira $g (Rang '#1eb5e5') ($fx + 26) ($fy + 11) 8
Yoz $g 'UZ' (Shrift 70 'Bahnschrift' 'Bold') (Rang '#111111') $fx ($fy + $fh + 4) $fw 90
$g.Dispose()
$b.Save((Join-Path $Out 'T_Raqam_UZ.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$b.Dispose()

# ------------------------------------------------------------ avtobus yon yozuvi ("Zero emission" o'rniga, xuddi shu joylashuvda)
$b, $g = Yangi 2048 1024
$g.Clear([System.Drawing.Color]::Transparent)
Yoz $g 'Toshkent' (Shrift 300 'Bahnschrift' 'Bold') (Rang '#7fb07f') 60 40 1900 420 'Near'
Yoz $g 'shahar avtobusi' (Shrift 190 'Bahnschrift' 'Regular') (Rang '#d0d0d0') 380 360 1660 300 'Near'
$g.Dispose()
$b.Save((Join-Path $Out 'T_Avtobus_Yozuv.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$b.Dispose()
'tayyor: ' + $Out
