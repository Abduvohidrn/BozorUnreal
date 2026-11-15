@echo off
rem Unreal Engine 5.8 ni topadi va UE_ROOT ga yozadi. Boshqa joyga o'rnatilgan bo'lsa: set UE_ROOT=<papka> (Engine papkasining otasi).
if defined UE_ROOT if exist "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" exit /b 0
for %%P in ("D:\UE_5.8" "C:\Program Files\Epic Games\UE_5.8" "D:\install\UE_5.8" "E:\Unreal\UE_5.8") do (
	if exist "%%~P\Engine\Binaries\Win64\UnrealEditor.exe" (
		set "UE_ROOT=%%~P"
		exit /b 0
	)
)
echo Unreal Engine 5.8 topilmadi. UE_ROOT ni o'rnating, masalan: set UE_ROOT=D:\UE_5.8
pause
exit /b 1
