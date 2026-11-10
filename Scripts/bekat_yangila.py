"""Bozor xaritasiga avtobus bekati va intro avtobusini qo'yadi (xaritani qayta qurmasdan): bozor_bekat.py.
Bor aktorlar joyida qoladi (qo'lda qilingan o'zgarishlar saqlanadi), saqlashdan oldin xarita nusxasi Saved/XaritaZaxira'ga olinadi.
Faqat editor yopiq holda (ochiq editor saqlanmagan o'zgarishlarni shu faylga yozib yuboradi).
Commandlet'da: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/bekat_yangila.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_bekat
importlib.reload(bozor_bekat)
import unreal

LOG = os.path.join(unreal.Paths.project_saved_dir(), "bekat_yangila.txt")
if os.path.exists(LOG):
    os.remove(LOG)


def log(*a):
    msg = " ".join(str(x) for x in a)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(msg + "\n")
    unreal.log_warning("BOZOR " + msg)


MAP = "/Game/Bozor/Maps/Bozor"
bozor_bekat.zaxira(log)
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
bozor_bekat.apply(world, log)
log("bekat: saqlandi", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))
