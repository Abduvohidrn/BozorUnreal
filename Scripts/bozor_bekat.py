"""O'yin boshlanishi: bozor qarshisidagi avtobus bekati va o'yinchini olib keladigan avtobus.

Bekat: Fab "Bus Stop Shelters & Signs Pack (Australian)" (JoTrain) - shisha devorli, o'rindiqli soyabon (bus_shelter02)
va ustun; ko'k "Avtobus bekati" belgisi va elektron tablo ABozorBekat'da (Toshkentning yangi bekatlari kabi).
Avtobus: City Bus (/Game/City_Bus/Blueprints/BP_City_Bus) - yashil, "Zero emission", elektr, 4-yo'nalish.
Intro: ABozorAvtobusIntro (BozorAvtobus.cpp).

Ko'cha (Bozor xaritasi): bozor darvozasi x=0 da, uning oldida y bo'ylab asfalt x -1400..-400 (10 m), g'arbiy yo'lak x -1800..-1400.
O'ng tomonlama harakat: avtobus +Y ga yuradi, o'ng eshiklari g'arbga (bekatga) qaraydi.

Commandlet: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/bekat_yangila.py
"""
import os

import unreal

ROOT = "/Game/Bekat"
FBX = (r"E:\Unreal\Unreal Projects\VaultCache\FabLibrary\Bus_Stop_Shelters___Signs_Pack__Australian_-2a973e41"
       r"\fbx\bus_stop_shelters_sign_p_extracted\source\Brisbane Bus Stops.fbx")
BUS_BP = "/Game/City_Bus/Blueprints/BP_City_Bus"
FOLDER = "Bekat"

# bekat va avtobus to'xtash joyi (sm). Avtobus markazi - o'rta eshik bekat ro'parasiga to'g'ri keladigan joy.
# y=950: avtobus orqasi (~y 350) darvoza ro'parasidagi zebradan (y -200..200) 1.5 m narida
TOXTASH = unreal.Vector(-1245, 950, 0)
BEKAT_X = -1655                          # soyabon markazi: orqasi ariq tomonda, oldida 2 m chiqish joyi
# o'ng tomonlama harakat: shimolga ketayotgan avtobus yo'lning g'arbiy yarmida (x -1400..-900). Nuqtalar kam va keng -
# burilishlar silliq (yaqin nuqtalar splaynni to'lqinlantirib avtobusni qaltiratardi)
KELISH = [(-1150, -3400), (-1150, -700), (-1245, 350), (-1245, 950)]
# bekatdan chiqib o'z bo'lagiga qaytadi, shimoldagi chorrahada o'ngga - sharqqa ketayotgan bo'lakka (y ~4560) buriladi
KETISH = [(-1180, 1900), (-1150, 2900), (-1150, 3800), (-1060, 4250), (-820, 4500), (-450, 4565), (400, 4565), (7000, 4565)]

# zebra: darvoza ro'parasida (bozor tomoni bordyurida y -200..200 ochiq joy), yo'l x -1400..-400
ZEBRA_Y = (-200, 200)
ZEBRA_X = [-1340 + i * 98 for i in range(10)]   # tasmalar markazi, 50 sm keng

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def mesh(name):
    """FBX'da material va mesh nomlari bir xil - mesh'ga import "1" qo'shimchasini beradi"""
    for n in (name, name + "1"):
        a = load("%s/%s" % (ROOT, n))
        if isinstance(a, unreal.StaticMesh):
            return a
    return None


# ============================================================ assetlar
def import_fbx(log):
    if eal.does_asset_exist(ROOT + "/jpole_small_white"):
        return
    if not os.path.exists(FBX):
        raise RuntimeError("bekat FBX topilmadi: " + FBX)
    task = unreal.AssetImportTask()
    task.filename = FBX
    task.destination_path = ROOT
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])
    log("bekat: import", list(task.imported_object_paths or []))


def rang_material(log):
    """Bir rangli material va ko'k/qora nusxalari (belgi plitasi, tablo qutisi)."""
    path = ROOT + "/M_BekatRang"
    m = load(path)
    if not m:
        m = asset_tools.create_asset("M_BekatRang", ROOT, unreal.Material, unreal.MaterialFactoryNew())
        mel = unreal.MaterialEditingLibrary
        rang = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, 0)
        rang.set_editor_property("parameter_name", "Rang")
        mel.connect_material_property(rang, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 200)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.45)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(m)
        eal.save_loaded_asset(m)
    out = {}
    for name, color, rough in (("MI_Bekat_Kok", unreal.LinearColor(0.0, 0.11, 0.55, 1), 0.45),
                               ("MI_Bekat_Qora", unreal.LinearColor(0.012, 0.012, 0.014, 1), 0.45),
                               ("MI_Zebra", unreal.LinearColor(0.78, 0.78, 0.75, 1), 0.8)):   # yo'l bo'yog'i - biroz eskirgan oq
        mi = load(ROOT + "/" + name) or asset_tools.create_asset(name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", m)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Rang", color)
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
        eal.save_loaded_asset(mi)
        out[name] = mi
    log("bekat: materiallar", list(out))
    return out


