@echo off
rem Bozor o'yinini Unreal'da (muharrirsiz) ishga tushiradi: Third Person qahramoni, bozor xaritasi.
rem Kirish logolari -> bosh menyu (sozlamalar: sichqoncha sezgirligi, tugmalar). O'yinda Esc - pauza menyusi.
rem Daraja: Bozor_Oyin.bat 2  (bo'lmasa - sotuv.json "boshlangichDaraja", hozir 4 - Mehmonlar keldi)
call "%~dp0Scripts\ue_top.bat" || exit /b 1
set DARAJA_ARG=
if not "%1"=="" set DARAJA_ARG=-Daraja=%1
start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0BozorUnreal.uproject" -game -windowed -ResX=1600 -ResY=900 %DARAJA_ARG% -log
