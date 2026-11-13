param([string]$Kirish, [string]$Chiqish, [int]$K = 6)
# Logoni ko'k fondan ajratadi: oq qism -> oq rang + alfa. Belgi (chapdagi) va matn alohida PNG.
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile($Kirish)
$W = $src.Width; $H = $src.Height
# ustunlar bo'yicha oq piksel bormi
$ustun = @()
for ($x = 0; $x -lt $W; $x++) {
  $bor = $false
  for ($y = 0; $y -lt $H; $y++) { if ($src.GetPixel($x, $y).R -gt 128) { $bor = $true; break } }
  $ustun += $bor
}
# segmentlar
$seg = @(); $bosh = -1
for ($x = 0; $x -lt $W; $x++) {
  if ($ustun[$x] -and $bosh -lt 0) { $bosh = $x }
  if (-not $ustun[$x] -and $bosh -ge 0) { $seg += ,@($bosh, ($x - 1)); $bosh = -1 }
}
if ($bosh -ge 0) { $seg += ,@($bosh, ($W - 1)) }
"segmentlar: " + (($seg | ForEach-Object { "$($_[0])-$($_[1])" }) -join ', ')
# eng katta bo'shliq bo'yicha ikkiga bo'lish (belgi | matn)
$eng = 0; $bolish = 0
for ($i = 0; $i -lt $seg.Count - 1; $i++) { $g = $seg[$i + 1][0] - $seg[$i][1]; if ($g -gt $eng) { $eng = $g; $bolish = $i } }
$belgi = @($seg[0][0], $seg[$bolish][1]); $matn = @($seg[($bolish + 1)][0], $seg[($seg.Count - 1)][1])
"belgi: $belgi  matn: $matn"

function Kesib-Sakla($x0, $x1, $nom) {
  # y chegaralari
  $y0 = $H; $y1 = 0
  for ($x = $x0; $x -le $x1; $x++) { for ($y = 0; $y -lt $H; $y++) { if ($src.GetPixel($x, $y).R -gt 128) { if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y } } } }
  $pad = 3
  $x0 = [Math]::Max(0, $x0 - $pad); $x1 = [Math]::Min($W - 1, $x1 + $pad); $y0 = [Math]::Max(0, $y0 - $pad); $y1 = [Math]::Min($H - 1, $y1 + $pad)
  $w = $x1 - $x0 + 1; $h = $y1 - $y0 + 1
  $katta = New-Object System.Drawing.Bitmap ($w * $K), ($h * $K)
  $g = [System.Drawing.Graphics]::FromImage($katta)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, ($w * $K), ($h * $K)), $x0, $y0, $w, $h, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose()
  $out = New-Object System.Drawing.Bitmap $katta.Width, $katta.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  for ($y = 0; $y -lt $katta.Height; $y++) {
    for ($x = 0; $x -lt $katta.Width; $x++) {
      $r = $katta.GetPixel($x, $y).R
      # 25..215 oralig'ini 0..1 ga - qirralar silliq, lekin tiniq
      $a = [Math]::Min(255, [Math]::Max(0, [int](($r - 25) * 255 / 190)))
      $out.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, 255, 255, 255))
    }
  }
  $out.Save((Join-Path $Chiqish $nom), [System.Drawing.Imaging.ImageFormat]::Png)
  "$nom : $($out.Width)x$($out.Height)"
  $katta.Dispose(); $out.Dispose()
}
New-Item -ItemType Directory -Force $Chiqish | Out-Null
Kesib-Sakla $belgi[0] $belgi[1] 'T_Logo_Belgi.png'
Kesib-Sakla $matn[0] $matn[1] 'T_Logo_Matn.png'
Kesib-Sakla $belgi[0] $matn[1] 'T_Logo_Toliq.png'