def soyabon_material(log):
    """FBX importi teksturalarni topmaydi: soyabon atlas teksturasi (bus_shelter02_albedo) M_BozorLit nusxasiga, shisha - City Bus oynasi."""
    tex_path = ROOT + "/T_bus_shelter02_albedo"
    tex = load(tex_path)
    if not tex:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(os.path.dirname(os.path.dirname(FBX)), "textures", "bus_shelter02_albedo.png")
        task.destination_path = ROOT
        task.destination_name = "T_bus_shelter02_albedo"
        task.automated = True
        task.replace_existing = True
        task.save = True
        asset_tools.import_asset_tasks([task])
        tex = load(tex_path)
    mi = load(ROOT + "/MI_Bekat_Soyabon") or asset_tools.create_asset("MI_Bekat_Soyabon", ROOT, unreal.MaterialInstanceConstant,
                                                                      unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mi.set_editor_property("parent", load("/Game/Bozor/Materials/M_BozorLit"))
    # o'zbekcha plakatli nusxa (Scripts/yozuvlar_chiz.ps1 + yozuv_yangila.py) bo'lsa - o'sha
    mel.set_material_instance_texture_parameter_value(mi, "BaseMap", load(ROOT + "/T_Bekat_Soyabon_UZ") or tex)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(1, 1, 1, 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", 0.25)
    mel.set_material_instance_scalar_parameter_value(mi, "Smoothness", 0.4)
    eal.save_loaded_asset(mi)
    glass = load("/Game/City_Bus/Materials/MI_Glass")
    for name in ("bus_shelter02", "bus_shelter_window"):
        sm = mesh(name)
        if not sm:
            continue
        slots = []
        for i, s in enumerate(sm.static_materials):
            slot = str(s.material_slot_name)
            m = glass if "window" in slot.lower() or name.endswith("window") else mi
            sm.set_material(i, m)
            slots.append((slot, m.get_name()))
        eal.save_loaded_asset(sm)
        log("bekat: material", name, slots)


# ============================================================ sahna
def clear_old(log):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in actors.get_all_level_actors()
           if str(a.get_folder_path()).startswith(FOLDER) or isinstance(a, (unreal.BozorBekat, unreal.BozorAvtobusIntro))]
    for a in old:
        actors.destroy_actor(a)
    log("bekat: eski aktorlar o'chirildi", len(old))


def ground_z(world, x, y, default=0.0):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 500), unreal.Vector(x, y, -500),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        return hit.to_tuple()[4].z if hasattr(hit, "to_tuple") else default
    return default


def spawn(cls, loc, rot, label):
    a = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(cls, loc, rot)
    a.set_actor_label(label)
    a.set_folder_path(FOLDER)
    return a


def set_props(obj, props, log):
    lib = unreal.BozorLevelTools
    for k, v in props.items():
        if not lib.set_property_text(obj, k, v):
            log("bekat: xususiyat o'rnatilmadi", k)


# True - "Bekat" papkasi butunlay o'chirilib, hammasi skriptdagi joyiga qaytadan qo'yiladi (foydalanuvchining qo'lda qilgan
# o'zgarishlari yo'qoladi!). False (odatiy) - bor aktorlar joyida qoladi, faqat sozlamalari yangilanadi, yo'qlari qo'shiladi
QAYTA_QUR = False


def topish(label):
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def joy(cls, loc, rot, label, log):
    """Bor bo'lsa - o'shani (joyi va qo'lda qilingan o'zgarishlari bilan) qaytaradi, yo'q bo'lsa yaratadi."""
    a = topish(label)
    if a and a.get_class() == cls:
        log("bekat: bor, joyi saqlandi -", label)
        return a
    return spawn(cls, loc, rot, label)


def zaxira(log):
    """Xaritani saqlashdan oldin nusxasi: Saved/XaritaZaxira/Bozor_<vaqt>.umap"""
    import shutil
    import time
    src = os.path.join(unreal.Paths.project_content_dir(), "Bozor", "Maps", "Bozor.umap")
    dst_dir = os.path.join(unreal.Paths.project_saved_dir(), "XaritaZaxira")
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, time.strftime("Bozor_%Y%m%d_%H%M%S.umap"))
    shutil.copy2(src, dst)
    log("zaxira:", dst)


