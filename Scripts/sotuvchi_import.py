"""Faqat sotuvchi gaplarini import qiladi (BozorKit/Audio/Sotuvchi/sot_*.wav -> /Game/Bozor/Audio/Sotuvchi), qolgan ovozlarga va
materiallarga tegmaydi (import_audio.py hammasini qayta import qiladi va yer materiallarini ham saqlaydi).
Keyin: ovoz_yangila.py (Sotuvchi_Ovozlari aktori yangi ro'yxat bilan, diskda yo'q eski assetlar o'chiriladi).
Commandlet'da: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/sotuvchi_import.py
"""
import glob
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\sotuvchi_import.txt"
if os.path.exists(bc.LOG):
    os.remove(bc.LOG)
SRC = r"D:\Trk\BozorKit_hammasi\BozorKit\Audio\Sotuvchi"
DEST = bc.ROOT + "/Audio/Sotuvchi"
T = bc.ROOT + "/Audio/Tuzilma/"

files = sorted(glob.glob(os.path.join(SRC, "sot_*.wav")))
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
