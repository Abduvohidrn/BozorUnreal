# Menyu uchun jonli fon: zebra boshidan bola ko'zi bilan 5 daqiqa yozib, aylanma MP4 qiladi.
# 1) o'yin -FonVideo bilan har kadrni PNG qiladi (qat'iy 30 fps qadam)  2) ffmpeg: oxirgi 2 s boshiga qorishadi (choki ko'rinmaydi)
# Natija: Content/Movies/MenyuFon.mp4. Ishga tushirish: powershell -File Scripts\ui_menyu_video.ps1 [-Soniya 300]
param([int]$Soniya = 300, [int]$Qorishma = 2, [switch]$FaqatYigish)
$ErrorActionPreference = 'Stop'
$p = Split-Path $PSScriptRoot -Parent
$kadrlar = "$p\Saved\Screenshots\MenyuVideo"
$fps = 30
$jami = ($Soniya + $Qorishma) * $fps
if (-not $FaqatYigish) {
  if (Test-Path $kadrlar) { Remove-Item $kadrlar -Recurse -Force }
  $a = @("`"$p\BozorUnreal.uproject`"", "/Game/Bozor/Maps/Bozor", "-game", "-AvtobusYoq", "-MenyuFonOl",
    "-FonKamera=-1400,0,122,5,0", "-FonFokus=18", "-FonDiafragma=16", "-FonKutish=15", "-FonVideo=$jami",
    "-benchmark", "-fps=$fps", "-windowed", "-ResX=1920", "-ResY=1080", "-nosound", "-log=MenyuVideo.log")
  $pr = Start-Process "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $a -PassThru
  $pr.WaitForExit()
  Start-Sleep 3
}
$soni = (Get-ChildItem $kadrlar -Filter k_*.png).Count
"kadrlar: $soni / $jami"
if ($soni -lt $jami) { throw "kadrlar yetarli emas" }
$ff = (Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter ffmpeg.exe -Recurse | Select-Object -First 1).FullName
New-Item -ItemType Directory -Force "$p\Content\Movies" | Out-Null
$q = $Qorishma * $fps
$offset = $Soniya - $Qorishma
& $ff -hide_banner -loglevel warning -y -framerate $fps -i "$kadrlar\k_%05d.png" -filter_complex `
  "[0:v]split[a][b];[a]trim=start_frame=$q,setpts=PTS-STARTPTS[m];[b]trim=end_frame=$q,setpts=PTS-STARTPTS[h];[m][h]xfade=transition=fade:duration=$($Qorishma):offset=$offset,format=yuv420p[v]" `
  -map "[v]" -c:v libx264 -preset slow -crf 21 -r $fps -movflags +faststart "$p\Content\Movies\MenyuFon.mp4"
if ($LASTEXITCODE -ne 0) { throw "ffmpeg xato" }
"mp4: $([int]((Get-Item "$p\Content\Movies\MenyuFon.mp4").Length / 1MB)) MB"
Remove-Item $kadrlar -Recurse -Force
"tayyor"
