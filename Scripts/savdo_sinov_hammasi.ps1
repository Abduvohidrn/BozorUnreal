# Savdo animatsiyasi: 1-3 darajalar (savdo_sinov.ps1) va SotuvTest (uzatish suratlari). Faqat ASCII.
# Natija: Saved\savdo_sinov_hammasi.txt
param([string]$UE = "D:\install\UE_5.8")
$loyiha = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$chiq = Join-Path $loyiha "Saved\savdo_sinov_hammasi.txt"
"" | Set-Content $chiq
foreach ($d in 1, 2, 3) {
	"===== $d-daraja" | Add-Content $chiq
	& powershell -NoProfile -File (Join-Path $loyiha "Scripts\savdo_sinov.ps1") -Daraja $d | Select-String "uzatishdan|mol uzatildi|YASHIL|QIZIL|Natija|xalta=|TIMEOUT" | ForEach-Object { $_.Line } | Add-Content $chiq
}
"===== SotuvTest" | Add-Content $chiq
$log = Join-Path $loyiha "Saved\Logs\SotuvTestSavdo.log"
$p = Start-Process "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -ArgumentList ("`"$loyiha\BozorUnreal.uproject`" /Game/Bozor/Maps/Bozor -game -windowed " +
	"-ResX=1280 -ResY=720 -SotuvTest -Daraja=2 -AvtobusYoq -Unattended -log=SotuvTestSavdo.log")   # 1-darajada pul 2 kiloga yetmaydi
if (-not $p.WaitForExit(300000)) { $p.Kill(); "TIMEOUT" | Add-Content $chiq }
Select-String -Path $log -Pattern "SavdoAnim\] javob=|V_\w+ -> |MolBer\]|variant yo|qo'ldagi mol" | ForEach-Object {
	($_.Line -replace "^\[[^\]]*\]\[[^\]]*\]", "" -replace "Log\w+: (Display: )?(\[SavdoAnim\] )?", "") } | Add-Content $chiq
"TAYYOR" | Add-Content $chiq
