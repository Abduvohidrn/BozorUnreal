"""Mavjud Bozor xaritasida go'sht pavilyonini tuzatadi (xaritani qayta qurmasdan): bozor_gosht.py.
Commandlet'da: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/gosht_yangila.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import bozor_gosht
importlib.reload(bozor_gosht)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\gosht_yangila.txt"
if os.path.exists(bc.LOG):
    os.remove(bc.LOG)
MAP = bc.ROOT + "/Maps/Bozor"

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
bozor_gosht.apply(world, bc.log)
bc.log("gosht: navigatsiya", unreal.BozorLevelTools.build_navigation(world))
bc.log("gosht: saqlandi", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))
