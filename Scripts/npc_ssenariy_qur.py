"""Editorsiz: kerakli kliplarni retarget qiladi, ssenariylarni xaritaga yozadi va saqlaydi.
UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script="D:/Unreal Projects/BozorUnreal/Scripts/npc_ssenariy_qur.py"
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import unreal
import npc_anim_retarget
import npc_ssenariy

MAP = "/Game/Bozor/Maps/Bozor"
LOG = r"D:\Unreal Projects\BozorUnreal\Saved\npc_ssenariy_qur.txt"


def log(*a):
    s = " ".join(str(x) for x in a)
    print(s)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(s + "\n")


if os.path.exists(LOG):
    os.remove(LOG)
try:
    made = npc_anim_retarget.run()
    log("retarget:", made)
except Exception as e:
    import traceback
    log("retarget XATO:", traceback.format_exc())
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
stats, missing = npc_ssenariy.apply()
log("ssenariy:", stats, "topilmadi:", sorted(missing))
log("saqlandi:", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))
