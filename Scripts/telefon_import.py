"""NPC qo'lidagi telefon: mesh, materiallar, "cho'ntakdan olish / qarash / qaytarish" kliplari, Phone soketi.

  1) Assets/Telefon/Export/SM_Telefon_<Redmi|Oddiy|Multfilm>.fbx (Blender, Fab assetlaridan) -> /Game/Bozor/Telefon/<nom>/.
     Haqiqiy o'lcham (~15-17 x 7.5 x 0.85 sm), pivot - telefon markazi, tepasi +X, ekrani +Z.
  2) Civilian NPCs AI paketining AS_PhoneStart / AS_Phone_Loop / AS_Phone_End kliplari Citizen skeletlariga (Male, Female)
     retarget qilinadi -> /Game/Bozor/CitizenAnim/<Male|Female>/Telefon/ (ABozorNpc shu yo'ldan yuklaydi).
  3) Civilian manekenining soketlari (Phone ...) Citizen skeletlariga ko'chiriladi va jurnalga yoziladi.

Ishga tushirish (editor YOPIQ bo'lsin - skeletlar o'zgaradi):
  UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript "-script=D:/Unreal Projects/BozorUnreal/Scripts/telefon_import.py"
Jurnal: Saved/telefon_import.txt
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\telefon_import.txt"
log = bc.log
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.BozorLevelTools
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET

ROOT = bc.ROOT
DEST = ROOT + "/Telefon"
SRC = r"D:\Unreal Projects\BozorUnreal\Assets\Telefon\Export"
CIV_MESH = "/Game/CivilianNPCsAI/TPS_Example/Characters/Mannequins/Meshes/SKM_Manny"
CIV_ANIM = "/Game/CivilianNPCsAI/AI_BP/AnimationBlueprint/Animations/"
KLIPLAR = ["AS_PhoneStart", "AS_Phone_Loop", "AS_Phone_End"]
CITIZEN = {
    "Male": ("/Game/CitizenNPC/CharacterParts/Meshes/Basebody/SK_ma_body_average", ROOT + "/Retarget/Citizen/IK_CitizenMale"),
    "Female": ("/Game/CitizenNPC/CharacterParts/Meshes/Basebody/SK_fe_body_average", ROOT + "/Retarget/Citizen/IK_CitizenFemale"),
}


# ------------------------------------------------------------------ 1) mesh va materiallar
def material():
    path = DEST + "/M_Telefon"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    m = tools.create_asset("M_Telefon", DEST, unreal.Material, unreal.MaterialFactoryNew())
    rang = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    rang.set_editor_property("parameter_name", "Rang")
    gadir = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 200)
    gadir.set_editor_property("parameter_name", "Gadir")
    gadir.set_editor_property("default_value", 0.35)
    nur = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 320)
    nur.set_editor_property("parameter_name", "Nur")
    kop = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 360)
    mel.connect_material_expressions(rang, "", kop, "A")
    mel.connect_material_expressions(nur, "", kop, "B")
    mel.connect_material_property(rang, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(gadir, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(kop, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def instansiya(asos, nomi, rgb, gadir, nur):
    path = DEST + "/MI_Telefon_" + nomi
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        "MI_Telefon_" + nomi, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, asos)
    mel.set_material_instance_vector_parameter_value(mi, "Rang", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Gadir", gadir)
    mel.set_material_instance_scalar_parameter_value(mi, "Nur", nur)
    eal.save_loaded_asset(mi)
    return mi


def pipeline(nomi, materiallar):
    path = bc.make_pipeline(nomi, static=True)
    p = eal.load_asset(path)
    p.mesh_pipeline.set_editor_property("build_nanite", False)   # NPC qo'lida, harakatda - oddiy mesh
    if materiallar:   # Redmi va Oddiy: FBX ichidagi teksturali materiallar
        p.material_pipeline.set_editor_property("import_materials", True)
        p.material_pipeline.texture_pipeline.set_editor_property("import_textures", True)
    eal.save_loaded_asset(p)
    return path


def mesh():
    # Blender'da tayyorlangan (Assets/Telefon/*.blend): Redmi 12 va oddiy smartfon (Fab) - o'z teksturalari bilan,
    # Cartoon Phone 3D (Fab) - teksturasiz, Korpus/Ekran slotlariga M_Telefon nusxalari
    for nom, fbx, tayyor_mat in (("Redmi", "SM_Telefon_Redmi.fbx", True), ("Oddiy", "SM_Telefon_Oddiy.fbx", True),
                                 ("Multfilm", "SM_Telefon_Multfilm.fbx", False)):
        dest = DEST + "/" + nom
        bc.interchange_import(os.path.join(SRC, fbx), dest, pipeline("P_Telefon_" + nom, tayyor_mat))
        sm = eal.load_asset("%s/SM_Telefon_%s" % (dest, nom))
        if not sm:
            log("telefon: import bo'lmadi", fbx)
            continue
        if not tayyor_mat:
            asos = material()
            korpus = instansiya(asos, "Korpus", (0.012, 0.012, 0.014), 0.3, 0.0)
            ekran = instansiya(asos, "Ekran", (0.10, 0.28, 0.75), 0.15, 2.5)
            for i, s in enumerate(sm.static_materials):
                sm.set_material(i, ekran if "ekran" in str(s.material_slot_name).lower() else korpus)
        sm.set_editor_property("light_map_resolution", 16)
        eal.save_loaded_asset(sm)
        eal.save_directory(dest, only_if_is_dirty=True, recursive=True)
        b = sm.get_bounding_box()
        log("telefon mesh:", nom, [str(s.material_slot_name) + "=" + (s.material_interface.get_name() if s.material_interface else "-")
                                   for s in sm.static_materials], "min", b.min, "max", b.max)


# ------------------------------------------------------------------ 2) kliplar Citizen skeletlariga
def retargeter(jins, tgt_mesh, tgt_rig):
    path = "%s/Retarget/Citizen/RTG_Civilian_Citizen%s" % (ROOT, jins)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    src_rig = unreal.load_asset(ROOT + "/Retarget/IK_Src_Civilian")
    src_mesh = unreal.load_asset(CIV_MESH)
    rtg = tools.create_asset("RTG_Civilian_Citizen" + jins, ROOT + "/Retarget/Citizen", unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(S, src_rig)
    rc.set_ik_rig(T, tgt_rig)
    rc.set_preview_mesh(S, src_mesh)
    rc.set_preview_mesh(T, tgt_mesh)
    if rc.get_num_retarget_ops() == 0:
        rc.add_default_ops()
    rc.assign_ik_rig_to_all_ops(S, src_rig)
    rc.assign_ik_rig_to_all_ops(T, tgt_rig)
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    rc.auto_align_all_bones(T)
    eal.save_loaded_asset(rtg)
    return rtg


def kliplar():
    src_mesh = unreal.load_asset(CIV_MESH)
    ads = [eal.find_asset_data(CIV_ANIM + k) for k in KLIPLAR]
    ads = [a for a in ads if a and a.is_valid()]
    for jins, (mesh_path, rig_path) in CITIZEN.items():
        tgt_mesh, tgt_rig = unreal.load_asset(mesh_path), unreal.load_asset(rig_path)
        if not tgt_mesh or not tgt_rig:
            log("kliplar:", jins, "skelet yoki IK rig topilmadi", mesh_path, rig_path)
            continue
        rtg = retargeter(jins, tgt_mesh, tgt_rig)
        dest = "%s/CitizenAnim/%s/Telefon" % (ROOT, jins)
        res = unreal.IKRetargetBatchOperation.duplicate_and_retarget(ads, src_mesh, tgt_mesh, rtg, target_path=dest,
                                                                    include_referenced_assets=False, overwrite_existing_files=True)
        eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
        log("kliplar:", jins, "->", sorted(str(r.asset_name) for r in res))
        for k in KLIPLAR:
            a = unreal.load_asset(dest + "/" + k)
            if a:
                log("   ", k, "uzunlik %.2f s" % a.get_play_length(), "skelet", a.get_editor_property("skeleton").get_name())


# ------------------------------------------------------------------ 3) soketlar
SOKET_NOMLARI = ["Phone", "PhoneSocket", "phone", "Phone_Socket", "SOCKET_Phone"]


def soket_yoz(nom, mesh):
    for n in SOKET_NOMLARI:
        s = mesh.find_socket(n)
        if s:
            log("   ", nom, "soket", n, "suyak", s.bone_name, "joy", s.relative_location, "burilish", s.relative_rotation)
            return
    log("   ", nom, "Phone soketi topilmadi")


def soketlar():
    src = unreal.load_asset(CIV_MESH)
    paket_tel = unreal.load_asset("/Game/CivilianNPCsAI/AI_BP/Meshes/SM_Phone")
    if paket_tel:   # paketning o'z telefoni soketga qanday o'qlarda qo'yilgan - bizniki shunga moslanadi
        b = paket_tel.get_bounding_box()
        log("paket SM_Phone:", "min", b.min, "max", b.max)
    soket_yoz("manba", src)
    for jins, (mesh_path, _) in CITIZEN.items():
        m = unreal.load_asset(mesh_path)
        sk = m.get_editor_property("skeleton") if m else None
        if not sk:
            continue
        n = lib.copy_sockets(src, sk)
        eal.save_loaded_asset(sk)
        log("soketlar:", jins, sk.get_path_name(), "qo'shildi", n)
        soket_yoz(jins, m)


# TELEFON_QISM=mesh,klip,soket (vergul bilan; bo'sh - hammasi)
QISM = [q for q in os.environ.get("TELEFON_QISM", "mesh,klip,soket").split(",") if q]
if "mesh" in QISM:
    mesh()
if "klip" in QISM:
    kliplar()
if "soket" in QISM:
    soketlar()
log("TAYYOR")
