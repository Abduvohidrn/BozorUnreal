"""Haydaladigan BMW (ABozorMashina, Chaos Vehicle): skelet mesh import, materiallar, fizika asseti va sahnaga qo'yish.

1) Saved/BMWSource/SK_BMW_Haydash.glb (Scripts/bmw_haydash_eksport.py) -> /Game/Bozor/Mashinalar/BMW/Haydash/SK_BMW_Haydash
   materiallar SM_BMW slot nomlaridan ko'chiriladi; fizika asseti - kuzov uchun bitta quti (UBozorLevelTools.MashinaFizikasi).
2) Sahna: janubiy qatordagi turgan BMW (5600, -4530) o'rniga - uning ISM nusxasi, oynasi va raqami olib tashlanadi -
   Bozor_BMW_Haydash aktori (o'sha joyda, sharqqa qarab) qo'yiladi.

UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/bmw_haydash_import.py   (muharrir yopiq)
"""
import json
import math
import os
import stat

import unreal as ue

V = ue.Vector
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
SRC = os.path.join(PROJ, "Saved", "BMWSource")
DEST = "/Game/Bozor/Mashinalar/BMW/Haydash"
MAP = "/Game/Bozor/Maps/Bozor"
JOY = (5600.0, -4530.0)   # janubiy yo'l, shimoliy yarmi - sharqqa qarab (o'ng tomonlama harakat)
EAL = ue.EditorAssetLibrary


def log(*a):
    ue.log_warning("HAYDASH " + " ".join(str(x) for x in a))


def yozish(fayl):
    if os.path.exists(fayl):
        os.chmod(fayl, stat.S_IREAD | stat.S_IWRITE)   # git LFS 'lockable'


def import_skelet():
    papka = os.path.join(PROJ, "Content", *DEST.split("/")[2:])
    for ildiz, _, fs in os.walk(papka):
        for f in fs:
            yozish(os.path.join(ildiz, f))
    pl_yol = DEST + "/PL_Haydash"
    if EAL.does_asset_exist(pl_yol):
        EAL.delete_asset(pl_yol)
    src = ue.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline.DefaultAssetsPipeline")
    p = ue.AssetToolsHelpers.get_asset_tools().duplicate_asset("PL_Haydash", DEST, src)
    p.set_editor_property("asset_type_sub_folders", False)
    p.set_editor_property("scene_name_sub_folder", False)
    p.set_editor_property("reimport_strategy", ue.ReimportStrategyFlags.APPLY_PIPELINE_PROPERTIES)
    p.material_pipeline.set_editor_property("import_materials", False)
    p.material_pipeline.texture_pipeline.set_editor_property("import_textures", False)
    # normallar Unreal'da qayta hisoblanadi: Blender'da soddalashtirilgan skelet mesh'ning normallari buzilib, kuzov qora chiqardi
    p.common_meshes_properties.set_editor_property("recompute_normals", True)
    p.common_meshes_properties.set_editor_property("recompute_tangents", True)
    p.common_meshes_properties.set_editor_property("import_lods", False)
    p.common_skeletal_meshes_and_animations_properties.set_editor_property("import_only_animations", False)
    mp = p.mesh_pipeline
    mp.set_editor_property("import_static_meshes", False)
    mp.set_editor_property("import_skeletal_meshes", True)
    mp.set_editor_property("create_physics_asset", True)
    p.animation_pipeline.set_editor_property("import_animations", False)
    EAL.save_loaded_asset(p)
    mgr = ue.InterchangeManager.get_interchange_manager_scripted()
    params = ue.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines.append(ue.SoftObjectPath(pl_yol))
    mgr.import_asset(DEST, mgr.create_source_data(os.path.join(SRC, "SK_BMW_Haydash.glb")), params)
    EAL.delete_asset(pl_yol)
    hammasi = {p.split(".")[0]: EAL.find_asset_data(p).asset_class_path.asset_name
               for p in EAL.list_assets(DEST, recursive=False, include_folder=False)}
    log("import:", hammasi)
    sk = next(ue.load_asset(p) for p, c in hammasi.items() if c == "SkeletalMesh")
    pa = next((ue.load_asset(p) for p, c in hammasi.items() if c == "PhysicsAsset"), None)
    return sk, pa


def materiallar(sk):
    manba = ue.load_asset("/Game/Bozor/Mashinalar/BMW/SM_BMW")
    bor = {str(s.material_slot_name): s.material_interface for s in manba.static_materials}
    mats = sk.get_editor_property("materials")
    yoq = []
    for m in mats:
        nom = str(m.get_editor_property("material_slot_name"))
        if nom in bor:
            m.set_editor_property("material_interface", bor[nom])
        else:
            yoq.append(nom)
    sk.set_editor_property("materials", mats)
    log("materiallar:", len(mats), "topilmadi:", yoq)


