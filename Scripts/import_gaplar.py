"""Sotuvchi suhbat gaplarini import qiladi: AI/tts/gaplar/*.wav (gaplar_yoz.py, Navoiy) -> /Game/Bozor/Audio/Gaplar.

Ovoz qoidalari Sotuvchi kliplari bilan bir xil (import_audio.py): SC_Ovoz, Att_Sotuvchi, Con_Sotuvchi.
O'yin bo'laklarni nomi bilan yuklaydi (ABozorSotuvchiOvozi::SayGap), xaritadan havola yo'q - shuning uchun papka
DefaultGame.ini da DirectoriesToAlwaysCook ga qo'shilgan. Diskda yo'q eski gaplar Unreal'dan o'chiriladi.
Qayta ishga tushirsa bo'ladi (faqat yangi/o'zgargan fayllar). Ishga tushirish:
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/import_gaplar.py
"""
import glob
import os
import sys
import time

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

LOYIHA = os.path.abspath(unreal.Paths.project_dir())
bc.LOG = os.path.join(LOYIHA, "Saved", "import_gaplar.txt")
log = bc.log
SRC = os.path.join(LOYIHA, "AI", "tts", "gaplar")
DEST = bc.ROOT + "/Audio/Gaplar"
TUZ = bc.ROOT + "/Audio/Tuzilma"
T0 = time.time()


def load(path):
    return unreal.load_asset(path) if bc.eal.does_asset_exist(path) else None


# "<nom>.sinov.wav" - gaplar_yoz.py sifat nazoratining vaqtinchalik fayli (to'xtatilsa qolib ketishi mumkin)
files = sorted(f for f in glob.glob(os.path.join(SRC, "*.wav")) if "." not in os.path.basename(f)[:-4])
nomlar = {os.path.splitext(os.path.basename(f))[0] for f in files}
# yangi yoki asset'dan keyin o'zgargan fayllar (asset fayli .uasset vaqti bilan solishtiriladi)
kontent = os.path.join(LOYIHA, "Content", "Bozor", "Audio", "Gaplar")
yangi = [f for f in files if not os.path.exists(os.path.join(kontent, os.path.splitext(os.path.basename(f))[0] + ".uasset"))
         or os.path.getmtime(f) > os.path.getmtime(os.path.join(kontent, os.path.splitext(os.path.basename(f))[0] + ".uasset"))]
log("gaplar: diskda %d, import qilinadi %d" % (len(files), len(yangi)))

sc = load(TUZ + "/SC_Ovoz")
att = load(TUZ + "/Att_Sotuvchi")
con = load(TUZ + "/Con_Sotuvchi")
for i in range(0, len(yangi), 200):
    tasks = []
    for f in yangi[i:i + 200]:
        t = unreal.AssetImportTask()
        for k, v in (("filename", f), ("destination_path", DEST), ("automated", True), ("replace_existing", True), ("save", False)):
            t.set_editor_property(k, v)
        tasks.append(t)
    bc.asset_tools.import_asset_tasks(tasks)
    for f in yangi[i:i + 200]:
        w = load("%s/%s" % (DEST, os.path.splitext(os.path.basename(f))[0]))
        if not isinstance(w, unreal.SoundWave):
            log("gaplar: import bo'lmadi", f)
            continue
        w.set_editor_property("looping", False)
        if sc:
            w.set_editor_property("sound_class_object", sc)
        if att:
            w.set_editor_property("attenuation_settings", att)
        if con:
            cs = unreal.Set(unreal.SoundConcurrency)
            cs.add(con)
            w.set_editor_property("concurrency_set", cs)
    bc.eal.save_directory(DEST, only_if_is_dirty=True, recursive=False)
    log("gaplar: %d/%d  %.0fs" % (min(i + 200, len(yangi)), len(yangi), time.time() - T0))

# diskdan olib tashlangan (eski matn) gaplar
ochdi = 0
for p in bc.eal.list_assets(DEST, recursive=False, include_folder=False):
    nom = p.rsplit("/", 1)[-1].split(".")[0]
    if nom not in nomlar and bc.eal.delete_asset(p):
        ochdi += 1
log("gaplar: TAYYOR, Unreal'da %d, eskisi o'chirildi %d, %.0fs" % (len(bc.eal.list_assets(DEST, recursive=False)), ochdi, time.time() - T0))
