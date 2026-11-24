# Bekat ustunidagi belgi: ko'k plita, oq hoshiya, o'rtada oq maydon va qora avtobus (5.16 "Avtobus to'xtash joyi" kabi).
# Chiqish: Saved/Yozuvlar/out/T_Bekat_Belgi.png (800x1000 - plita 60x75 sm nisbatida).
# Keyin: Scripts/bekat_belgi_yangila.py (Unreal'ga import, MI_Bekat_Belgi).
# Ishga tushirish: powershell -ExecutionPolicy Bypass -File Scripts\bekat_belgi_chiz.ps1
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot
$Out = Join-Path $Root 'Saved\Yozuvlar\out'
New-Item -ItemType Directory -Force $Out | Out-Null

function Rang([string]$h) { [System.Drawing.ColorTranslator]::FromHtml($h) }
function Yumaloq([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc($x, $y, $d, $d, 180, 90); $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90); $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure(); $p
}
function Toldir($g, $rang, $path) { $b = New-Object System.Drawing.SolidBrush($rang); $g.FillPath($b, $path); $b.Dispose(); $path.Dispose() }
function Doira($g, $rang, [float]$cx, [float]$cy, [float]$r) { $b = New-Object System.Drawing.SolidBrush($rang); $g.FillEllipse($b, $cx - $r, $cy - $r, 2 * $r, 2 * $r); $b.Dispose() }

$W = 800; $H = 1000
$bmp = New-Object System.Drawing.Bitmap($W, $H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$Kok = Rang '#1E73C8'; $Oq = Rang '#F4F4F2'; $Qora = Rang '#151515'

$g.Clear($Oq)                                                       # chetidagi ingichka oq hoshiya
Toldir $g $Kok (Yumaloq 14 14 ($W - 28) ($H - 28) 36)               # ko'k plita
# o'rtadagi oq maydon (rasmdagidek: yonlarda ingichka, pastda kengroq ko'k)
$Mx = 70; $My = 130; $Mw = $W - 2 * $Mx; $Mh = 640
Toldir $g $Oq (Yumaloq $Mx $My $Mw $Mh 28)

# avtobus (yon ko'rinish, o'ngga qaragan): qora tana, 4 ta oq oyna, g'ildiraklar oq halqa bilan ajratilgan
$cy = $My + $Mh / 2
$Bx = 140; $Bw = $W - 2 * $Bx; $Bt = $cy - 120; $Bh = 215
Toldir $g $Qora (Yumaloq $Bx $Bt $Bw $Bh 46)
$Ox = $Bx + 34; $Ot = $Bt + 26; $Oh = 84; $Gap = 16
$Ow = ($Bw - 68 - 3 * $Gap) / 4
for ($i = 0; $i -lt 4; $i++) {
    $r = if ($i -eq 3) { 20 } else { 8 }                            # old oyna burchagi yumaloqroq
    Toldir $g $Oq (Yumaloq ($Ox + $i * ($Ow + $Gap)) $Ot $Ow $Oh $r)
}
$Gy = $Bt + $Bh - 6
foreach ($gx in @(($Bx + 125), ($Bx + $Bw - 125))) {
    Doira $g $Oq $gx $Gy 64
    Doira $g $Qora $gx $Gy 50
}

$g.Dispose()
$f = Join-Path $Out 'T_Bekat_Belgi.png'
$bmp.Save($f, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
"saqlandi: $f"
