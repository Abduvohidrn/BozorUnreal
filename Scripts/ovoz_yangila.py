"""Mavjud Bozor xaritasiga sotuvchilar ovozi va rasta ovozlari aktorlarini qo'yadi (xaritani qayta qurmasdan).
Commandlet'da: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/ovoz_yangila.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import bozor_ovoz
importlib.reload(bozor_ovoz)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\ovoz_yangila.txt"
if os.path.exists(bc.LOG):
    os.remove(bc.LOG)
MAP = bc.ROOT + "/Maps/Bozor"

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
scene = bc.load_scene()
a = bozor_ovoz.place(world, bc.log, scene)
b = bozor_ovoz.place_rasta(world, bc.log, scene)
c = bozor_ovoz.place_qadam(world, bc.log)
d = bozor_ovoz.place_pavilyonlar(world, bc.log)
if a or b or c or d:
    bc.log("ovoz: xarita saqlandi", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))
    # editor ochiq bo'lsa BOZOR_NO_CLEANUP=1: eski assetlarni editorning o'zi o'chirsin (commandlet ochiq editor ushlab
    # turgan fayllarni o'chira olmaydi, lekin "o'chirildi" deb xabar beradi)
    if os.environ.get("BOZOR_NO_CLEANUP") != "1":
        bozor_ovoz.cleanup_clips(bc.log)
