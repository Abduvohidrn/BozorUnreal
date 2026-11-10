"""Bozor sahnasini Unity eksportidan (scene.json) quradi: /Game/Bozor/Maps/Bozor.

Obyektlar, yorug'lik va NavMesh - Unity sahnasidan. NPC'lar - ABozorNpc (Unity BozorNpc.cs ning C++ ko'chirmasi), bozor_npc.py.
Unity (m, chap qo'l, Y yuqori) -> Unreal (sm, Z yuqori): (x, y, z) -> (z, x, y) * 100, kvaternion (x, y, z, w) -> (z, x, y, w).
"""
import collections
import os
import re
import sys
import time

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import bozor_npc
importlib.reload(bozor_npc)
import bozor_fon
importlib.reload(bozor_fon)
import bozor_ovoz
importlib.reload(bozor_ovoz)
import bozor_gosht
importlib.reload(bozor_gosht)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\build_level.txt"
log = bc.log
scene = bc.load_scene()
ROOT = bc.ROOT
MAP = ROOT + "/Maps/Bozor"
T0 = time.time()
lib = unreal.BozorLevelTools
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

NO_COLLISION_GROUPS = {"Bozor/Decallar"}


# ============================================================ koordinatalar
def V(v, scale=100.0):
    return unreal.Vector(v[2] * scale, v[0] * scale, v[1] * scale)


def Q(q):
    return unreal.Quat(q[2], q[0], q[1], q[3])


def S(s):
    return unreal.Vector(s[2], s[0], s[1])


def T(pos, rot, scale):
    return unreal.Transform(V(pos), Q(rot).rotator(), S(scale))


def load(path):
    return unreal.load_asset(path) if bc.eal.does_asset_exist(path) else None


def mesh(mid):
    return load("%s/Meshes/%s" % (ROOT, mid))


def mat(mid):
    return load("%s/Materials/%s" % (ROOT, mid)) if mid else None


def spawn(cls, loc=unreal.Vector(0, 0, 0), rot=unreal.Rotator(0, 0, 0), label=None, folder=None):
    a = actors.spawn_actor_from_class(cls, loc, rot)
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def vadd(a, b, k=1.0):
    return unreal.Vector(a.x + b.x * k, a.y + b.y * k, a.z + b.z * k)


# ============================================================ assetlarni tayyorlash
def prepare_assets():
    used = collections.defaultdict(set)
    for o in scene["objects"]:
        used[o["mesh"]].add(o["group"])
    n = 0
    for mid, groups in used.items():
        if groups - NO_COLLISION_GROUPS:
            sm = mesh(mid)
            if sm:
                lib.set_complex_collision(sm)
                n += 1
    log("level: complex collision", n)
    # bitta skeletdagi turli bo'yli odamlar: suyak uzunligi har kimning o'zidan
    lib.setup_skeleton_retargeting(load(ROOT + "/Characters/Yigit_01_Skeleton"))
    # paket kliplari joyida o'ynaydi (yurishni AI beradi): root lock
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for path in (ROOT + "/Animations", ROOT + "/Pack"):
        for ad in reg.get_assets_by_path(path, True):
            a = ad.get_asset()
            if isinstance(a, unreal.AnimSequence):
                lib.set_anim_root_lock(a, True)
    for d in ("/Meshes", "/Characters", "/Animations", "/Pack"):
        bc.eal.save_directory(ROOT + d, only_if_is_dirty=True, recursive=True)
    log("level: assetlar tayyor %.0fs" % (time.time() - T0))


# ============================================================ obyektlar
def build_objects():
    groups = collections.defaultdict(lambda: collections.defaultdict(list))
    for o in scene["objects"]:
        key = (o["mesh"], tuple(o["mats"]), o["shadow"])
        groups[o["group"]][key].append(T(o["pos"], o["rot"], o["scale"]))
    total = 0
    for g, items in sorted(groups.items()):
        actor = spawn(unreal.BozorMeshGroup, label=g.replace("/", "_"), folder="Bozor")
        collide = g not in NO_COLLISION_GROUPS
        for (mid, mats, shadow), transforms in items.items():
            sm = mesh(mid)
            if not sm:
                log("level: mesh yo'q", mid)
                continue
            actor.add_instances(sm, [mat(m) for m in mats], transforms, shadow, collide)
            total += len(transforms)
    log("level: obyektlar", total, "%.0fs" % (time.time() - T0))


