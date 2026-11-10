"""MS_Bozor_Fon'ni qayta quradi va mavjud Bozor xaritasidagi fon aktorini yangilaydi (xaritani qayta qurmasdan).
Commandlet'da: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/fon_yangila.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import bozor_fon
importlib.reload(bozor_fon)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\fon_yangila.txt"
log = bc.log
MAP = bc.ROOT + "/Maps/Bozor"

try:
    ms = bozor_fon.build_metasound(log)
    if hasattr(unreal, "BozorFonZonalar"):
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        bozor_fon.place(world, log, ms, bc.load_scene().get("ambience"))
        bozor_fon.place_zonalar(world, log)
        log("fon: xarita saqlandi", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))
    else:
        log("fon: ABozorFonZonalar hali kompilyatsiya qilinmagan - xarita o'zgarmadi")
except bozor_fon.FonError as e:
    log("fon: XATO", e)
