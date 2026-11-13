"""Sotuvchi suhbat ovozlarini import qiladi (BozorKit/Audio/Dialog/dlg_*.wav -> /Game/Bozor/Audio/Dialog).
WAV'lar va Content/Bozor/Data/ovoz_gaplar.json jadvalini BozorKit_hammasi/Ovoz/unreal_eksport.py tayyorlaydi.
Assetlar Sotuvchi_Ovozlari aktoriga qo'shilmaydi: ABozorSotuvchiOvozi::SayLine ularni jadvaldagi yo'l bo'yicha yuklaydi
(cook uchun DefaultGame.ini DirectoriesToAlwaysCook=/Game/Bozor/Audio/Dialog).
Commandlet'da (skript yo'li to'liq bo'lsin): UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript "-script=D:/Unreal Projects/BozorUnreal/Scripts/dialog_import.py"
"""
import glob
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\dialog_import.txt"
if os.path.exists(bc.LOG):
    os.remove(bc.LOG)
SRC = r"D:\Trk\BozorKit_hammasi\BozorKit\Audio\Dialog"
DEST = bc.ROOT + "/Audio/Dialog"
T = bc.ROOT + "/Audio/Tuzilma/"

files = sorted(glob.glob(os.path.join(SRC, "dlg_*.wav")))
tasks = []
for f in files:
    t = unreal.AssetImportTask()
    for k, v in (("filename", f), ("destination_path", DEST), ("automated", True), ("replace_existing", True), ("save", False)):
        t.set_editor_property(k, v)
    tasks.append(t)
bc.asset_tools.import_asset_tasks(tasks)
sc, att, con = (unreal.load_asset(T + n) for n in ("SC_Ovoz", "Att_Sotuvchi", "Con_Sotuvchi"))
ok = 0
for f in files:
    w = unreal.load_asset("%s/%s" % (DEST, os.path.splitext(os.path.basename(f))[0]))
    if not isinstance(w, unreal.SoundWave):
        bc.log("sotuvchi: import bo'lmadi", f)
        continue
    w.set_editor_property("looping", False)
    w.set_editor_property("sound_class_object", sc)
    w.set_editor_property("attenuation_settings", att)
    cs = unreal.Set(unreal.SoundConcurrency)
    cs.add(con)
    w.set_editor_property("concurrency_set", cs)
    bc.eal.save_loaded_asset(w, only_if_is_dirty=False)
    ok += 1
bc.log("sotuvchi: import %d / %d" % (ok, len(files)))
