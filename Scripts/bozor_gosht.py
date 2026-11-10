"""Go'sht pavilyoni: kirish ochiladi, Fab (Quixel Megascans) go'shtlari peshtaxta va ilgaklarga, qassoblar va ularning navbatlari.

Unity sahnasida (scene.json) pavilyon bo'sh: sotuvchi yo'q, kirish oldida meva sotuvchisi va uning NavMesh zonasi turibdi.
Shuning uchun xarita qurilgandan keyin shu yerda tuzatiladi: build_level.py (oxirida) yoki muharrirda gosht_yangila.py.
Qayta ishga tushirsa bo'ladi: oldingi go'sht aktorlari o'chirilib, qaytadan qo'yiladi.
"""
import math
import os

import unreal

import bozor_common as bc
import bozor_npc

ROOT = bc.ROOT
DEST = ROOT + "/Gosht"
FAB = r"C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\FabLibrary"
FOLDER = "Gosht"
STALL0 = 900             # qassob navbatlarining Stall raqamlari (Unity rastalari bilan to'qnashmasin)

# Fab kesh papkasi -> (asset nomi, masshtab). Megascans o'lchami haqiqiy (qovurilgan, 15-20 sm), bozor peshtaxtasida
# xom go'sht bo'laklaridek ko'rinishi uchun qo'y qovurg'alari va dudlangan go'sht kattalashtirilgan
MEATS = {
    "Banquet_Lamb_Rack_of_Ribs-166a3f54": ("SM_Gosht_Qovurga_Toplam", 1.8),
    "Banquet_Lamb_Ribs-dc2b6a23": ("SM_Gosht_Qovurga", 1.8),
    "Banquet_Roasted_Quail-6fb34001": ("SM_Gosht_Bedana", 1.0),
    "Smoked_Meat-bd012294": ("SM_Gosht_Dudlangan", 2.0),
}

# o'yindagi joylar (Unreal, sm). Pavilyon: X 3367..4432, Y 788..1632; kirish - X 3780..3980 (ikki ustun orasi), Y 788
COUNTER_Y_OLD = 1485     # SM_Gosht_Rastasi_2m pivoti (old tomoni -Y): devorga taqalgan, qassobga joy yo'q
COUNTER_Y = 1405         # 80 sm oldinga: orqasida 1 m (qassob turadigan joy)
COUNTER_TOP = 88.0       # peshtaxta ustki yuzasi (trace)
HOOK_Z = 212.0           # ilgak tayog'i balandligi (trace), peshtaxta orqa chetida
BUTCHERS = (             # (xarakter, ayolmi, walkStyle)
    ("Sotuvchi_Jilet_01", False, 0),
    ("Otaxon_Pidjak_01", False, 2),
    ("Sotuvchi_Sport_01", False, 0),
    ("Erkak_Kurtka_01", False, 0),
)

# kirishni to'sgan meva sotuvchisi (Unity Sotuvchi_36): eshik qarshisidan shimoliy meva to'dasi ortiga ko'chadi
FRUIT_VENDOR = "Sotuvchi_36"
FRUIT_DX = 459.0
FRUIT_FROM = unreal.Vector(3841, 701, 0)
FRUIT_QUEUES = ([unreal.Vector(4225, 448, 2.5)], [unreal.Vector(4370, 445, 2.5)])
FRUIT_PROPS = ("SM_SM_Plastik_Stul_LOD0", "SM_SM_Yashik_Plastik_Yashil_LOD0", "SM_SM_Tarozi_Elektron_LOD0")

# pavilyon ichidagi yurish nuqtalari: yo'lovchilar ham kirib-chiqib yuradi
WANDER = (unreal.Vector(3880, 980, 2.5), unreal.Vector(4050, 1250, 2.5), unreal.Vector(3600, 1300, 2.5))


def load(path):
    return unreal.load_asset(path) if bc.eal.does_asset_exist(path) else None


def _files(key):
    d = os.path.join(FAB, key, "fbx", "mid")
    for sub in os.listdir(d):
        p = os.path.join(d, sub)
        if os.path.isdir(p):
            fs = os.listdir(p)
            fbx = next(f for f in fs if f.lower().endswith(".fbx"))
            maps = {f.rsplit("_", 1)[1].split(".")[0]: os.path.join(p, f) for f in fs if f.lower().endswith(".jpg")}
            return os.path.join(p, fbx), maps
    raise FileNotFoundError(d)


def _texture(file, name, normal=False, linear=False):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file)
    task.set_editor_property("destination_path", DEST + "/Textures")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    bc.asset_tools.import_asset_tasks([task])
    t = load("%s/Textures/%s" % (DEST, name))
    if normal:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
        t.set_editor_property("flip_green_channel", True)     # Megascans FBX: OpenGL normal -> Unreal (DirectX)
    elif linear:
        t.set_editor_property("srgb", False)
    return t


