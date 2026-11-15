# Yozilgan gap bo'laklarini o'yin STT'si (CrispASR + FastConformer uz) bilan matnga qaytaradi va asl matn bilan solishtiradi.
# Ishga tushirish: powershell -File AI\tts\tekshir.ps1 [-Soni 40] [-Naqsh "G_*"]
# (fayl faqat ASCII: Windows PowerShell 5.1 BOM'siz UTF-8 ni ANSI deb o'qiydi)
param([int]$Soni = 40, [string]$Naqsh = "*")
$bu = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $bu "..\stt\crispasr.exe"
$model = Join-Path $bu "..\stt\stt-uz.gguf"
$tmp = Join-Path $env:TEMP "gaplar_stt"
New-Item -ItemType Directory -Force $tmp | Out-Null
$matn = @{}
Get-Content (Join-Path $bu "gaplar\royxat.tsv") -Encoding utf8 | ForEach-Object { $q = $_ -split "`t"; $matn[$q[0]] = $q[1] }
$tutuq = "[" + [char]0x02BB + [char]0x02BC + [char]0x2018 + [char]0x2019 + "``]"; $chiziq = [string][char]0x2013 + [char]0x2014
function Toza($s) { (($s.ToLower() -replace $tutuq, "'") -replace "[^\p{L}\p{N}' ]", " " -replace "\s+", " ").Trim() }
$fayllar = Get-ChildItem (Join-Path $bu "gaplar") -Filter "$Naqsh.wav" | Get-Random -Count $Soni -SetSeed 7
$mos = 0
foreach ($f in $fayllar) {
	$o = Join-Path $tmp $f.BaseName
	& $exe --backend fastconformer-ctc -m $model -f $f.FullName -t 4 -nt -ng -np --lid-backend off --no-punctuation -otxt -of $o 2>$null | Out-Null
	$eshitildi = (Get-Content "$o.txt" -Encoding utf8 -ErrorAction SilentlyContinue) -join " "
	$kutilgan = $matn[$f.BaseName] -replace "^[\s.,:;!?\-" + $chiziq + "]+", ""
	$ok = (Toza $eshitildi) -eq (Toza $kutilgan)
	if ($ok) { $mos++ }
	"{0} {1,-34} {2,5:N2}s  kutilgan: {3}  |  eshitildi: {4}" -f $(if ($ok) { "OK" } else { "--" }), $f.BaseName, (($f.Length - 44) / 48000), $kutilgan, $eshitildi
}
"aynan mos: $mos / $($fayllar.Count)"
