"""Bino tagini Toshkent mavzesiga o'xshatish (Unreal Python, muharrirda).

Xulosa: D:/Trk/BozorKit_hammasi/toshkent_bino_tagi.md
 - devor bo'ylab beton otmostka (trace bilan devor chizig'i topiladi, 1 m bo'laklar = choklar);
 - tekis yashil maysa o'rniga chang lyoss tuproq + siyrak o't (M_Toshkent_BinoTagi, Poly Haven dirt CC0);
 - sarg'ish o't yamoqlari, o't chiziqlari va gortenziya gulzorlari olib tashlanadi,
   4 ta gulzor bordyur ramkali palisadnik bo'lib qoladi;
 - devor va bordyur chetida mayda quruq o't tutamlari;
 - daraxt tanalari 1.1 m gacha ohaklangan.
Ishlatish: exec(open(path).read()); main()   (saqlash: main(saqla=True))
"""
import math
import random
from pathlib import Path
import shutil
import stat
import time

import unreal as ue


V = ue.Vector
ROOT = "/Game/Bozor/Toshkent/Yer"
FOLDER = "Toshkent_Kochalari"
GURUH = "Toshkent_Bino_Tagi"
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
ASSETS = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
NODES = ue.MaterialEditingLibrary
SAVED = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir()))
SOURCE = SAVED / "ToshkentSource"

def asset(path):
    result = ue.load_asset(path)
    if not result:
        raise RuntimeError(f"Asset topilmadi: {path}")
    return result

def import_file(file, folder, name):
    task = ue.AssetImportTask()
    task.filename = str(file)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.save = True
    TOOLS.import_asset_tasks([task])
    return list(task.imported_object_paths)

def group(name):
    actor = ACTORS.spawn_actor_from_class(ue.BozorMeshGroup, V(), ue.Rotator())
    actor.set_actor_label(name)
    actor.set_folder_path(FOLDER)
    return actor

def xy_bounds(component, transform):
    b = component.static_mesh.get_bounding_box()
    corners = [transform.transform_location(V(x,y,z)) for x in (b.min.x,b.max.x)
               for y in (b.min.y,b.max.y) for z in (b.min.z,b.max.z)]
    return min(p.x for p in corners), max(p.x for p in corners), min(p.y for p in corners), max(p.y for p in corners)


# Binolar: (nom, xmin, xmax, ymin, ymax) - taxminiy to'rtburchak, devor trace bilan aniqlanadi
BINOLAR_FON = [
    ("shimol", 6650, 7990, -2300, 3100),
    ("janub", -4000, -1950, -4900, 3700),
]
OTMOSTKA_KENG = 85.0     # devordan tashqariga, sm
OTMOSTKA_TEPA = 4.0      # yer (0) dan balandligi, sm
QADAM = 100.0            # bo'lak uzunligi (chok oralig'i)


# ---------------------------------------------------------------- materiallar
def _tex(name, source, channel):
    path = f"{ROOT}/T_{name}_{channel}"
    if not ASSETS.does_asset_exist(path):
        file, = SOURCE.glob(f"{source}_{channel}.*")
        import_file(file, ROOT, f"T_{name}_{channel}")
    image = asset(path)
    if channel != "diff":
        image.set_editor_property("srgb", False)
    if channel == "nor_dx":
        image.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
    ASSETS.save_loaded_asset(image)
    return image