def main():
    sk, pa = import_skelet()
    materiallar(sk)
    if not pa:
        raise RuntimeError("Fizika asseti yaratilmadi")
    # kuzov qutisi (mashina fazosida, sm): yerdan 25 sm dan tomgacha (146), uzunligi 496, eni 186
    ok = ue.BozorLevelTools.mashina_fizikasi(pa, sk, "root", V(0, 0, 83), V(492, 186, 116))
    sk.set_editor_property("physics_asset", pa)
    for p in EAL.list_assets(DEST, recursive=False, include_folder=False):   # skelet asseti ham (aks holda mesh buziladi)
        EAL.save_asset(p.split(".")[0], only_if_is_dirty=False)
    # kuzov materiallari skelet mesh'da ham ishlashi uchun "Skeletal Mesh" belgisi (bo'lmasa o'yinda standart material)
    for m in sk.get_editor_property("materials"):
        mi = m.get_editor_property("material_interface")
        baza = mi.get_base_material() if mi else None
        if baza and not baza.get_editor_property("used_with_skeletal_mesh"):
            yozish(os.path.join(PROJ, "Content", *baza.get_path_name().split(".")[0].split("/")[2:]) + ".uasset")
            baza.set_editor_property("used_with_skeletal_mesh", True)
            ue.MaterialEditingLibrary.recompile_material(baza)
            EAL.save_loaded_asset(baza, False)
            log("skelet belgisi:", baza.get_name())
    log("fizika asseti:", pa.get_name(), "quti:", ok)

    # sahna
    ue.EditorLoadingAndSavingUtils.load_map(MAP)
    act = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    actors = act.get_all_level_actors()
    for a in actors:
        if a.get_actor_label() == "Bozor_BMW_Haydash":
            act.destroy_actor(a)
    for a in actors:   # o'sha joydagi turgan BMW (kuzov, oyna, raqam ISM nusxalari) olib tashlanadi
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            if not c.static_mesh or c.static_mesh.get_name() not in ("SM_BMW", "SM_BMW_Oyna", "SM_Raqam_BMW"):
                continue
            for i in reversed(range(c.get_instance_count())):
                t = c.get_instance_transform(i, True).translation
                if math.hypot(t.x - JOY[0], t.y - JOY[1]) < 300:
                    c.modify()
                    c.remove_instance(i)
                    log("olib tashlandi:", c.static_mesh.get_name(), round(t.x), round(t.y))
    info = json.load(open(os.path.join(SRC, "haydash.json"), encoding="utf-8"))
    m = act.spawn_actor_from_class(ue.BozorMashina, V(JOY[0], JOY[1], 30), ue.Rotator(0, 0, 0))
    m.set_actor_label("Bozor_BMW_Haydash")
    m.set_folder_path("Bozor")
    m.get_editor_property("mesh").set_skeletal_mesh_asset(sk)
    tartib = ["wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr"]
    m.set_editor_property("gildirak_meshlari",
                          [ue.load_asset("/Game/Bozor/Mashinalar/BMW/Qatnov/" + info["meshlar"][t]) for t in tartib])
    m.set_editor_property("material_manbasi", ue.load_asset("/Game/Bozor/Mashinalar/BMW/SM_BMW"))   # skelet mesh materiallari
    m.set_editor_property("kuzov_mesh", ue.load_asset("/Game/Bozor/Mashinalar/BMW/Qatnov/SM_BMW_Qatnov"))   # ko'rinadigan kuzov
    m.set_editor_property("oyna_mesh", ue.load_asset("/Game/Bozor/Mashinalar/BMW/SM_BMW_Oyna"))
    m.set_editor_property("raqam_mesh", ue.load_asset("/Game/Bozor/Mashinalar/Raqamlar/SM_Raqam_BMW"))
    m.set_editor_property("dvigatel_ovozi", ue.load_asset("/Game/City_Bus/Sound/S_Engine_Diesel_1000"))
    m.set_editor_property("signal_ovozi", ue.load_asset("/Game/City_Bus/Sound/S_Horn"))
    m.set_editor_property("ovoz_sonishi", ue.load_asset("/Game/City_Bus/Sound/ATT_Engine"))
    yozish(os.path.join(PROJ, "Content", "Bozor", "Maps", "Bozor.umap"))
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    log("sahna saqlandi:", ue.EditorLoadingAndSavingUtils.save_map(world, MAP))


main()
