@echo off
rem Bozor o'yinini Unreal'da (muharrirsiz) ishga tushiradi: Third Person qahramoni, bozor xaritasi.
rem Kirish logolari -> bosh menyu (sozlamalar: sichqoncha sezgirligi, tugmalar). O'yinda Esc - pauza menyusi.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0BozorUnreal.uproject" -game -windowed -ResX=1600 -ResY=900 -log
