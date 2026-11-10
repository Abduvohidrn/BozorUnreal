"""Bozor NPC'lari: Unity BozorNpc.cs ning C++ ko'chirmasi (ABozorNpc, ABozorNpcManager) bilan.

Har bir Unity NPC'si (sotuvchi, xaridor, hamroh, yo'lovchi, hammol, suhbatdosh) bitta ABozorNpc bo'ladi; xulqni
paketlarning AI'si emas, Unity'dagi bilan bir xil C++ korutinlari boshqaradi. Animatsiya: ABP_Manny (yurish/tik turish)
+ DefaultSlot'da Unity kategoriyalari (V_Idle, C_Talk ...).
Unity (m, chap qo'l, Y yuqori) -> Unreal (sm, Z yuqori): (x, y, z) -> (z, x, y) * 100, kvaternion (x, y, z, w) -> (z, x, y, w).
"""
import collections

import unreal

ROOT = "/Game/Bozor"
ABP = ROOT + "/Pack/Queue/ABP_Manny"
# MetaHuman: eski personaj -> MetaHuman (tana + yuz + o'tkazilgan kiyim). Animatsiyalar MetaHuman skeletiga retarget qilingan
# (/Game/Bozor/MetaAnim). Yuz va kiyim tana pozasiga ABozorNpc::BeginPlay'da (leader pose) ergashadi.
USE_MH = True
MH = {
    "Otaxon_Chopon_01": "MH_KeksaOtaxon", "Otaxon_Pidjak_01": "MH_DehqonOta", "Momo_01": "MH_KeksaMomo",
    "Ayol_Kofta_01": "MH_XaridorAyol", "Sotuvchi_Ayol_01": "MH_Onaxon", "Qiz_01": "MH_YoshSotuvchiQiz",
    "Qiz_02": "MH_Bosh_02", "Hammol_01": "MH_AravakashYigit", "Sotuvchi_Jilet_01": "MH_OrtaYoshliSotuvchi",
    "Sotuvchi_Sport_01": "MH_KochaSavdogari", "Erkak_Kurtka_01": "MH_ZamonaviyXaridor", "Yigit_01": "MH_TalabaYigit",
    "Yigit_02": "MH_YoshDehqon",
}
MH_ABP = ROOT + "/MetaAnim/ABP_Manny"
MH_ANIM = ROOT + "/MetaAnim"
FOLDER = "NPC"
CAPSULE_HALF = 88.0
ROLES = {
    "Sotuvchi": unreal.BozorRole.SOTUVCHI,
    "Xaridor": unreal.BozorRole.XARIDOR,
    "Hamroh": unreal.BozorRole.HAMROH,
    "Yolovchi": unreal.BozorRole.YOLOVCHI,
    "Hammol": unreal.BozorRole.HAMMOL,
    "Suhbatdosh": unreal.BozorRole.SUHBATDOSH,
}


def V(v, scale=100.0):
    return unreal.Vector(v[2] * scale, v[0] * scale, v[1] * scale)


def Q(q):
    return unreal.Quat(q[2], q[0], q[1], q[3])


def flat_dir(v):
    f = V(v, 1.0)
    ln = max((f.x ** 2 + f.y ** 2) ** 0.5, 1e-6)
    return unreal.Vector(f.x / ln, f.y / ln, 0)