# ============================================================ yorug'lik va muhit
def build_environment():
    sun = next((l for l in scene["lights"] if l["type"] == "Directional"), None)
    rot = unreal.Rotator(-38, 150, 0)
    if sun:
        x, y, z, w = sun["rot"]
        # Unity nur yo'nalishi = rot * (0, 0, 1)
        fx, fy, fz = 2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)
        rot = unreal.Vector(fz, fx, fy).rotator()
    d = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), rot, "Quyosh", "Muhit")
    lc = d.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property("intensity", 9.0)
    lc.set_editor_property("light_color", unreal.Color(229, 244, 255, 255))
    lc.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, label="Osmon", folder="Muhit")
    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 500), label="Osmon_Yorugligi", folder="Muhit")
    sc = sky.get_component_by_class(unreal.SkyLightComponent)
    sc.set_editor_property("real_time_capture", True)
    fog = spawn(unreal.ExponentialHeightFog, label="Tuman", folder="Muhit")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.008)
    fc.set_editor_property("fog_height_falloff", 0.1)
    spawn(unreal.VolumetricCloud, label="Bulutlar", folder="Muhit")
    pp = spawn(unreal.PostProcessVolume, label="PostProcess", folder="Muhit")
    pp.set_editor_property("unbound", True)
    s = pp.get_editor_property("settings")
    # ekspozitsiya oralig'i (EV100): ochiq joyda ~-2.1; soyabon ostida -3.7 gacha tushadi. Oraliq tor bo'lmasa, soyadan qaraganda
    # quyoshdagi odamlar tonemapper yelkasiga tushib oqaradi (oq teri, och-ko'k ro'mol), shuning uchun deyarli qat'iy ekspozitsiya
    for k, v in (("auto_exposure_bias", 0.0), ("auto_exposure_min_brightness", -2.2), ("auto_exposure_max_brightness", -1.5),
                 ("bloom_intensity", 0.35), ("vignette_intensity", 0.18), ("white_temp", 6200.0)):
        s.set_editor_property("override_" + k, True)
        s.set_editor_property(k, v)
    pp.set_editor_property("settings", s)
    p = scene.get("player", {"pos": [0, 0.05, -7.5]})
    spawn(unreal.PlayerStart, vadd(V(p["pos"]), unreal.Vector(0, 0, 95)), unreal.Rotator(0, 0, 0), "OyinchiStart", "Muhit")
    log("level: muhit tayyor")


def build_ambience():
    # zonali fon (bozor_fon.py): qatlamlar import_audio.py bilan import qilingan, MetaSound bo'lmasa shu yerda quriladi
    amb = scene.get("ambience")
    if not amb:
        return
    try:
        ms = load(ROOT + "/Audio/" + bozor_fon.MS_NAME) or bozor_fon.build_metasound(log)
    except bozor_fon.FonError as e:
        log("level: fon ovozi yo'q -", e)
        return
    bozor_fon.place(world(), log, ms, amb)
    bozor_fon.place_zonalar(world(), log)
    log("level: fon ovozi")


# ============================================================ NavMesh
def build_nav():
    v = spawn(unreal.NavMeshBoundsVolume, V([0, 1.5, 21]), label="NavMesh_Chegara", folder="NavMesh")
    lib.make_box_brush(v, unreal.Vector(5200, 3600, 400))
    for i, b in enumerate(scene["navBlockers"]):
        a = spawn(unreal.BozorNavBlocker, V(b["pos"]), Q(b["rot"]).rotator(), "Sotuvchi_Zona_%02d" % i, "NavMesh")
        a.set_editor_property("extent", unreal.Vector(b["size"][2] * 50, b["size"][0] * 50, b["size"][1] * 50))
    log("level: nav chegara + %d zona" % len(scene["navBlockers"]))


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def set_default_map():
    path = r"D:\Unreal Projects\BozorUnreal\Config\DefaultEngine.ini"
    text = open(path, encoding="utf-8").read()
    for key in ("EditorStartupMap", "GameDefaultMap"):
        text = re.sub(r"^%s=.*$" % key, "%s=%s.Bozor" % (key, MAP), text, flags=re.M)
    open(path, "w", encoding="utf-8").write(text)


# ============================================================
if os.environ.get("BOZOR_SKIP_PREPARE") != "1":
    prepare_assets()
if bc.eal.does_asset_exist(MAP):
    bc.eal.delete_asset(MAP)
levels.new_level(MAP, False)
build_objects()
build_environment()
build_ambience()
build_nav()
levels.save_current_level()
log("level: navigatsiya build", lib.build_navigation(world()), "%.0fs" % (time.time() - T0))
bozor_npc.build(scene, log)
bozor_ovoz.place(world(), log, scene)
bozor_ovoz.place_rasta(world(), log, scene)
bozor_ovoz.place_qadam(world(), log)
bozor_ovoz.place_pavilyonlar(world(), log)
bozor_gosht.apply(world(), log)
# NPC'lardan keyin ham NavMesh qayta quriladi: oldin xarita bo'sh NavMesh bilan saqlanib qolgan, o'yinda hech kim yurmagan
log("level: navigatsiya build (NPC'dan keyin)", lib.build_navigation(world()))
levels.save_current_level()
set_default_map()
log("level: TAYYOR %.0fs" % (time.time() - T0))