def _graph(mat):
    def node(kind, x, y, **props):
        n = NODES.create_material_expression(mat, kind, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def connect(a, b, pin, output=""):
        if not NODES.connect_material_expressions(a, output, b, pin):
            raise RuntimeError(f"{mat.get_name()}: pin ulanmadi {pin}")
    return node, connect


def bino_tagi_material(qayta=False):
    path = f"{ROOT}/M_Toshkent_BinoTagi"
    dirt = [_tex("Toshkent_Tuproq", "dirt", c) for c in ("diff", "nor_dx", "rough")]
    grass = [asset(f"{ROOT}/T_Toshkent_Maysa_{c}") for c in ("diff", "nor_dx", "rough")]
    if ASSETS.does_asset_exist(path):
        if not qayta:
            return asset(path)
        mat = asset(path)                      # havolalar saqlanadi, grafik qayta quriladi
        NODES.delete_all_material_expressions(mat)
    else:
        mat = TOOLS.create_asset("M_Toshkent_BinoTagi", ROOT, ue.Material, ue.MaterialFactoryNew())
    node, connect = _graph(mat)
    S = ue.MaterialSamplerType
    pos = node(ue.MaterialExpressionWorldPosition, -1500, 0)
    uv_d = node(ue.MaterialExpressionDivide, -1250, -300, const_b=230.0)
    uv_g = node(ue.MaterialExpressionDivide, -1250, 300, const_b=170.0)
    connect(pos, uv_d, "A", "XY")
    connect(pos, uv_g, "A", "XY")
    samples = {}
    for i, (img, st) in enumerate(zip(dirt, (S.SAMPLERTYPE_COLOR, S.SAMPLERTYPE_NORMAL, S.SAMPLERTYPE_LINEAR_COLOR))):
        s = node(ue.MaterialExpressionTextureSample, -1000, -600 + i*220, texture=img, sampler_type=st)
        connect(uv_d, s, "UVs")
        samples[("d", i)] = s
    for i, (img, st) in enumerate(zip(grass, (S.SAMPLERTYPE_COLOR, S.SAMPLERTYPE_NORMAL, S.SAMPLERTYPE_LINEAR_COLOR))):
        s = node(ue.MaterialExpressionTextureSample, -1000, 100 + i*220, texture=img, sampler_type=st)
        connect(uv_g, s, "UVs")
        samples[("g", i)] = s
    # Lyoss: och bej-kulrang chang tuproq
    d_des = node(ue.MaterialExpressionDesaturation, -700, -650)
    d_amt = node(ue.MaterialExpressionConstant, -900, -700, r=0.6)
    connect(samples[("d", 0)], d_des, "", "RGB")
    connect(d_amt, d_des, "Fraction")
    d_col = node(ue.MaterialExpressionMultiply, -500, -650)
    d_tint = node(ue.MaterialExpressionConstant3Vector, -700, -760, constant=ue.LinearColor(1.30, 1.24, 1.10, 1))
    connect(d_des, d_col, "A")
    connect(d_tint, d_col, "B")
    # Quruq, sarg'aygan siyrak o't
    g_col = node(ue.MaterialExpressionMultiply, -500, 100)
    g_tint = node(ue.MaterialExpressionConstant3Vector, -700, 0, constant=ue.LinearColor(0.85, 1.0, 0.68, 1))
    connect(samples[("g", 0)], g_col, "A", "RGB")
    connect(g_tint, g_col, "B")
    # Yamoq niqobi: past chastotali shovqin (3-6 m dog'lar), yumshoq chegara. Mayda shovqin
    # qo'shilsa "yorilgan loy" naqshi chiqadi - shuning uchun faqat bitta qatlam.
    noise = node(ue.MaterialExpressionNoise, -1000, 800, scale=0.0035, levels=4, output_min=0.0, output_max=1.0,
                 quality=1)
    connect(pos, noise, "")
    sub = node(ue.MaterialExpressionSubtract, -750, 800, const_b=0.42)
    mul = node(ue.MaterialExpressionMultiply, -600, 800, const_b=2.2)
    mask = node(ue.MaterialExpressionSaturate, -450, 800)
    connect(noise, sub, "A")
    connect(sub, mul, "A")
    connect(mul, mask, "")
    outs = []
    for i, (a, b) in enumerate(((d_col, g_col), (samples[("d", 1)], samples[("g", 1)]),
                                 (samples[("d", 2)], samples[("g", 2)]))):
        lerp = node(ue.MaterialExpressionLinearInterpolate, 0, -400 + i*300)
        connect(a, lerp, "A", "" if i == 0 else "RGB")
        connect(b, lerp, "B", "" if i == 0 else "RGB")
        connect(mask, lerp, "Alpha")
        outs.append(lerp)
    rough = node(ue.MaterialExpressionMultiply, 200, 200, const_b=1.0)
    connect(outs[2], rough, "A")
    rough_r = node(ue.MaterialExpressionComponentMask, 350, 200, r=True, g=False, b=False, a=False)
    connect(rough, rough_r, "")
    assert NODES.connect_material_property(outs[0], "", ue.MaterialProperty.MP_BASE_COLOR)
    assert NODES.connect_material_property(outs[1], "", ue.MaterialProperty.MP_NORMAL)
    assert NODES.connect_material_property(rough_r, "", ue.MaterialProperty.MP_ROUGHNESS)
    NODES.recompile_material(mat)
    ASSETS.save_loaded_asset(mat)
    return mat


def ohak_material():
    """Ohak (oq) - daraxt tanasi uchun, chang bilan biroz xira."""
    path = f"{ROOT}/M_Toshkent_Ohak"
    if ASSETS.does_asset_exist(path):
        return asset(path)
    dirt = _tex("Toshkent_Tuproq", "dirt", "diff")
    mat = TOOLS.create_asset("M_Toshkent_Ohak", ROOT, ue.Material, ue.MaterialFactoryNew())
    node, connect = _graph(mat)
    s = node(ue.MaterialExpressionTextureSample, -700, 0, texture=dirt)
    tc = node(ue.MaterialExpressionTextureCoordinate, -900, 0, u_tiling=0.6, v_tiling=0.6)
    connect(tc, s, "UVs")
    des = node(ue.MaterialExpressionDesaturation, -450, 0)
    one = node(ue.MaterialExpressionConstant, -650, 150, r=1.0)
    connect(s, des, "", "RGB")
    connect(one, des, "Fraction")
    lerp = node(ue.MaterialExpressionLinearInterpolate, -200, 0)
    a = node(ue.MaterialExpressionConstant3Vector, -450, -150, constant=ue.LinearColor(0.80, 0.79, 0.75, 1))
    b = node(ue.MaterialExpressionConstant3Vector, -450, -250, constant=ue.LinearColor(0.55, 0.53, 0.48, 1))
    connect(b, lerp, "A")
    connect(a, lerp, "B")
    p = node(ue.MaterialExpressionMultiply, -300, 100, const_b=3.0)
    sat = node(ue.MaterialExpressionSaturate, -250, 200)
    connect(des, p, "A")
    connect(p, sat, "")
    connect(sat, lerp, "Alpha")
    r = node(ue.MaterialExpressionConstant, -200, 250, r=0.93)
    assert NODES.connect_material_property(lerp, "", ue.MaterialProperty.MP_BASE_COLOR)
    assert NODES.connect_material_property(r, "", ue.MaterialProperty.MP_ROUGHNESS)
    NODES.recompile_material(mat)
    ASSETS.save_loaded_asset(mat)
    return mat


# ---------------------------------------------------------------- yordamchi
def _world():
    return ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()


def _by_label():
    return {str(a.get_actor_label()): a for a in ACTORS.get_all_level_actors()}


def _ism(actor, mesh_name):
    return [c for c in actor.get_components_by_class(ue.InstancedStaticMeshComponent)
            if c.static_mesh and c.static_mesh.get_name() == mesh_name]


def _trace(world, a, b, ignore):
    h = ue.SystemLibrary.line_trace_single(world, a, b, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                           ue.DrawDebugTrace.NONE, True)
    if not h:
        return None
    t = h.to_tuple()
    return t[4], t[7], t[9], t[10]   # impact point, impact normal, actor, component


def _bino_hit(hit, binolar):
    if not hit:
        return False
    _, n, actor, comp = hit
    if actor is None or str(actor.get_actor_label()) not in binolar:
        return False
    sm = getattr(comp, "static_mesh", None)
    if sm and "Daraxt" in sm.get_name():
        return False
    return abs(n.z) < 0.3


# ---------------------------------------------------------------- otmostka
def otmostka_joylari(world, ignore):
    """Har bir bino tomoni bo'ylab 1 m qadam bilan devor nuqtasi va normalini topadi."""
    binolar = {"Bozor_Fon", "Toshkent_Turar_Joy_va_Xizmat"}
    rects = list(BINOLAR_FON)
    joy = _by_label().get("Toshkent_Turar_Joy_va_Xizmat")
    components = joy.get_components_by_class(ue.InstancedStaticMeshComponent) if joy else []
    for c in components:
        for i in range(c.get_instance_count()):
            x0, x1, y0, y1 = xy_bounds(c, c.get_instance_transform(i, True))
            rects.append((c.static_mesh.get_name() + str(i), x0, x1, y0, y1))
    segs = []
    for name, x0, x1, y0, y1 in rects:
        m = 450.0
        sides = []
        for t in _arange(y0 + 40, y1 - 40, QADAM):
            sides += [(V(x0 - m, t, 25), V(x0 + 300, t, 25)), (V(x1 + m, t, 25), V(x1 - 300, t, 25))]
        for t in _arange(x0 + 40, x1 - 40, QADAM):
            sides += [(V(t, y0 - m, 25), V(t, y0 + 300, 25)), (V(t, y1 + m, 25), V(t, y1 - 300, 25))]
        for a, b in sides:
            hit = _trace(world, a, b, ignore)
            if _bino_hit(hit, binolar):
                p, n, _, _ = hit
                segs.append((p, V(n.x, n.y, 0).normal()))
    return segs


def _arange(a, b, step):
    n = max(1, int((b - a) // step) + 1)
    return [a + (b - a) * (i + 0.5) / n for i in range(n)]


def otmostka_transformlar(segs):
    out = []
    for p, n in segs:
        yaw = math.degrees(math.atan2(n.y, n.x))
        c = p + n * (OTMOSTKA_KENG / 2 - 12)
        # biroz qiya (devordan tashqariga pastlab) - 2 daraja
        out.append(ue.Transform(V(c.x, c.y, OTMOSTKA_TEPA - 6), ue.Rotator(roll=0, pitch=-1.5, yaw=yaw),
                                V((OTMOSTKA_KENG + 12) / 100, QADAM * 1.03 / 100, 0.12)))
    return out


# ---------------------------------------------------------------- tozalash
OLIB_TASHLA = ("SM_ATR_Ot_Yamoq_1", "SM_ATR_Ot_Yamoq_2", "SM_ATR_Ot_Yamoq_3", "SM_ATR_Ot_Chiziq")
# Gortenziya Toshkent hovlilariga xos emas, bundan tashqari Gulzor meshlarida gullar bbox markazidan
# siljigan (ostidagi tuproq plitasi gullardan chetda qoladi) - hammasi olib tashlanadi.
GULZOR_QOLADI = set()


def tozalash(atrof):
    removed = {}
    for c in atrof.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        keep = None
        if name in OLIB_TASHLA:
            keep = lambda t: False  # noqa: E731
        elif name in ("SM_ATR_Gulzor_1", "SM_ATR_Gulzor_1_Tuproq"):
            keep = lambda t: (round(t.translation.x), round(t.translation.y)) in GULZOR_QOLADI  # noqa: E731
        elif name in ("SM_ATR_Gulzor_2", "SM_ATR_Gulzor_2_Tuproq"):
            keep = lambda t: False  # noqa: E731
        if keep is None:
            continue
        c.modify()
        idx = [i for i in range(c.get_instance_count()) if not keep(c.get_instance_transform(i, True))]
        for i in reversed(idx):
            c.remove_instance(i)
        removed[name] = len(idx)
        if name == "SM_ATR_Gulzor_1_Tuproq":
            # qolgan gulzor ostidagi tuproq gul butalarini to'liq qoplasin: 250 x 400 sm
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                c.update_instance_transform(i, ue.Transform(t.translation + V(0, 0, 0.6), t.rotation.rotator(),
                                                            V(250 / 120, 400 / 300, 1)), True, True, True)
    return removed


# ---------------------------------------------------------------- daraxtlar (ohak)
def ohak_transformlar(world, groups, ignore_all_but):
    """Daraxt tanasi markazi/radiusini trace bilan o'lchab, ohak qatlami (silindr) qo'yadi."""
    cyl = []
    olchov = {}
    for g in groups:
        for c in g.get_components_by_class(ue.InstancedStaticMeshComponent):
            name = c.static_mesh.get_name()
            if not name.startswith("SM_Daraxt_Olxa"):
                continue
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                if abs(t.translation.x) > 9000 or abs(t.translation.y) > 12000:
                    continue
                cyl.append((name, t))
    # Kolliziyali (Bozor_Atrof) daraxtdan har mesh uchun lokal tana o'lchamlari
    atrof = _by_label()["Bozor_Atrof"]
    ignore = [a for a in ACTORS.get_all_level_actors() if str(a.get_actor_label()) != "Bozor_Atrof"]
    for c in atrof.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        if not name.startswith("SM_Daraxt_Olxa") or name in olchov:
            continue
        cpath = c.get_path_name()
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            res = []
            for z in (20.0, 110.0):
                pts = []
                for k in range(8):
                    ang = k * math.pi / 4
                    d = V(math.cos(ang), math.sin(ang), 0)
                    a = t.translation + V(0, 0, z) + d * 150
                    h = _trace(world, a, t.translation + V(0, 0, z) - d * 10, ignore)
                    if h and h[3] and h[3].get_path_name() == cpath:
                        pts.append(h[0])
                if len(pts) < 6:
                    break
                cx = sum(p.x for p in pts) / len(pts)
                cy = sum(p.y for p in pts) / len(pts)
                r = sum(math.hypot(p.x - cx, p.y - cy) for p in pts) / len(pts)
                res.append((cx, cy, r))
            if len(res) == 2 and 3 < res[0][2] < 40:
                s = t.scale3d.x
                inv = t.inverse()
                loc = [inv.transform_location(V(cx, cy, t.translation.z + z)) for (cx, cy, _), z in zip(res, (20.0, 110.0))]
                olchov[name] = (loc, [r / s for _, _, r in res])
                break
    out = []
    for name, t in cyl:
        if name not in olchov:
            continue
        loc, rads = olchov[name]
        a = t.transform_location(loc[0])
        b = t.transform_location(loc[1])
        s = t.scale3d.x
        r = max(rads) * s * 1.06 + 0.6
        d = b - a
        L = d.length()
        if L < 1:
            continue
        # tana bo'yicha 0..115 sm (yerdan) - silindr markazi va yo'nalishi
        dirv = d / L
        base = a - dirv * (a.z - t.translation.z) / max(dirv.z, 0.2)
        h = 115.0
        centre = base + dirv * (h / 2)
        rot = ue.MathLibrary.make_rot_from_z(dirv)
        out.append(ue.Transform(centre, rot, V(r * 2 / 100, r * 2 / 100, h / 100)))
    return out, olchov


# ---------------------------------------------------------------- mayda o't tutamlari
def otlar_transformlar(segs, seed=7):
    rnd = random.Random(seed)
    out = []
    for p, n in segs:
        if rnd.random() > 0.33:
            continue
        side = V(-n.y, n.x, 0)
        d = OTMOSTKA_KENG - 12 + rnd.uniform(5, 45)
        c = p + n * d + side * rnd.uniform(-45, 45)
        s = rnd.uniform(0.16, 0.32)
        out.append(ue.Transform(V(c.x, c.y, -2), ue.Rotator(0, 0, rnd.uniform(0, 360)), V(s, s, s * rnd.uniform(0.7, 1.1))))
    return out


# ---------------------------------------------------------------- palisadnik bordyuri
def palisadnik_bordyur():
    """SM_SM_Bordyur_1m: lokal x -15..0, y -100..0, z 0..30 (burchak pivot). Ramka 280 x 430 sm, 15 sm baland."""
    out = []
    for x, y in GULZOR_QOLADI:
        for px in (x - 125, x + 140):                       # uzun tomonlar (y bo'ylab), yaw 0
            for k in range(4):
                out.append(ue.Transform(V(px, y - 200 + 100 * (k + 1), 0), ue.Rotator(0, 0, 0), V(1, 1, 0.5)))
        for py in (y - 200, y + 215):                       # qisqa tomonlar (x bo'ylab), yaw 90
            for k in range(3):
                out.append(ue.Transform(V(x - 140 + 93.4 * k, py, 0), ue.Rotator(0, 0, 90), V(1, 0.934, 0.5)))
    return out


# ---------------------------------------------------------------- asosiy
MAYSA_BINO_TAGI_EMAS = ("SM_YER_Maysa_70x190", "SM_YER_Maysa_100x14", "SM_YER_Maysa_100x46")


def main(saqla=False):
    world = _world()
    if not world or not world.get_path_name().startswith("/Game/Bozor/Maps/Bozor"):
        raise RuntimeError("Bozor xaritasi ochiq emas")
    labels = _by_label()
    if GURUH in labels:
        raise RuntimeError(f"{GURUH} allaqachon bor - avval o'chiring yoki zaxiradan qaytaring")
    atrof = labels["Bozor_Atrof"]
    yer_mat = bino_tagi_material()
    ohak = ohak_material()
    beton = asset("/Game/Bozor/Materials/MI_M_Beton")
    tuproq_mi = asset("/Game/Bozor/Materials/MI_M_Yer_Tuproq")
    cube = asset("/Engine/BasicShapes/Cube")
    cyl = asset("/Engine/BasicShapes/Cylinder")
    bordyur = asset("/Game/Bozor/Meshes/SM_SM_Bordyur_1m_LOD0")
    otmesh = asset("/Game/Bozor/Meshes/SM_ATR_Ot_Yamoq_1")

    # Diqqat: unreal obyektlari set/hash bilan to'g'ri solishtirilmaydi - label bo'yicha
    ignore = [a for a in ACTORS.get_all_level_actors()
              if str(a.get_actor_label()) not in ("Bozor_Fon", "Toshkent_Turar_Joy_va_Xizmat")]
    segs = otmostka_joylari(world, ignore)

    hisobot = {"otmostka": len(segs)}
    hisobot["olib_tashlandi"] = tozalash(atrof)

    # bino tagi maysalari -> chang tuproq + siyrak o't
    n = 0
    for c in atrof.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        if name.startswith("SM_YER_Maysa_") and name not in MAYSA_BINO_TAGI_EMAS:
            c.modify()
            c.set_material(0, yer_mat)
            n += 1
        elif name == "SM_ATR_Gulzor_1_Tuproq":
            c.modify()
            c.set_material(0, tuproq_mi)
    hisobot["maysa_komponent"] = n

    g = group(GURUH)
    g.add_instances(cube, [beton], otmostka_transformlar(segs), True, False)
    g.add_instances(otmesh, [], otlar_transformlar(segs), False, False)
    g.add_instances(bordyur, [], palisadnik_bordyur(), True, False)
    daraxt_guruhlari = [atrof, labels["Bozor_Fon"]]
    if "Toshkent_Daraxtzor" in labels:
        daraxt_guruhlari.append(labels["Toshkent_Daraxtzor"])
    tr, olchov = ohak_transformlar(world, daraxt_guruhlari, None)
    g.add_instances(cyl, [ohak], tr, True, False)
    hisobot["ohak"] = len(tr)
    hisobot["tana_olchov"] = {k: [round(r, 1) for r in v[1]] for k, v in olchov.items()}
    ue.log(f"BINO_TAGI: {hisobot}")
    if saqla:
        saqlash(world)
    return hisobot


def saqlash(world):
    content = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_content_dir()))
    file = content / "Bozor/Maps/Bozor.umap"
    backup = SAVED / "XaritaZaxira" / f"Bozor_BinoTagiOldi_{time.time_ns()}.umap"
    backup.parent.mkdir(exist_ok=True)
    shutil.copyfile(file, backup)
    file.chmod(file.stat().st_mode | stat.S_IWRITE)
    if not ue.EditorLoadingAndSavingUtils.save_map(world, "/Game/Bozor/Maps/Bozor"):
        raise RuntimeError("Xarita saqlanmadi")
    ue.log(f"BINO_TAGI: saqlandi, zaxira {backup}")
    return str(backup)
