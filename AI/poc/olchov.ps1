# 0-bosqich: dasturni "kuchsiz kompyuter" sharoitida ishga tushirib vaqtini o'lchaydi.
# Ultra 9 285K: mantiqiy 0-7 = P-yadrolar, 8-23 = E-yadrolar. Standart niqob 0xF00 = 4 ta E-yadro (kuchsiz CPU taqlidi).
# Ishlatish: . .\olchov.ps1 ; Olcha -Exe "..." -Argumentlar "..." [-Niqob 0xF00]
function Olcha {
    param([string]$Exe, [string]$Argumentlar, [long]$Niqob = 0xF00, [string]$Chiqish = "")
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = $Argumentlar
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = (Get-Location).Path
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $p = [System.Diagnostics.Process]::Start($psi)
    try { $p.ProcessorAffinity = [IntPtr]$Niqob } catch {}
    $outTask = $p.StandardOutput.ReadToEndAsync()
    $errTask = $p.StandardError.ReadToEndAsync()
    $p.WaitForExit()
    $sw.Stop()
    [pscustomobject]@{
        Ms     = $sw.ElapsedMilliseconds
        Kod    = $p.ExitCode
        Chiqdi = $outTask.Result
        Xato   = $errTask.Result
    }
}
