# Megascans mol teksturalarini 4K -> 1K PNG qiladi (BaseColor, Normal, AO): E:\TRK\Megascans_manba\<zip>\ -> E:\TRK\Megascans_manba\_1k\
# Keyin: Scripts/mol_megascans_import.py. Faqat ASCII (Windows PowerShell 5.1 BOM'siz UTF-8 ni ANSI deb o'qiydi).
param([string]$Manba = "E:\TRK\Megascans_manba", [int]$Olcham = 1024)
Add-Type -AssemblyName System.Drawing
$chiq = Join-Path $Manba "_1k"
New-Item -ItemType Directory -Force $chiq | Out-Null
foreach ($d in Get-ChildItem $Manba -Directory | Where-Object Name -ne "_1k") {
	foreach ($tur in "BaseColor", "Normal", "AO") {
		$f = Get-ChildItem $d.FullName -Filter "*_4K_$tur.jpg" | Select-Object -First 1
		if (-not $f) { "yo'q: $($d.Name) $tur"; continue }
		$o = Join-Path $chiq ("{0}_{1}.png" -f ($d.Name -replace "_high$", ""), $tur)
		if (Test-Path $o) { continue }
		$img = [Drawing.Image]::FromFile($f.FullName)
		$bmp = New-Object Drawing.Bitmap $Olcham, $Olcham
		$g = [Drawing.Graphics]::FromImage($bmp)
		$g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
		$g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
		$g.DrawImage($img, 0, 0, $Olcham, $Olcham)
		$bmp.Save($o, [Drawing.Imaging.ImageFormat]::Png)
		$g.Dispose(); $bmp.Dispose(); $img.Dispose()
	}
}
"tayyor: " + (Get-ChildItem $chiq -Filter *.png).Count + " ta PNG, " + [math]::Round(((Get-ChildItem $chiq -Filter *.png | Measure-Object Length -Sum).Sum / 1MB), 0) + " MB"

