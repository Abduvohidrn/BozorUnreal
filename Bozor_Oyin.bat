@echo off
rem Bozor o'yinini Unreal'da (muharrirsiz) ishga tushiradi: Third Person qahramoni, bozor xaritasi.
rem Boshqaruv: WASD - yurish, sichqoncha - qarash, Probel - sakrash, Esc yoki Alt+F4 - chiqish.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0BozorUnreal.uproject" /Game/Bozor/Maps/Bozor -game -windowed -ResX=1600 -ResY=900 -log
