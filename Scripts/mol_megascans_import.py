"""Quixel Megascans mollarini import qiladi: E:\\TRK\\Megascans_manba (Fab zip'lari ochilgan) -> /Game/Bozor/Mollar.

Har mol: SM_Mol_<Nom>_<n> (FBX, Nanite, kolliziyasiz), T_Mol_<Nom>_<n>_{BaseColor,Normal,AO} (1K PNG,
Scripts/mol_teksturalar_1k.ps1 tayyorlaydi), MI_Mol_<Nom>_<n> (M_BozorLit - boshqa Megascans mollari kabi:
Normal yashil kanali teskari, OpenGL -> DirectX). Nom o'zbekcha: sotuv.json "kalitlar" mesh nomidan molni topadi.
Muharrir ochiq bo'lsa ham bo'ladi (faqat yangi papkaga yozadi). Natija: Saved/mol_import.txt.
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/mol_megascans_import.py
"""
import glob
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

LOYIHA = os.path.abspath(unreal.Paths.project_dir())
bc.LOG = os.path.join(LOYIHA, "Saved", "mol_import.txt")
open(bc.LOG, "w").close()
log = bc.log
MANBA = r"E:\TRK\Megascans_manba"
DEST = bc.ROOT + "/Mollar"
mel = unreal.MaterialEditingLibrary

# zip nomi -> (mol nomi, Smoothness - M_BozorLit, Unity'dagi kabi; silliq po'stlilar yaltiroqroq)
MOLLAR = {
    "onion_tlwmdcwiw": ("Piyoz_1", 0.30), "onion_tlzkefghw": ("Piyoz_2", 0.30),
    "cucumber_tl2leafjw": ("Bodring_1", 0.35), "cucumber_vfendieiw": ("Bodring_2", 0.35),
    "zucchini_vdsicd0jw": ("Qovoqcha_1", 0.35), "lemon_th5jddwva": ("Limon_1", 0.45),
    "potato_tiimceoqx": ("Kartoshka_1", 0.15), "carrot_ub2maesjw": ("Sabzi_1", 0.25),
    "cabbage_tgrmacdpa": ("Karam_1", 0.30), "red_grapes_wjbgdiz": ("Uzum_1", 0.50),
    "pomegranate_ub4kcehhw": ("Anor_1", 0.40), "orange_tibhfezva": ("Apelsin_1", 0.45),
    "quince_vewlad1iw": ("Behi_1", 0.25), "kiwi_uiuiciiiw": ("Kivi_1", 0.15),
    "red_apple_tegqfetla": ("Olma_1", 0.55), "red_bell_pepper_uhriag3va": ("Qalampir_1", 0.65),
    "garlic_tlwlachjw": ("Sarimsoq_1", 0.20), "eggplant_uhqhbdshw": ("Baqlajon_1", 0.60),
    "watermelon_tguocjppa": ("Tarvuz_1", 0.40), "melon_tkkmbbohw": ("Qovun_1", 0.30),
}


def load(path):
    return unreal.load_asset(path) if bc.eal.does_asset_exist(path) else None


def tekstura(png, nom, tur):
    t = unreal.AssetImportTask()
    for k, v in (("filename", png), ("destination_path", DEST), ("destination_name", nom), ("automated", True),
                 ("replace_existing", True), ("save", False)):
        t.set_editor_property(k, v)
    bc.asset_tools.import_asset_tasks([t])
    tx = load("%s/%s" % (DEST, nom))
    if not tx:
        log("tekstura import bo'lmadi:", png)
        return None
    if tur == "Normal":
        tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tx.set_editor_property("srgb", False)
        tx.set_editor_property("flip_green_channel", True)
    elif tur == "AO":
        tx.set_editor_property("srgb", False)
    return tx


pipeline = bc.make_pipeline("PL_Mol", static=True)
ota = unreal.load_asset(bc.ROOT + "/Materials/M_BozorLit")
oq = unreal.load_asset(bc.ROOT + "/Textures/T_BozorWhiteLinear")
for papka in sorted(glob.glob(os.path.join(MANBA, "*_high"))):
    kalit = os.path.basename(papka)[:-5]
    if kalit not in MOLLAR:
        log("ro'yxatda yo'q, o'tkazildi:", kalit)
        continue
    mol, silliq = MOLLAR[kalit]
    fbx = glob.glob(os.path.join(papka, "*.fbx"))[0]
    tx = {tur: tekstura(os.path.join(MANBA, "_1k", "%s_%s.png" % (kalit, tur)), "T_Mol_%s_%s" % (mol, tur), tur)
          for tur in ("BaseColor", "Normal", "AO")}

    mi_yol = "%s/MI_Mol_%s" % (DEST, mol)
    mi = load(mi_yol) or bc.asset_tools.create_asset("MI_Mol_" + mol, DEST, unreal.MaterialInstanceConstant,
                                                     unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, ota)
    for p, t in (("BaseMap", tx["BaseColor"]), ("NormalMap", tx["Normal"]), ("OcclusionMap", tx["AO"]), ("MetallicGlossMap", oq)):
        if t:
            mel.set_material_instance_texture_parameter_value(mi, p, t)
    mel.set_material_instance_scalar_parameter_value(mi, "Smoothness", silliq)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", 0.0)
    mel.update_material_instance(mi)

    # FBX vaqtinchalik papkaga, keyin o'zbekcha nomga ko'chiriladi (qayta ishga tushirilsa eskisi almashadi)
    tmp = DEST + "/_tmp"
    obs = bc.interchange_import(fbx, tmp, pipeline)
    sm = next((o for o in obs if isinstance(o, unreal.StaticMesh)), None)
    if not sm:
        log("mesh import bo'lmadi:", fbx)
        continue
    nom = "%s/SM_Mol_%s" % (DEST, mol)
    if bc.eal.does_asset_exist(nom):
        bc.eal.delete_asset(nom)
    bc.eal.rename_asset(sm.get_path_name().split(".")[0], nom)
    sm = load(nom)
    for i in range(len(sm.static_materials)):
        sm.set_material(i, mi)
    b = sm.get_bounding_box()
    o = b.max - b.min
    log("%-12s %-28s %6d uchburchak  %5.1f x %5.1f x %5.1f sm  silliq %.2f" % (mol, kalit, sm.get_num_triangles(0), o.x, o.y, o.z, silliq))

if bc.eal.does_directory_exist(DEST + "/_tmp"):
    bc.eal.delete_directory(DEST + "/_tmp")
bc.eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
log("TAYYOR")