def import_meats(log):
    """Fab keshidagi FBX + 2K teksturalar -> /Game/Bozor/Gosht (M_BozorLit asosidagi MI bilan)."""
    master = load(ROOT + "/Materials/M_BozorLit")
    mel = unreal.MaterialEditingLibrary
    pipeline = bc.make_pipeline("PL_Gosht", static=True)
    out = {}
    for key, (name, _) in MEATS.items():
        sm = load("%s/%s" % (DEST, name))
        if not sm:
            fbx, maps = _files(key)
            got = bc.interchange_import(fbx, DEST + "/Tmp_" + name, pipeline)
            src = next((a for a in got if isinstance(a, unreal.StaticMesh)), None)
            if not src:
                log("gosht: import bo'lmadi", fbx)
                continue
            bc.eal.rename_asset(src.get_path_name(), "%s/%s" % (DEST, name))
            bc.eal.delete_directory(DEST + "/Tmp_" + name)
            sm = load("%s/%s" % (DEST, name))
            base = name.replace("SM_", "T_")
            mi = bc.asset_tools.create_asset(name.replace("SM_", "MI_"), DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(mi, master)
            mel.set_material_instance_texture_parameter_value(mi, "BaseMap", _texture(maps["BaseColor"], base + "_BaseColor"))
            mel.set_material_instance_texture_parameter_value(mi, "NormalMap", _texture(maps["Normal"], base + "_Normal", normal=True))
            mel.set_material_instance_texture_parameter_value(mi, "OcclusionMap", _texture(maps["AO"], base + "_AO", linear=True))
            mel.set_material_instance_scalar_parameter_value(mi, "Smoothness", 0.55)     # nam go'sht: biroz yaltiroq
            mel.update_material_instance(mi)
            for i in range(len(sm.static_materials)):
                sm.set_material(i, mi)
            bc.eal.save_directory(DEST, only_if_is_dirty=True, recursive=True)
        out[name] = sm
    log("gosht: assetlar", sorted(out))
    return out


def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _group(world, label):
    return next((a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorMeshGroup) if a.get_actor_label() == label), None)


def _move_instances(group, mesh_name, match, fn):
    """match(loc) mos kelgan nusxalarni fn(transform) -> transform bilan siljitadi; nechtasi siljiganini qaytaradi."""
    n = 0
    for c in group.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if c.get_editor_property("static_mesh").get_name() != mesh_name:
            continue
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            if match(t.translation):
                c.update_instance_transform(i, fn(t), True, True, True)
                n += 1
    return n


def _near(p, q, r=8.0):
    return abs(p.x - q.x) < r and abs(p.y - q.y) < r


def open_entrance(world, log, mgr):
    """Meva sotuvchisini (u, stul, tarozi, yashik, NavMesh zonasi, navbatlari) eshik qarshisidan shimolga ko'chiradi."""
    vendor = next((a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorNpc) if a.get_actor_label() == FRUIT_VENDOR), None)
    if not vendor or not _near(vendor.get_actor_location(), FRUIT_FROM, 30):
        log("gosht: meva sotuvchisi allaqachon ko'chirilgan yoki yo'q")
        return
    shift = unreal.Vector(FRUIT_DX, 0, 0)
    vendor.set_actor_location(vendor.get_actor_location() + shift, False, False)
    props = _group(world, "Bozor_Jihozlar")
    n = 0
    for mesh_name in FRUIT_PROPS:
        def move(t):
            return unreal.Transform(t.translation + shift, t.rotation.rotator(), t.scale3d)
        n += _move_instances(props, mesh_name, lambda p: p.x > 3780 and p.x < 3900 and p.y > 480 and p.y < 680, move)
    for z in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorNavBlocker):
        if _near(z.get_actor_location(), unreal.Vector(3830, 655, 0), 30):
            z.set_actor_location(z.get_actor_location() + shift, False, False)
    queues = list(mgr.get_editor_property("queues"))
    k = 0
    for q in queues:
        if q.vendor == vendor and k < len(FRUIT_QUEUES):
            q.points = FRUIT_QUEUES[k]
            k += 1
    mgr.set_editor_property("queues", queues)
    ovoz = next(iter(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorRastaOvozlari)), None)
    if ovoz:
        pts = list(ovoz.get_editor_property("points"))
        for p in pts:
            if _near(p.location, unreal.Vector(3841, 651, 0), 30):
                p.location = p.location + shift
        ovoz.set_editor_property("points", pts)
    log("gosht: kirish ochildi, meva sotuvchisi ko'chdi (rekvizit %d, navbat %d)" % (n, k))


