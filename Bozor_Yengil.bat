@echo off
setlocal
set "UE_EXE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UE_EXE%" (
    echo Unreal Engine 5.8 topilmadi: "%UE_EXE%"
    pause
    exit /b 1
)

rem 720p chiqish, 50%% ichki render (640x360), DX11/SM5.
rem Qo'shimcha parametrlar sinov uchun uzatiladi.
start "" "%UE_EXE%" "%~dp0BozorUnreal.uproject" -game -dx11 -windowed -ResX=1280 -ResY=720 -ForceRes -abslog="%~dp0Saved\Logs\Bozor_Yengil.log" -ExecCmds="scalability 0,r.ScreenPercentage 50,r.DynamicGlobalIlluminationMethod 0,r.ReflectionMethod 0,r.Shadow.Virtual.Enable 0,r.VolumetricFog 0,r.SkeletalMeshLODBias 1,r.StaticMeshLODDistanceScale 1.5,r.VSync 0,t.MaxFPS 30" %*
