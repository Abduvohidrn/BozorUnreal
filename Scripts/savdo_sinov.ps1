# Savdo animatsiyasi sinovi: bot 1-darajani o'ynaydi (bitta non), keyin logdan sotuvchi harakatlari va mol uzatish ([MolBer]).
# Mol uzatilmasa yoki sotuvchi harakati klipsiz qolsa - QIZIL (chiqish kodi 1). Faqat ASCII (PowerShell 5.1).
# Ishga tushirish: powershell -File Scripts\savdo_sinov.ps1 [-Daraja 1]
# UE - loyiha EngineAssociation'idagi dvigatel (reestr: D:/install/UE_5.8); boshqasi bilan build PCH to'qnashuvi beradi
param([int]$Daraja = 1, [string]$UE = "D:\install\UE_5.8")
$loyiha = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$log = Join-Path $loyiha "Saved\Logs\SavdoSinov.log"
Remove-Item $log -ErrorAction SilentlyContinue
$p = Start-Process "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -ArgumentList ("`"$loyiha\BozorUnreal.uproject`" /Game/Bozor/Maps/Bozor -game -windowed " +
	"-ResX=1280 -ResY=720 -DarajaTest -Daraja=$Daraja -AvtobusYoq -Unattended -log=SavdoSinov.log")
if (-not $p.WaitForExit(300000)) { $p.Kill(); "TIMEOUT"; exit 2 }
$q = Select-String -Path $log -Pattern "SavdoAnim|\[MolBer\]|Natija:" | ForEach-Object { $_.Line -replace "^\[[^\]]*\]\[[^\]]*\]", "" }
$q
$mol = ($q | Where-Object { $_ -match "\[MolBer\]" }).Count
$klipsiz = ($q | Where-Object { $_ -match "klip yo'q|o'ynadi 0" }).Count
# uzatish klipi faqat mol berilganda (tortishda sotuvchi molni "uzatmaydi")
$uzatish = ($q | Where-Object { $_ -match "-> AnimationSequence_Cashier_Handout" }).Count
$tortish = ($q | Where-Object { $_ -match " V_Weigh -> " }).Count
$tortdim = ($q | Where-Object { $_ -match "javob=(Tortdim|TaroziUzr)" }).Count
# ta'zim: kelganda va salomga alik (2 ta), xayrlashuvda (1 ta); ketma-ket kelgani bir-birini kesadi
$tazim = ($q | Where-Object { $_ -match " V_Greet -> " }).Count
$rahmat = ($q | Where-Object { $_ -match " V_Thanks -> " }).Count
# uzatish (V_Handout) boshlangach mol sotuvchi qo'lidan chiqquncha (0.55 x davom + 0.3 s) boshqa harakat qo'lni tortib olmasin
$xato = @()
$qatorlar = Select-String -Path $log -Pattern "SavdoAnim\] \S+ V_\w+ -> " | ForEach-Object { $_.Line }
for ($i = 0; $i -lt $qatorlar.Count; $i++) {
	if ($qatorlar[$i] -notmatch " V_Handout -> .*davom ([\d.]+)") { continue }
	$uch = 0.55 * [double]$Matches[1] + 0.3
	$t0 = [datetime]::ParseExact(($qatorlar[$i] -replace "^\[([^\]]+)\].*", '$1'), "yyyy.MM.dd-HH.mm.ss:fff", $null)
	if ($i + 1 -lt $qatorlar.Count) {
		$t1 = [datetime]::ParseExact(($qatorlar[$i + 1] -replace "^\[([^\]]+)\].*", '$1'), "yyyy.MM.dd-HH.mm.ss:fff", $null)
		$fark = ($t1 - $t0).TotalSeconds
		"--- uzatishdan keyingi harakat: {0:N2} s keyin (mol qo'ldan {1:N2} s da chiqadi)" -f $fark, $uch
		# 0.05 s - kadr va log vaqtidagi farq (harakat aynan mol qo'ldan chiqqan kadrda boshlanadi)
		if ($fark -lt $uch - 0.05) { $xato += ("uzatish {0:N2} s da kesildi" -f $fark) }
	}
}
# qo'lga olingan mol suhbat paneli ostida qolmasin (panelning o'ng chetidan o'ngda)
foreach ($s in ($q | Where-Object { $_ -match "qo'ldagi mol " })) {
	if ($s -notmatch "(\+[\d.]+ s).*x=([\d.]+) y=([\d.]+) tepasi y=([\d.-]+), panel x=([\d.]+)\.\.([\d.]+)") { continue }
	$p = @($Matches[1], [double]$Matches[2], [double]$Matches[3], [double]$Matches[4], [double]$Matches[5], [double]$Matches[6])
	"--- qo'ldagi mol {0}: x={1} y={2} tepasi y={3}, panel {4}..{5}" -f $p
	if ($p[1] -gt $p[4] -and $p[1] -lt $p[5]) { $xato += "mol panel ostida (x=$($p[1]))" }
	# xalta kaftdan osiladi - markazi pastda bo'lishi tabiiy; tepasi (dastasi) ko'rinsin, markazi ekrandan chiqmasin
	if ($p[3] -gt 0.8 -or $p[2] -gt 1.0) { $xato += "mol ko'rinmaydi (y=$($p[2]), tepasi $($p[3]))" }
}
# chegaralar bitta suhbat uchun - darajada bir nechta sotuvchi bilan gaplashiladi
$suhbat = [Math]::Max(1, ($q | Where-Object { $_ -match "javob=KutibOlish" }).Count)
"--- suhbatlar: $suhbat, mol uzatildi: $mol, klipsiz: $klipsiz, uzatish klipi: $uzatish, tortish: $tortish/$tortdim, ta'zim: $tazim, rahmat: $rahmat"
$tazim = $tazim / $suhbat
$rahmat = $rahmat / $suhbat
if ($mol -eq 0) { $xato += "mol uzatilmadi" }
if ($klipsiz -gt 0) { $xato += "klipsiz harakat" }
if ($uzatish -ne $mol) { $xato += "uzatish klipi $uzatish marta (mol $mol marta berildi)" }
if ($tortish -lt $tortdim) { $xato += "tortishda V_Weigh yo'q" }
if ($tazim -gt 2) { $xato += "ta'zim $tazim marta" }
if ($rahmat -gt 1) { $xato += "rahmat ta'zimi $rahmat marta" }
if ($xato.Count) { "QIZIL: " + ($xato -join "; "); exit 1 } else { "YASHIL"; exit 0 }
