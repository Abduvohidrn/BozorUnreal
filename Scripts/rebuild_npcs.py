"""Bozor xaritasidagi NPC'larni ABozorNpc (Unity xulqi) bilan almashtiradi, xaritaning qolgan qismiga tegmaydi.
Commandlet'da: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=rebuild_npcs.py
"""
import importlib
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bozor_common as bc
import bozor_npc
importlib.reload(bozor_npc)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\rebuild_npcs.txt"
if os.path.exists(bc.LOG):
    os.remove(bc.LOG)
MAP = "/Game/Bozor/Maps/Bozor"

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
bozor_npc.clear_old(bc.log)
bozor_npc.build(bc.load_scene(), bc.log)
bc.log("npc: navigatsiya", unreal.BozorLevelTools.build_navigation(world))
bc.log("npc: saqlandi", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))