def place(world, log, mats):
    if QAYTA_QUR:
        clear_old(log)
    z = ground_z(world, BEKAT_X, TOXTASH.y, 0.0)   # commandlet'da nur ko'pincha tegmaydi; yo'lak sathi o'yinda o'lchangan - ~0
    log("bekat: yo'lak balandligi", z)

    bekat = joy(unreal.BozorBekat, unreal.Vector(BEKAT_X, TOXTASH.y, z), unreal.Rotator(0, 0, 0), "Bekat_DehqonBozori", log)
    bekat.soyabon.set_static_mesh(mesh("bus_shelter02"))
    oyna = mesh("bus_shelter_window")
    if oyna:
        bekat.oyna.set_static_mesh(oyna)
    bekat.set_editor_property("ustun_material", load("/Game/City_Bus/Materials/MI_Metal_Matte"))
    bekat.set_editor_property("kok_material", mats["MI_Bekat_Kok"])
    bekat.set_editor_property("qora_material", mats["MI_Bekat_Qora"])
    bekat.set_editor_property("soyabon_burilish", SOYABON_BURILISH)
    unreal.BozorLevelTools.rerun_construction(bekat)

    bus = joy(load(BUS_BP).generated_class(), TOXTASH, unreal.Rotator(0, 0, 90), "Avtobus_4", log)
    set_props(bus, {
        "Exterior Preset": "0",
        "Color": "(B=22,G=190,R=150,A=255)",          # och yashil-sariq (rasmdagi kabi)
        "Roof Color": "(B=235,G=235,R=235,A=255)",
        # "Toshkent shahar avtobusi" (Scripts/yozuv_yangila.py yaratadi); bo'lmasa paketdagi "Zero emission"
        "Decal Material": (ROOT + "/MI_Avtobus_Yozuv.MI_Avtobus_Yozuv") if eal.does_asset_exist(ROOT + "/MI_Avtobus_Yozuv")
        else "/Game/City_Bus/Materials/Decals/MI_Decal_Zero_Emission.MI_Decal_Zero_Emission",
        "Electric Engine": "True",
        "Route Number": "4",
        "Destination Text": "DEHQON BOZORI",
        # raqam to'liq teksturada (T_Reg_Plate o'rniga Toshkent raqami - yozuv_yangila.py): BP matnlari bo'sh
        "Text": "",
        "Country Code": "",
        "Passengers": "True",
        "Passengers %": "45",
        "Driver": "True",
        "Kneeling": "True",
    }, log)
    # BP o'zi o'yinchini egallaydi (haydash demosi) - bizda o'yinchi bola, avtobus faqat intro uchun
    bus.set_editor_property("auto_possess_player", unreal.AutoReceiveInput.DISABLED)
    bus.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.DISABLED)
    unreal.BozorLevelTools.rerun_construction(bus)

    intro = joy(unreal.BozorAvtobusIntro, unreal.Vector(TOXTASH.x, TOXTASH.y, 0), unreal.Rotator(0, 0, 0), "Avtobus_Intro", log)
    intro.set_editor_property("avtobus", bus)
    intro.set_editor_property("kelish_nuqtalari", [unreal.Vector(x, y, TOXTASH.z) for x, y in KELISH])
    intro.set_editor_property("ketish_nuqtalari", [unreal.Vector(x, y, TOXTASH.z) for x, y in KETISH])
    raqam = load(ROOT + "/M_Raqam_UZ")   # Toshkent raqami (Scripts/yozuv_yangila.py yaratadi)
    if raqam:
        intro.set_editor_property("raqam_material", raqam)
    unreal.BozorLevelTools.rerun_construction(intro)
    zebra(mats["MI_Zebra"], log)
    log("bekat: joylandi - bekat", bekat.get_actor_location(), "avtobus", bus.get_actor_location())


def zebra(mat, log):
    """Bekatdan bozor darvozasiga piyodalar o'tish joyi: yo'l bo'ylab cho'zilgan oq tasmalar (kub, kolliziyasiz), yo'ldan 1.6 sm baland."""
    kub = unreal.load_asset("/Engine/BasicShapes/Cube")
    uzun = ZEBRA_Y[1] - ZEBRA_Y[0]
    for i, x in enumerate(ZEBRA_X):
        label = "Zebra_%02d" % i
        bor = topish(label)
        a = bor or spawn(unreal.StaticMeshActor, unreal.Vector(x, (ZEBRA_Y[0] + ZEBRA_Y[1]) * 0.5, 0.8), unreal.Rotator(0, 0, 0), label)
        c = a.static_mesh_component
        c.set_static_mesh(kub)
        if not bor:   # bor tasmaning joyi/o'lchami qo'lda o'zgartirilgan bo'lishi mumkin - tegmaymiz
            c.set_world_scale3d(unreal.Vector(0.5, uzun / 100.0, 0.016))
        c.set_material(0, mat)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property("cast_shadow", False)
    log("bekat: zebra", len(ZEBRA_X), "tasma")


# modelda soyabon uzun tomoni X bo'ylab, orqa shisha devori -Y da: -90 - ochiq tomoni aktorning +X'iga (yo'lga) qaraydi
SOYABON_BURILISH = -90.0


def apply(world, log):
    import_fbx(log)
    mats = rang_material(log)
    soyabon_material(log)
    for p in ("bus_shelter02", "bus_shelter_window", "jpole_small_white", "jpole_white_lod0", "bus_stop_sign_rectangle02"):
        m = mesh(p)
        if m:
            bb = m.get_bounding_box()
            log("bekat: mesh", p, "min", bb.min, "max", bb.max)
        else:
            log("bekat: mesh yo'q", p)
    place(world, log, mats)