def counters(world, log):
    """Go'sht rastalarini 80 sm oldinga suradi (orqasida qassob turadi); har rastaning X boshlanishini qaytaradi."""
    g = _group(world, "Bozor_Rastalar")

    def fwd(t):
        p = t.translation
        return unreal.Transform(unreal.Vector(p.x, COUNTER_Y, p.z), t.rotation.rotator(), t.scale3d)
    n = _move_instances(g, "SM_SM_Gosht_Rastasi_2m_LOD0", lambda p: abs(p.y - COUNTER_Y_OLD) < 5, fwd)
    xs = []
    for c in g.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if c.get_editor_property("static_mesh").get_name() == "SM_SM_Gosht_Rastasi_2m_LOD0":
            for i in range(c.get_instance_count()):
                p = c.get_instance_transform(i, True).translation
                if abs(p.y - COUNTER_Y) < 5:
                    xs.append(p.x - 203.0)     # pivot - rastaning +X uchi, mesh -X tomonga 2 m
    log("gosht: rastalar oldinga surildi", n, "rasta", len(xs))
    return sorted(xs)


def _place_on(sm, scale, base, yaw, top=True, hang=False):
    """Mesh pastki yuzasi base.z ga tushadi (top) yoki yuqori uchi base.z dan osiladi (hang); markazi base.xy da."""
    b = sm.get_bounding_box()
    rot = unreal.Rotator(0, 0, yaw)
    if hang:
        # uzun tomoni pastga: ilgakdan osilib turadi (Megascans o'qlari assetga qarab har xil)
        size = b.max - b.min
        if size.x >= size.y and size.x >= size.z:
            rot = unreal.Rotator(0, 90, yaw)
        elif size.y >= size.z:
            rot = unreal.Rotator(90, 0, yaw)
    t = unreal.Transform(unreal.Vector(0, 0, 0), rot, unreal.Vector(scale, scale, scale))
    pts = [t.transform_location(unreal.Vector(x, y, z)) for x in (b.min.x, b.max.x) for y in (b.min.y, b.max.y) for z in (b.min.z, b.max.z)]
    cx = (min(p.x for p in pts) + max(p.x for p in pts)) / 2
    cy = (min(p.y for p in pts) + max(p.y for p in pts)) / 2
    z = base.z - (max(p.z for p in pts) if hang else min(p.z for p in pts))
    return unreal.Transform(unreal.Vector(base.x - cx, base.y - cy, z), rot, unreal.Vector(scale, scale, scale))


