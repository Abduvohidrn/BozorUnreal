@echo off
rem Bozor loyihasini Unreal Editor'da ochadi (bozor xaritasi o'zi yuklanadi; o'ynash uchun Play tugmasi yoki Alt+P).
rem Dvigatel: UE_ROOT o'zgaruvchisi, bo'lmasa ma'lum o'rnatish joylari (birinchi topilgani).
call "%~dp0Scripts\ue_top.bat" || exit /b 1
start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0BozorUnreal.uproject"
