"""O'zbekcha plakat va avtobus yozuvini Unreal'ga import qilib qo'llaydi (Scripts/yozuvlar_chiz.ps1 chizgan PNG'lar):
- /Game/Bekat/T_Bekat_Soyabon_UZ -> MI_Bekat_Soyabon (soyabondagi Avstraliya reklamalari o'rniga)
- /Game/Bekat/T_Avtobus_Yozuv -> MI_Avtobus_Yozuv (avtobus yonidagi "Zero emission" o'rniga), Bozor xaritasidagi Avtobus_4 ga
- /Game/Bekat/T_Raqam_UZ -> M_Raqam_UZ (Toshkent raqami, Yevropa plitasi o'rniga) -> Avtobus_Intro.RaqamMaterial
Tartib: bekat_yangila.py (bekat/avtobus/intro/zebra) dan keyin.
Commandlet: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/yozuv_yangila.py
"""
import os

import unreal

SRC = os.path.join(unreal.Paths.project_saved_dir(), "Yozuvlar", "out")
ROOT = "/Game/Bekat"
MAP = "/Game/Bozor/Maps/Bozor"
LOG = os.path.join(unreal.Paths.project_saved_dir(), "Yozuvlar", "yozuv_yangila.txt")
eal = unreal.EditorAssetLibrary
lines = []


def log(*a):
    lines.append(" ".join(str(x) for x in a))


def import_png(name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, name + ".png")
    task.destination_path = ROOT
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    t = unreal.load_asset("%s/%s" % (ROOT, name))
    log("import", name, t)
    return t


soyabon = import_png("T_Bekat_Soyabon_UZ")
yozuv = import_png("T_Avtobus_Yozuv")

mi = unreal.load_asset(ROOT + "/MI_Bekat_Soyabon")
unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "BaseMap", soyabon)
eal.save_loaded_asset(mi)
log("MI_Bekat_Soyabon BaseMap", soyabon.get_name())

dekal_path = ROOT + "/MI_Avtobus_Yozuv"
if not eal.does_asset_exist(dekal_path):
    eal.duplicate_asset("/Game/City_Bus/Materials/Decals/MI_Decal_Zero_Emission", dekal_path)
dekal = unreal.load_asset(dekal_path)
unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(dekal, "Texture", yozuv)
eal.save_loaded_asset(dekal)
log("dekal", dekal.get_path_name())

# Toshkent raqami: tekstura -> M_Raqam_UZ (oddiy material: tekstura BaseColor'ga). Intro uni avtobus plitalariga qo'yadi
raqam_tex = import_png("T_Raqam_UZ")
# paketdagi Yevropa plitasi teksturasi ham shu bilan almashtiriladi - muharrirda ham Toshkent raqami ko'rinsin
task = unreal.AssetImportTask()
task.filename = os.path.join(SRC, "T_Raqam_UZ.png")
task.destination_path = "/Game/City_Bus/Textures"
task.destination_name = "T_Reg_Plate"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
log("T_Reg_Plate <- T_Raqam_UZ")
raqam_path = ROOT + "/M_Raqam_UZ"
raqam = unreal.load_asset(raqam_path) if eal.does_asset_exist(raqam_path) else None
mel = unreal.MaterialEditingLibrary
if not raqam:
    raqam = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Raqam_UZ", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    ts = mel.create_material_expression(raqam, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    ts.set_editor_property("parameter_name", "Raqam")
    ts.set_editor_property("texture", raqam_tex)
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(raqam, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", 0.45)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(raqam)
eal.save_loaded_asset(raqam)
log("raqam", raqam.get_path_name())

import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bozor_bekat
bozor_bekat.zaxira(log)   # saqlashdan oldin xarita nusxasi
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_actor_label() == "Avtobus_4":
        for k in ("Text", "Country Code"):   # raqam teksturada
            unreal.BozorLevelTools.set_property_text(a, k, "")
        ok = unreal.BozorLevelTools.set_property_text(a, "Decal Material", dekal.get_path_name())
        unreal.BozorLevelTools.rerun_construction(a)
        log("Avtobus_4 Decal Material", ok)
    if isinstance(a, unreal.BozorAvtobusIntro):
        a.set_editor_property("raqam_material", raqam)
        log("Avtobus_Intro raqam_material")
log("saqlandi", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP))

with open(LOG, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