def place_meat(world, log, meshes, xs):
    old = _group(world, "Bozor_Gosht")
    if old:
        _actors().destroy_actor(old)
    g = _actors().spawn_actor_from_class(unreal.BozorMeshGroup, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    g.set_actor_label("Bozor_Gosht")
    g.set_folder_path("Bozor")
    scale = {name: s for name, s in MEATS.values()}
    items = {name: [] for name in meshes}
    yb = COUNTER_Y + 6           # peshtaxtaning old cheti (pivot Y + 6 .. + 86)
    for k, x0 in enumerate(xs):
        # ilgaklarda: tayoq X x0+14..x0+134, peshtaxtaning orqa chetida
        hang = ("SM_Gosht_Qovurga_Toplam", "SM_Gosht_Qovurga", "SM_Gosht_Qovurga_Toplam", "SM_Gosht_Dudlangan")
        for j, x in enumerate((x0 + 30, x0 + 60, x0 + 92, x0 + 122)):
            name = hang[(j + k) % len(hang)]
            if name in meshes and (j + k) % 4 != 3:
                items[name].append(_place_on(meshes[name], scale[name], unreal.Vector(x, COUNTER_Y + 85, HOOK_Z - 6), 90 + 8 * (j - 1.5), hang=True))
        # peshtaxta ustida (kunda X x0+34..x0+74 dan o'ngda): dudlangan go'sht, qovurg'alar, bedanalar
        row = (("SM_Gosht_Dudlangan", x0 + 105, yb + 28, 20), ("SM_Gosht_Dudlangan", x0 + 128, yb + 52, -35),
               ("SM_Gosht_Qovurga", x0 + 160, yb + 25, 85), ("SM_Gosht_Qovurga", x0 + 168, yb + 45, 95),
               ("SM_Gosht_Bedana", x0 + 188, yb + 22, 10), ("SM_Gosht_Bedana", x0 + 186, yb + 50, -20),
               ("SM_Gosht_Qovurga_Toplam", x0 + 140, yb + 20, 180 + 15 * k))
        for name, x, y, yaw in row:
            if name in meshes:
                items[name].append(_place_on(meshes[name], scale[name], unreal.Vector(x, y, COUNTER_TOP), yaw + 25 * k))
    n = 0
    for name, ts in items.items():
        if ts:
            g.add_instances(meshes[name], [], ts, True, False)
            n += len(ts)
    log("gosht: go'sht bo'laklari", n)


def butchers(world, log, mgr, xs):
    """Har go'sht rastasi ortida qassob (ABozorNpc, Sotuvchi), oldida xaridor navbati; pavilyonga yurish nuqtalari."""
    actors = _actors()
    for a in actors.get_all_level_actors():
        if str(a.get_folder_path()).startswith(FOLDER) or str(a.get_folder_path()).startswith("NPC/" + FOLDER):
            actors.destroy_actor(a)
    abp = load(bozor_npc.ABP).generated_class()
    queues = [q for q in mgr.get_editor_property("queues") if q.stall < STALL0 and q.vendor]
    yaw = -90.0                  # yuzi -Y (kirish va xaridorlar tomonga)
    for k, x0 in enumerate(xs):
        char, female, style = BUTCHERS[k % len(BUTCHERS)]
        cx = x0 + 100
        a = actors.spawn_actor_from_class(unreal.BozorNpc, unreal.Vector(cx + 20, COUNTER_Y + 145, bozor_npc.CAPSULE_HALF + 2), unreal.Rotator(0, 0, yaw))
        a.set_actor_label("Qassob_%02d" % (k + 1))
        a.set_folder_path("NPC/" + FOLDER)
        m = a.get_editor_property("mesh")
        m.set_editor_property("skeletal_mesh_asset", load("%s/Characters/%s" % (ROOT, char)))
        m.set_editor_property("anim_class", abp)
        m.set_editor_property("enable_update_rate_optimizations", True)
        a.set_editor_property("npc_role", unreal.BozorRole.SOTUVCHI)
        a.set_editor_property("female", female)
        a.set_editor_property("walk_style", style)
        a.set_editor_property("walk_speed", 85.0)
        a.set_editor_property("order", 200 + k)
        # orqadagi tor yo'lakka xaridor kirmasin (Unity sotuvchi zonasi kabi)
        z = actors.spawn_actor_from_class(unreal.BozorNavBlocker, unreal.Vector(cx, COUNTER_Y + 145, 100), unreal.Rotator(0, 0, 0))
        z.set_actor_label("Qassob_Zona_%02d" % (k + 1))
        z.set_folder_path(FOLDER)
        z.set_editor_property("extent", unreal.Vector(100, 55, 150))
        unreal.BozorLevelTools.rerun_construction(z)
        # navbat: 0-nuqta peshtaxta oldida, keyingilari orqasida (yo'lak bo'ylab)
        pts = [unreal.Vector(cx, COUNTER_Y - 60, 2.5), unreal.Vector(cx - 45, COUNTER_Y - 125, 2.5), unreal.Vector(cx + 40, COUNTER_Y - 165, 2.5)]
        queues.append(unreal.BozorQueueSpot(points=pts, forward=unreal.Vector(0, 1, 0), vendor=a, stall=STALL0 + k,
                                            start_only=True, can_leave_randomly=True, patience=unreal.Vector2D(20, 45)))
    mgr.set_editor_property("queues", queues)
    wander = list(mgr.get_editor_property("wander_points"))
    for p in WANDER:
        if not any(_near(p, q, 5) for q in wander):
            wander.append(p)
    mgr.set_editor_property("wander_points", wander)
    # qassob kassasi ovozi (bozor_ovoz.place_rasta sotuvchilar uchun qo'yadigandek)
    import bozor_ovoz
    ovoz = next(iter(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorRastaOvozlari)), None)
    if ovoz:
        pts = [p for p in ovoz.get_editor_property("points") if not (p.location.y > COUNTER_Y + 60 and p.location.x > 3400 and p.location.x < 4350 and p.location.y < 1620)]
        for x0 in xs:
            pts.append(unreal.BozorRastaNuqta(location=unreal.Vector(x0 + 120, COUNTER_Y + 95, 190), kinds=bozor_ovoz.VENDOR_KINDS,
                                              gap=unreal.Vector2D(bozor_ovoz.VENDOR_GAP[0], bozor_ovoz.VENDOR_GAP[1])))
        ovoz.set_editor_property("points", pts)
    log("gosht: qassoblar", len(xs), "navbatlar jami", len(queues))


def apply(world, log):
    mgr = next(iter(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorNpcManager)), None)
    if not mgr:
        log("gosht: Bozor_NPC_Menejeri yo'q - avval NPC'lar")
        return
    meshes = import_meats(log)
    open_entrance(world, log, mgr)
    xs = counters(world, log)
    place_meat(world, log, meshes, xs)
    butchers(world, log, mgr, xs)