def load(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def anim(name):
    if USE_MH:
        return load("%s/%s" % (MH_ANIM, name))
    return load("%s/Animations/%s" % (ROOT, name)) or load("%s/Pack/WW100/%s" % (ROOT, name))


# Fab'dagi tayyor kiyimlar (vaqtincha, rasmiy kiyimsiz): MetaHuman -> {rol: to'plam}, "*" - qolgan rollar.
# To'plamlar Blender'da b_fab_fit.py bilan har bir MetaHuman tanasiga moslangan (/Game/Bozor/MetaKiyim/<MH>/SKM_Fab_<to'plam>)
FAB = {
    "MH_AravakashYigit": {"*": "M_Hammol"},
    "MH_KochaSavdogari": {"Sotuvchi": "M_SotB", "*": "M_XarA"},
    "MH_OrtaYoshliSotuvchi": {"Sotuvchi": "M_SotA", "*": "M_XarB"},
    "MH_ZamonaviyXaridor": {"Sotuvchi": "M_SotV", "*": "M_XarB"},
    "MH_KeksaOtaxon": {"Sotuvchi": "M_KeksaSot", "*": "M_Keksa"},
    "MH_DehqonOta": {"*": "M_Keksa"},
    "MH_TalabaYigit": {"*": "M_Yosh"},
    "MH_YoshDehqon": {"*": "M_YoshB"},
    "MH_KeksaMomo": {"Sotuvchi": "W_A", "*": "W_Keksa"},
    "MH_Onaxon": {"Sotuvchi": "W_A", "*": "W_B"},
    "MH_XaridorAyol": {"Sotuvchi": "W_A", "*": "W_B"},
    "MH_YoshSotuvchiQiz": {"*": "W_C"},
    "MH_Bosh_02": {"*": "W_D"},
}


def mh_parts(character, role=None):
    """(tana, yuz, kiyim) skeletal mesh'lari yoki None. Kiyim rolga qarab Fab to'plamidan, bo'lmasa eski NPC kiyimi."""
    n = MH.get(character)
    if not (USE_MH and n):
        return None
    r = "/Game/MetaHumans/Heads/Unpacked/%s/" % n
    fab = FAB.get(n, {})
    kit = fab.get(role, fab.get("*"))
    outfit = load("%s/MetaKiyim/%s/SKM_Fab_%s" % (ROOT, n, kit)) if kit else None
    bp = load(r + "BP_" + n)
    parts = (load(r + "Body/SKM_%s_BodyMesh" % n), load(r + "Face/SKM_%s_FaceMesh" % n),
             outfit or load("%s/MetaKiyim/%s/SKM_Kiyim_%s" % (ROOT, n, character)), bp and bp.generated_class())
    return parts if all(parts) else None


# ---- Citizen NPC (D:\Citizen NPC paketi, /Game/CitizenNPC): bozor uslubidagi zamonaviy kundalik kiyim.
# Animatsiya: yashirin MetaHuman tanasi (Mesh) -> ABP_Citizen<Jins> (Retarget Pose From Mesh) bilan "CitizenPose"
# tegli yashirin Citizen tanasi -> kiyim/bosh/qo'l/shapka unga leader pose bilan ergashadi (ABozorNpc::LinkMetaHuman).
USE_CITIZEN = True
CIT = "/Game/CitizenNPC/CharacterParts/"
CIT_ABP = ROOT + "/Retarget/Citizen/ABP_Citizen%s"
# eski personaj -> bozor turi
CIT_TUR = {
    "Otaxon_Chopon_01": "Otaxon", "Otaxon_Pidjak_01": "Otaxon", "Momo_01": "Momo", "Ayol_Kofta_01": "Ayol",
    "Sotuvchi_Ayol_01": "Ayol", "Qiz_01": "Qiz", "Qiz_02": "Qiz", "Hammol_01": "Hammol", "Sotuvchi_Jilet_01": "Erkak",
    "Sotuvchi_Sport_01": "Erkak", "Erkak_Kurtka_01": "Erkak", "Yigit_01": "Yigit", "Yigit_02": "Yigit",
}
# to'plam: (kiyim, {slot: material}, qo'l, shapka/soch, {slot: material}); "ma" erkak, "fe" ayol
_KEPKA = ("Hats_Hair/SK_ma_hat_flatcap_01_hair", {})
_KEPKA_K = ("Hats_Hair/SK_ma_hat_flatcap_01_hair", {0: "MI_hat_flatcapl_01_blue"})
_BEYSBOLKA = ("Hats_Hair/SK_ma_hat_cap_01", {0: "MI_hat_cap_dark"})
_BEYSBOLKA_J = ("Hats_Hair/SK_ma_hat_cap_01", {0: "MI_hat_cap_01_brown"})
_SHAPKA = ("Hats_Hair/SK_ma_hat_casual_set_beanie_01_hair", {0: "MI_hat_casual-set_02_dark"})
_SOCH = ("Hats_Hair/SK_ma_hair_03", {})
# ro'mol: Assets/Romol/SK_Romol.fbx -> /Game/Bozor/Romol/SK_Romol_V2;
# naqsh/rang har bir NPC'ga indeks bo'yicha ROMOL_NAQSH dan
_RUMOL = ("ROMOL", {})
_RUMOL2 = ("ROMOL", {})
ROMOL = ROOT + "/Romol/"
ROMOL_NAQSH = ["01_Kok_Moychechak", "02_Zumrad_Atirgul", "03_Qora_Oltin",
               "04_Sutrang_Lola", "05_Binafsha_Yulduz", "06_Bordo_Chinnigul"]
_SOCH_Q = ("Hats_Hair/SK_fe_hair_05", {0: "MI_hair_02_dark"})
_SOCH_Q2 = ("Hats_Hair/SK_fe_hair_01", {0: "MI_fe_hair_01_dark", 1: "MI_fe_hair_01_dark"})
_QOL, _BILAK = "Basebody/SK_ma_hands_average", "Basebody/SK_ma_arm"
_QOLQOP = "SK_ma_gloves_worker_01"
_FQOL, _FBILAK, _FQOL2 = "Basebody/SK_fe_hands_average", "Basebody/SK_fe_arm_average", "Basebody/SK_fe_hands_muscular"
CIT_KIYIM = {
    ("Erkak", "Sotuvchi"): [
        ("SK_ma_clerk_01", {1: "MI_apron_02_brown"}, _QOL, _BEYSBOLKA),             # ko'ylak + fartuk
        ("SK_ma_waiter_02", {1: "MI_apron_02_stripes"}, _QOL, _KEPKA),               # jilet + fartuk
        ("SK_ma_casual_02_a", {1: "MI_casual_02_brown"}, _BILAK, _KEPKA_K),          # katak ko'ylak
        ("SK_ma_clerk_01", {1: "MI_apron_02_c_checker"}, _QOL, _KEPKA)],
    ("Erkak", "*"): [
        ("SK_ma_casual_jacket_01_a", {0: "MI_casual_jacket_01_dark"}, _QOL, _KEPKA),  # to'q kurtka
        ("SK_ma_casual_jacket_01_b", {0: "MI_casual_jacket_01_brown"}, _QOL, _SOCH),
        ("SK_ma_casual_02_a", {1: "MI_casual_02_brown"}, _BILAK, _KEPKA_K),
        ("SK_ma_casual_02_c", {1: "MI_casual_02_blue"}, _QOL, _SOCH),
        ("SK_ma_sport_01_a", {}, _QOL, _BEYSBOLKA)],         # sport kostyum
    ("Yigit", "Sotuvchi"): [
        ("SK_ma_clerk_01", {1: "MI_apron_02_brown"}, _QOL, _SHAPKA),
        ("SK_ma_sport_01_a", {}, _QOL, _BEYSBOLKA)],
    ("Yigit", "*"): [
        ("SK_ma_sport_01_a", {}, _QOL, _BEYSBOLKA),
        ("SK_ma_sport_01_c", {}, _QOL, _SHAPKA),
        ("SK_ma_casual_02_b", {1: "MI_casual_02"}, _QOL, _SOCH),                     # xudi
        ("SK_ma_casual_jacket_01_a", {0: "MI_casual_jacket_01_blue"}, _QOL, _SOCH)],
    ("Hammol", "*"): [
        ("SK_ma_construction_worker_01", {0: "MI_worker_tiles_01"}, _QOLQOP, _BEYSBOLKA_J),                 # ish kombinezoni + qo'lqop
        ("SK_ma_casual_02_b", {1: "MI_casual_02_brown"}, _QOLQOP, _SHAPKA),
        ("SK_ma_sport_01_c", {}, _QOLQOP, _BEYSBOLKA)],
    ("Otaxon", "Sotuvchi"): [
        ("SK_ma_waiter_02", {1: "MI_apron_02_brown"}, _QOL, _KEPKA),
        ("SK_ma_clerk_01", {1: "MI_apron_02_brown_stripes1"}, _QOL, _KEPKA)],
    ("Otaxon", "*"): [
        ("SK_ma_business_01_b", {}, _QOL, _KEPKA),        # pidjak
        ("SK_ma_business_01_b", {}, _QOL, _KEPKA_K),
        ("SK_ma_casual_jacket_01_a", {0: "MI_casual_jacket_01_brown"}, _QOL, _KEPKA)],
    ("Ayol", "Sotuvchi"): [
        ("SK_fe_clerk_02", {}, _FQOL2, _RUMOL),                                     # kofta + yo'l-yo'l fartuk
        ("SK_fe_clerk_02", {0: "MI_fe_casual_04_dark", 1: "MI_apron_02_brown"}, _FQOL2, _RUMOL2),
        ("SK_fe_clerk_01", {0: "MI_fe_casual_04_dark"}, _FBILAK, _RUMOL)],
    ("Ayol", "*"): [
        ("SK_fe_casual_02_a", {1: "MI_fe_casual_01_dark"}, _FQOL, _RUMOL),
        ("SK_fe_casual_02_b", {0: "MI_fe_casual_purple"}, _FBILAK, _RUMOL2),
        ("SK_fe_casual_02_c", {}, _FBILAK, _RUMOL),
        ("SK_fe_casual_02_a", {1: "MI_fe_casual_04_dark"}, _FQOL, _SOCH_Q)],
    ("Momo", "*"): [
        ("SK_fe_clerk_02", {0: "MI_fe_casual_04_dark", 1: "MI_apron_02_brown"}, _FQOL2, _RUMOL),
        ("SK_fe_casual_02_a", {1: "MI_fe_casual_01_dark"}, _FQOL, _RUMOL),
        ("SK_fe_casual_02_b", {0: "MI_fe_casual_04_dark"}, _FBILAK, _RUMOL)],
    ("Qiz", "*"): [
        ("SK_fe_casual_02_b", {0: "MI_fe_casual_03_blue_white"}, _FBILAK, _SOCH_Q),
        ("SK_fe_clerk_01", {1: "MI_apron_01_b_dark"}, _FBILAK, _SOCH_Q2),
        ("SK_fe_sport_02_a", {}, _FBILAK, _SOCH_Q),
        ("SK_fe_casual_02_c", {}, _FBILAK, _RUMOL2)],
}
CIT_BOSH = {
    "ma": ["SK_ma_head_05_asian", "SK_ma_head_05_basian", "SK_ma_head_01", "SK_ma_head_05_asian", "SK_ma_head_02"],
    "fe": ["SK_fe_head_02_asian", "SK_fe_head_02_b", "SK_fe_head_01"],
}
CIT_TERI = {"ma": "Basebody/MI_ma_skin_body_01_caucasian_yellow", "fe": "Basebody/MI_fe_skin_body_caucasian_yellow"}


def romol_material_for_outfit(mesh, overrides):
    """Match the scarf to the garment palette, with subdued cloth colors."""
    names = {str((overrides.get(i) or slot.material_interface).get_name())
             for i, slot in enumerate(mesh.materials)
             if overrides.get(i) or slot.material_interface}
    if "MI_fe_casual_purple" in names:
        pattern = "05_Binafsha_Yulduz"
    elif names & {"MI_fe_casual_01_dark", "MI_apron_02_brown_stripes1"}:
        pattern = "06_Bordo_Chinnigul"
    elif "MI_apron_02_brown" in names:
        pattern = "03_Qora_Oltin"
    elif "MI_fe_casual_04_dark" in names and "MI_apron_01_b_dark" not in names:
        pattern = "03_Qora_Oltin"
    else:
        pattern = "01_Kok_Moychechak"
    return load(ROMOL + "HairFit/MI_Romol_" + pattern + "_Muted")


def cit_parts(character, role, idx):
    """Citizen to'plami: (jins, [(nom, mesh, {slot: material}), ...]) yoki None."""
    tur = CIT_TUR.get(character)
    if not (USE_CITIZEN and tur):
        return None
    g = "fe" if tur in ("Ayol", "Momo", "Qiz") else "ma"
    kits = CIT_KIYIM.get((tur, role)) or CIT_KIYIM[(tur, "*")]
    kiyim, kmat, qol, (shapka, smat) = kits[idx % len(kits)]
    bosh = CIT_BOSH[g][(idx // len(kits) + idx) % len(CIT_BOSH[g])]
    parts = [("Kiyim", kiyim, kmat), ("Bosh", bosh, {}), ("Qol", qol, {}), ("Shapka", shapka, smat)]
    out = []
    for name, mesh, mats in parts:
        if mesh == "ROMOL":
            _, outfit, outfit_materials = out[0]
            out.append((name, load(ROMOL + "HairFit/SK_Romol_HairFit"),
                        {0: romol_material_for_outfit(outfit, outfit_materials)}))
            out.append(("Soch", load(ROMOL + "HairFit/SK_Soch_PastTugun"),
                        {0: load(ROMOL + "HairFit/M_Soch_PastTugun")}))
            continue
        m = load(CIT + "Meshes/" + mesh)
        if not m:
            return None
        out.append((name, m, {i: load(CIT + "Materials/" + (p if "/" in p else p)) or load(CIT + "Materials/Eye_hair/" + p)
                              for i, p in mats.items()}))
    return g, out


def cit_dress(actor, mesh_comp, g, parts):
    """Yashirin Citizen tanasi (CitizenPose) + unga biriktirilgan ko'rinadigan qismlar; teri bir xil ohangda."""
    body = add_part(actor, mesh_comp, "CitizenTana",
                    load(CIT + "Meshes/Basebody/SK_%s_body_average" % g))
    body.set_editor_property("anim_class", load(CIT_ABP % ("Male" if g == "ma" else "Female")).generated_class())
    body.set_editor_property("component_tags", ["CitizenPose"])
    body.set_editor_property("visibility_based_anim_tick_option",
                             unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    body.set_editor_property("visible", False)
    teri = load(CIT + "Materials/" + CIT_TERI[g])
    visible_parts = []
    for name, m, mats in parts:
        c = add_part(actor, body, name, m)
        visible_parts.append(c)
        for i, sm in enumerate(m.materials):
            mi = sm.get_editor_property("material_interface")
            if i in mats and mats[i]:
                c.set_material(i, mats[i])
            elif mi and "skin_body" in mi.get_name() and teri:
                c.set_material(i, teri)
    body.set_leader_pose_component(None)
    for c in visible_parts:
        c.set_leader_pose_component(body)
    mesh_comp.set_editor_property("visibility_based_anim_tick_option",
                                  unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    mesh_comp.set_visibility(False, False)
    return body


def add_part(actor, parent, name, mesh, cls=unreal.SkeletalMeshComponent):
    """Aktor nusxasiga parent'ga biriktirilgan qo'shimcha komponent (xaritada saqlanadi)."""
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    ph = next(h for h in sds.k2_gather_subobject_data_for_instance(actor) if lib.get_object(lib.get_data(h)) == parent)
    h, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=ph, new_class=cls))
    sds.rename_subobject(h, unreal.Text(name))
    c = lib.get_object(lib.get_data(h))
    if cls == unreal.ChildActorComponent:
        c.set_child_actor_class(mesh)
    else:
        c.set_skeletal_mesh_asset(mesh)
        c.set_editor_property("enable_update_rate_optimizations", True)
    return c


def dist2d(a, b):
    return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2) ** 0.5


def clear_old(log):
    """Eski NPC'lar (paket aktorlari, navbatlar) va oldingi ABozorNpc'lar o'chiriladi."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in actors.get_all_level_actors()
           if str(a.get_folder_path()).startswith(FOLDER) or isinstance(a, (unreal.BozorNpc, unreal.BozorNpcManager))]
    for a in old:
        actors.destroy_actor(a)
    log("npc: eski aktorlar o'chirildi", len(old))


def build(scene, log):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    npc = scene["npc"]
    npcs = npc["npcs"]
    abp = load(ABP).generated_class()
    mh_abp = load(MH_ABP).generated_class() if USE_MH else None

    def spawn(cls, loc, rot, label, folder):
        a = actors.spawn_actor_from_class(cls, loc, rot)
        a.set_actor_label(label)
        a.set_folder_path(folder)
        return a

    # ---- odamlar
    made = {}
    stats = collections.Counter()
    for i, n in enumerate(npcs):
        role = n["role"]
        feet = V(n["pos"])
        a = spawn(unreal.BozorNpc, unreal.Vector(feet.x, feet.y, feet.z + CAPSULE_HALF + 2), Q(n["rot"]).rotator(), n["name"], "%s/%s" % (FOLDER, role))
        m = a.get_editor_property("mesh")
        mh = mh_parts(n["character"], role)
        cit = cit_parts(n["character"], role, i) if mh else None
        if cit:
            # yashirin MetaHuman tanasi animatsiya manbai, ko'rinadigan gavda - Citizen qismlari
            m.set_editor_property("skeletal_mesh_asset", mh[0])
            m.set_editor_property("anim_class", mh_abp)
            cit_dress(a, m, *cit)
            stats["kiyim_" + cit[1][0][1].get_name()] += 1
            stats["Citizen"] += 1
        elif mh:
            m.set_editor_property("skeletal_mesh_asset", mh[0])
            m.set_editor_property("anim_class", mh_abp)
            # yuz, soch, qosh: MetaHuman BP'sining o'zi (child actor). Alohida qo'shilgan groom'lar o'yinda
            # ko'rinmaydi - BP'dagi LODSync va MetaHuman komponentlari kerak. Tanasi ABozorNpc::BeginPlay'da ergashadi.
            add_part(a, m, "MH", mh[3], unreal.ChildActorComponent)
            add_part(a, m, "Kiyim", mh[2])
            stats["kiyim_" + mh[2].get_name()] += 1
            stats["MetaHuman"] += 1
        else:
            if USE_MH:
                stats["MH_topilmadi_" + n["character"]] += 1
            m.set_editor_property("skeletal_mesh_asset", load("%s/Characters/%s" % (ROOT, n["character"])))
            m.set_editor_property("anim_class", abp)
        s = float(n["scale"][1])
        m.set_editor_property("relative_scale3d", unreal.Vector(s, s, s))
        m.set_editor_property("enable_update_rate_optimizations", True)
        a.set_editor_property("npc_role", ROLES[role])
        a.set_editor_property("female", bool(n["female"]))
        a.set_editor_property("walk_style", int(n.get("walkStyle", -1)))
        a.set_editor_property("walk_speed", float(n["walkSpeed"]) * 100.0)
        a.set_editor_property("group_slot", int(n["groupSlot"]))
        a.set_editor_property("order", int(n["sibling"]))
        a.set_editor_property("cart", bool(n.get("cart")))
        made[i] = a
        stats[role] += 1
    for i, n in enumerate(npcs):
        if n["role"] == "Hamroh" and n["leader"] in made:
            made[i].set_editor_property("leader", made[n["leader"]])

    # ---- suhbat guruhlari: bir guruh - bitta raqam, LookTarget - guruh markazi
    groups = collections.defaultdict(list)
    for i, n in enumerate(npcs):
        if n["role"] == "Suhbatdosh":
            groups[n["lookTargetName"] or n["name"]].append(i)
    for g, (key, members) in enumerate(sorted(groups.items())):
        for i in members:
            n = npcs[i]
            if n.get("lookTarget"):
                center = V(n["lookTarget"])
            else:
                ps = [V(npcs[j]["pos"]) for j in members]
                center = unreal.Vector(sum(p.x for p in ps) / len(ps), sum(p.y for p in ps) / len(ps), ps[0].z)
            made[i].set_editor_property("talk_group", g)
            made[i].set_editor_property("look_target", center)

    # ---- menejer: harakatlar, navbatlar, o'rindiqlar, yurish nuqtalari
    mgr = spawn(unreal.BozorNpcManager, V(npc["gatePoints"][0]) if npc["gatePoints"] else unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), "Bozor_NPC_Menejeri", FOLDER)
    acts, missing = [], set()
    for x in npc["actions"]:
        a = anim(x["state"])
        if not a:
            missing.add(x["state"])
            continue
        acts.append(unreal.BozorNpcAction(category=x["category"], anim=a, loop=bool(x["loop"])))
    mgr.set_editor_property("actions", acts)
    queues = []
    for s in npc["spots"]:
        if s["vendor"] not in made:
            continue
        pts = [V(p) for p in s.get("points") or [s["pos"]]]
        pat = s.get("patience", [20, 45])
        queues.append(unreal.BozorQueueSpot(points=pts, forward=flat_dir(s["fwd"]), vendor=made[s["vendor"]], stall=int(s.get("stall", -1)),
                                            start_only=s.get("entry", "StartOnly") == "StartOnly",
                                            can_leave_randomly=bool(s.get("canLeaveRandomly", True)), patience=unreal.Vector2D(pat[0], pat[1])))
    mgr.set_editor_property("queues", queues)
    vend = [V(n["pos"]) for n in npcs if n["role"] == "Sotuvchi"]
    seats = []
    for s in npc["seats"]:
        p = V(s["pos"])
        if any(dist2d(p, v) < 140 for v in vend):      # sotuvchi yonidagi stul sotuvchiniki
            continue
        seats.append(unreal.BozorSeat(position=p, forward=flat_dir(s["fwd"])))
    mgr.set_editor_property("seats", seats)
    mgr.set_editor_property("wander_points", [V(p) for p in npc["wanderPoints"]])
    mgr.set_editor_property("gate_points", [V(p) for p in npc["gatePoints"]])
    bags = [load("%s/Meshes/SM_SM_Paket_%s_LOD0" % (ROOT, c)) for c in ("Qora", "Oq")]
    mgr.set_editor_property("bag_meshes", [b for b in bags if b])
    mgr.set_editor_property("cart_mesh", load(ROOT + "/Meshes/SM_SM_Aravakash_Arava_LOD0"))
    log("npc: ABozorNpc", dict(stats), "navbat", len(queues), "o'rindiq", len(seats), "harakat", len(acts),
        "topilmagan klip", sorted(missing))
    return mgr
