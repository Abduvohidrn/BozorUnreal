"""Bozor yonidagi eski binolarni (Bozor_Fon: 2 ta uzun panelka + 4 ta sarg'ish minora) yangi
LowPolyBuildings uslubidagi panel uylar bilan almashtirish (Unreal Python, muharrirda).

Yangi binolar Gulbozor_5q_07 (5 qavat, 2 kirish, 38 m) modullaridan yig'iladi:
 - uzun (54 m, 3 kirish): o'rtadagi 5 ta modul (312 sm) nusxalanadi;
 - qisqa (18 m, 1 kirish): o'ng tomondagi 7 ta modul olib tashlanib, chekka devor suriladi.
Kirishlar bozor tomonga qaraydi.
Ishlatish: exec(open(path).read()); main()   (saqlash: main(saqla=True))
"""
import math
import shutil
import stat
import time
from pathlib import Path

import unreal as ue

V = ue.Vector
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
SAVED = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir()))
SHABLON = "Gulbozor_5q_07"
FOLDER = "Bozor_Atrofidagi_Binolar/Panel_Uylar/Bozor_Yoni"
MODUL = 312.0
INF = 1e9

# lokal X bo'yicha bo'laklar: (dan, gacha, surish) - instansiya markazi bo'yicha tanlanadi
UZUN = [(-INF, 21, 0), (-1539, 21, 5 * MODUL), (21, INF, 5 * MODUL)]
QISQA = [(-INF, -291, 0), (1840, INF, -7 * MODUL)]

# (nom, turi, markaz x, y, yaw, palisadnik chuqurligi, podyezddan yo'lakkacha yo'lcha uzunligi)
# yaw: lokal -Y = podyezdlar tomoni. Toshkent mikrorayonlaridagidek podyezdlar ichki HOVLIGA
# qaraydi; ko'cha va bozor tomonida faqat derazali orqa fasad, maysa va daraxtlar.
#  - Shimol: hovli +X tomonda (Zarqaynar/Gulbozor uylari bilan umumiy hovli, plitkali yo'lak)
#  - Janub: hovli -X tomonda (hovli ichi asfalt proezdi), +X - avtobus ko'chasi
#  - Minoralar: bozor darvozasi va bozor tomoni jamoat joyi - kirish tashqi ko'cha
#    (y = ±44 m dagi yo'l) tomonidagi trotuar/yo'lakka, 3-minorada hovli yo'lagiga
BINOLAR = [
    ("Bozor_Yoni_5q_Shimol", UZUN, 7403, 398, 90, 150, 50),
    ("Bozor_Yoni_5q_Janub", UZUN, -3204, -1397, -90, 120, 0),
    ("Bozor_Yoni_5q_Minora_1", QISQA, 1500, -3100, 0, 200, 420),
    ("Bozor_Yoni_5q_Minora_2", QISQA, 4023, -3200, 0, 180, 320),
    ("Bozor_Yoni_5q_Minora_3", QISQA, -2900, 2400, 180, 100, 0),
    ("Bozor_Yoni_5q_Minora_4", QISQA, 2700, 3300, 180, 150, 220),
]


def _by_label():
    return {str(a.get_actor_label()): a for a in ACTORS.get_all_level_actors()}


def _world():
    return ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()


def _corners(mesh, t):
    b = mesh.get_bounding_box()
    return [t.transform_location(V(x, y, z)) for x in (b.min.x, b.max.x)
            for y in (b.min.y, b.max.y) for z in (b.min.z, b.max.z)]


def shablon_qismlari():
    """Shablon binoning ISM komponentlari: (mesh, materiallar, soya, kolliziya, [lokal transformlar])."""
    a = _by_label()[SHABLON]
    out = []
    for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
        if not c.static_mesh or c.get_instance_count() == 0:
            continue
        ts = [c.get_instance_transform(i, False) for i in range(c.get_instance_count())]
        coll = c.get_collision_enabled() != ue.CollisionEnabled.NO_COLLISION
        out.append((c.static_mesh, list(c.get_materials()), bool(c.cast_shadow), coll, ts))
    return out


def yigish(qismlar, bolaklar):
    """Bo'laklar bo'yicha lokal transformlarni tanlab/surib, markazlangan ro'yxat qaytaradi."""
    res = []
    for mesh, mats, shadow, coll, ts in qismlar:
        new = []
        for t in ts:
            ps = _corners(mesh, t)
            cx = (min(p.x for p in ps) + max(p.x for p in ps)) / 2
            for lo, hi, sh in bolaklar:
                if lo <= cx < hi:
                    new.append(ue.Transform(t.translation + V(sh, 0, 0), t.rotation.rotator(), t.scale3d))
        if new:
            res.append([mesh, mats, shadow, coll, new])
    gaz_trubasini_ula(res)
    # devorlar (Angle) bo'yicha markaz
    xs, ys = [], []
    for mesh, _, _, _, ts in res:
        if mesh.get_name().startswith("SM_LP_Plinth"):
            for t in ts:
                for p in _corners(mesh, t):
                    xs.append(p.x)
                    ys.append(p.y)
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    for r in res:
        r[4] = [ue.Transform(t.translation - V(cx, cy, 0), t.rotation.rotator(), t.scale3d) for t in r[4]]
    return res, (max(xs) - min(xs), max(ys) - min(ys))


def gaz_trubasini_ula(res):
    """Modullar kesilgan/nusxalangan joyda gorizontal gaz trubasi (Gas_Pipe_03) uzilib qoladi -
    bir qatordagi bo'laklar orasidagi 6 m gacha bo'shliqlar cho'zilgan bo'lak bilan to'ldiriladi."""
    for r in res:
        mesh, ts = r[0], r[4]
        if mesh.get_name() != "SM_LP_Gas_Pipe_03":
            continue
        b = mesh.get_bounding_box()
        qator = {}
        for t in ts:
            ps = _corners(mesh, t)
            key = (round(t.translation.y), round(t.translation.z))
            qator.setdefault(key, []).append((min(p.x for p in ps), max(p.x for p in ps), t))
        yangi = []
        for items in qator.values():
            items.sort(key=lambda i: i[0])
            for (a0, a1, ta), (b0, b1, _) in zip(items, items[1:]):
                gap = b0 - a1
                if 5 < gap < 600:
                    L = gap + 10
                    sx = L / (b.max.x - b.min.x)
                    loc = V((a1 + b0) / 2, ta.translation.y, ta.translation.z)
                    # mesh lokal X bo'ylab cho'zilgan; pivot markazdan siljigan bo'lsa ham markazga keltiramiz
                    off = (b.min.x + b.max.x) / 2 * sx
                    rot = ta.rotation.rotator()
                    sign = 1 if abs(rot.yaw) < 90 else -1
                    yangi.append(ue.Transform(loc - V(off * sign, 0, 0), rot, V(sx, ta.scale3d.y, ta.scale3d.z)))
        r[4] = ts + yangi


def eski_binolarni_olib_tashla():
    # "Bozor_Fon" nomi ovoz zonasi (BozorFonZonalar) da ham bor - mesh guruhini olamiz
    fon = [a for a in ACTORS.get_all_level_actors()
           if str(a.get_actor_label()) == "Bozor_Fon" and isinstance(a, ue.BozorMeshGroup)][0]
    n = 0
    for c in fon.get_components_by_class(ue.InstancedStaticMeshComponent):
        if not c.static_mesh or "Daraxt" in c.static_mesh.get_name():
            continue
        k = c.get_instance_count()
        if k:
            c.modify()
            c.clear_instances()
            n += k
    return n


def joylash(nom, qism, x, y, yaw, z=-1.2):
    eski = _by_label().get(nom)
    if eski:
        ACTORS.destroy_actor(eski)
    actor = ACTORS.spawn_actor_from_class(ue.BozorMeshGroup, V(x, y, z), ue.Rotator(0, 0, yaw))
    actor.set_actor_label(nom)
    actor.set_folder_path(FOLDER)
    at = ue.Transform(V(x, y, z), ue.Rotator(roll=0, pitch=0, yaw=yaw), V(1, 1, 1))
    for mesh, mats, shadow, coll, ts in qism:
        world = [ue.MathLibrary.compose_transforms(t, at) for t in ts]
        actor.add_instances(mesh, mats, world, shadow, coll)
    return actor


# ---------------------------------------------------------------- atrof (uy oldi)
ATROF_GURUH = "Bozor_Yoni_Uy_Oldi"
ATROF_FOLDER = "Bozor_Atrofidagi_Binolar/Uy_Oldi"
# guruh -> olib tashlanadigan meshlar (None = hammasi); yer va yo'l meshlariga tegilmaydi
TOZALANADI = {
    "Bozor_Atrof": ("SM_ATR_", "SM_Daraxt", "SM_EB_", "SM_SM_", "SM_DEC_Lyuk"),
    "Bozor_Fon": ("SM_Daraxt",),
    "Daraxtlar_va_Ekinlar": None,
    "Skameykalar_va_Chiqindi_Qutilari": None,
    "Uy_Oldi_Gul_va_Butalar": None,
    "Toshkent_Bino_Tagi": None,
}
OTMOSTKA = 85.0
SKAMEYKA_YAW = -90.0


class Bino:
    def __init__(self, nom, x, y, yaw, L, W, eshiklar):
        self.nom, self.x, self.y, self.L, self.W, self.eshiklar = nom, x, y, L, W, eshiklar
        self.yaw = yaw
        self.c, self.s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def dunyo(self, lx, ly):
        return V(self.x + lx * self.c - ly * self.s, self.y + lx * self.s + ly * self.c, 0)

    def lokal(self, p):
        dx, dy = p.x - self.x, p.y - self.y
        return dx * self.c + dy * self.s, -dx * self.s + dy * self.c

    def ichida(self, p, m):
        lx, ly = self.lokal(p)
        return abs(lx) < self.L / 2 + m and abs(ly) < self.W / 2 + m

    def kirish_oldi(self, p):
        """Eshik oldidagi yo'lak zonasi (bo'sh qolishi kerak)."""
        lx, ly = self.lokal(p)
        return any(abs(lx - e) < 190 for e in self.eshiklar) and -self.W / 2 - 650 < ly < -self.W / 2


def eshiklar(qism):
    out = []
    for mesh, _, _, _, ts in qism:
        if mesh.get_name() == "SM_LP_EDoor_01FK":
            for t in ts:
                ps = _corners(mesh, t)
                out.append((min(p.x for p in ps) + max(p.x for p in ps)) / 2)
    return sorted(out)


def _guruh(label):
    for a in ACTORS.get_all_level_actors():
        if str(a.get_actor_label()) == label and isinstance(a, ue.BozorMeshGroup):
            return a
    return None


def tozalash(binolar):
    """Yangi binolar ichida/devoriga yopishgan daraxt, buta, gulzor, skameyka, eski otmostkani olib tashlaydi."""
    removed = {}
    for label, prefixes in TOZALANADI.items():
        g = _guruh(label)
        if not g:
            continue
        for c in g.get_components_by_class(ue.InstancedStaticMeshComponent):
            name = c.static_mesh.get_name() if c.static_mesh else ""
            if prefixes is not None and not name.startswith(prefixes):
                continue
            daraxt = name.startswith("SM_Daraxt")
            jihoz = name.startswith(("SM_EB_", "SM_SM_"))
            m = 450 if daraxt else (300 if label == "Toshkent_Bino_Tagi" or jihoz else 150)
            if "Damas" in name or "Labo" in name:   # mashina palisadnik/podyezdga kirib qolmasin
                m = 600
            idx = []
            for i in range(c.get_instance_count()):
                p = c.get_instance_transform(i, True).translation
                if any(b.ichida(p, m) or b.kirish_oldi(p) for b in binolar):
                    idx.append(i)
            if idx:
                c.modify()
                for i in reversed(idx):
                    c.remove_instance(i)
                removed[f"{label}/{name}"] = removed.get(f"{label}/{name}", 0) + len(idx)
    return removed


def yol_kesishmalari(binolar):
    """Bino ostida qolgan yo'l/yo'lak bo'laklari (ma'lumot uchun)."""
    out = []
    g = _guruh("Bozor_Atrof")
    for c in g.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        if not name.startswith(("SM_YOL_", "SM_YER_Yolak")):
            continue
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            ps = _corners(c.static_mesh, t)
            for b in binolar:
                loc = [b.lokal(p) for p in ps]
                if (min(l[0] for l in loc) < b.L / 2 and max(l[0] for l in loc) > -b.L / 2 and
                        min(l[1] for l in loc) < b.W / 2 and max(l[1] for l in loc) > -b.W / 2):
                    out.append((b.nom, name, i, round(t.translation.x), round(t.translation.y)))
    return out


# Bino ostida qolgan yo'l bo'laklari: (mesh, eski markaz x, y) -> (yangi markaz y, Y masshtabi)
# 27.8 m lik yo'laklar bozor tomonidagi bo'lagigacha qisqartiriladi, qolganlari binodan nariga suriladi.
YOL_TUZAT = {
    ("SM_YOL_Toza_27p8x4", 1600, -3010): (-1975, 710 / 2780),
    ("SM_YOL_Toza_27p8x4", 3200, -3010): (-2175, 1110 / 2780),
    ("SM_YOL_Toza_27p8x4", 3200, 3010): (2075, 910 / 2780),
    ("SM_YOL_Toza_1p4x17", -3250, 2930): (3170, 1.0),
    ("SM_YOL_Toza_2p8x22p4", -2980, 1400): (1530, 0.5),
    ("SM_YOL_Toza_2x18", 7500, -2400): (-2520, 1.0),
    # minoralar burilgach (2-qadam): 2-minora 3 m surildi, 3-minora kirishi yo'lakka qaradi
    ("SM_YOL_Toza_27p8x4", 3200, -2175): (-2025, 810 / 2780),
    ("SM_YOL_Toza_1p4x17", -3250, 3170): (3330, 1.0),
}


# Eski kirishlar oldidagi buta qatorlari: (eski x, y) -> yangi y (yangi kirish yo'laklari orasiga)
BUTA_SURISH = {
    (-2230, -2900): -2490, (-2230, -1400): -620, (-2230, 100): 1100,
    (6100, -1100): -1000, (6100, 1900): 1800,
}
ESKI_KIRISH = ("SM_EB_Skameyka", "SM_SM_Axlat_Qutisi", "SM_YOL_Toza_3p2x5p2")


def eski_kirishlarni_tozala(binolar):
    """Eski binolar kirishiga olib borgan yo'lak, skameyka va qutilar (kirish tomonida, 7 m gacha)."""
    g = _guruh("Bozor_Atrof")
    n = 0
    for c in g.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        if name.startswith(ESKI_KIRISH):
            idx = []
            for i in range(c.get_instance_count()):
                p = c.get_instance_transform(i, True).translation
                for b in binolar:
                    lx, ly = b.lokal(p)
                    if abs(lx) < b.L / 2 + 100 and -b.W / 2 - 700 < ly < -b.W / 2:
                        idx.append(i)
                        break
            if idx:
                c.modify()
                for i in reversed(idx):
                    c.remove_instance(i)
                n += len(idx)
        elif name == "SM_ATR_Buta_Qator":
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                for (x, y), ny in BUTA_SURISH.items():
                    if abs(t.translation.x - x) < 30 and abs(t.translation.y - y) < 30:
                        c.modify()
                        c.update_instance_transform(i, ue.Transform(V(x, ny, t.translation.z), t.rotation.rotator(),
                                                                    t.scale3d), True, True, True)
                        n += 1
    return n


# Shimol hovlisidagi 1.75 m asfalt trotuar devorga 1.5 m yaqin - podyezd/palisadnikka joy uchun
# plitkali yo'lak yoniga suriladi: (mesh, eski x, y) -> yangi x
YOL_SURISH_X = {("SM_YOL_Toza_54x1p75", 8312, 400): 8410}


def maysa_boshliqlari(g):
    """Janub ko'cha tomonidagi maysa qatori (x≈-2130) eski kirish yo'laklari olib tashlangach ochilib
    qolgan bo'shliqlarini o'sha maysa meshi (SM_YER_Maysa_2p5x5p4) bilan yopadi."""
    atrof = _guruh("Bozor_Atrof")
    oraliq, mesh, mats = [], None, []
    for c in atrof.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        if not name.startswith("SM_YER_Maysa_"):
            continue
        if name == "SM_YER_Maysa_2p5x5p4":
            mesh, mats = c.static_mesh, list(c.get_materials())
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            if abs(t.translation.x + 2130) > 30:
                continue
            ps = _corners(c.static_mesh, t)
            oraliq.append((min(p.y for p in ps), max(p.y for p in ps)))
    if not mesh:
        return 0
    b = mesh.get_bounding_box()
    h = b.max.y - b.min.y
    ts = []
    oraliq.sort()
    for (a0, a1), (b0, b1) in zip(oraliq, oraliq[1:]):
        gap = b0 - a1
        if 20 < gap < 700:
            ts.append(ue.Transform(V(-2130, (a1 + b0) / 2, -1.0), ue.Rotator(), V(1, (gap + 10) / h, 1)))
    if ts:
        g.add_instances(mesh, mats, ts, False, False)
    return len(ts)


def yollarni_tuzat():
    g = _guruh("Bozor_Atrof")
    n = 0
    for c in g.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            for (m, x, y), nx in YOL_SURISH_X.items():
                if m == name and abs(t.translation.x - x) < 30 and abs(t.translation.y - y) < 30:
                    c.modify()
                    c.update_instance_transform(i, ue.Transform(V(nx, t.translation.y, t.translation.z),
                                                                t.rotation.rotator(), t.scale3d), True, True, True)
                    n += 1
    for c in g.get_components_by_class(ue.InstancedStaticMeshComponent):
        name = c.static_mesh.get_name()
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            for (m, x, y), (ny, sy) in YOL_TUZAT.items():
                if m == name and abs(t.translation.x - x) < 30 and abs(t.translation.y - y) < 30:
                    c.modify()
                    s = t.scale3d
                    c.update_instance_transform(i, ue.Transform(V(t.translation.x, ny, t.translation.z),
                                                                t.rotation.rotator(), V(s.x, sy, s.z)),
                                                True, True, True)
                    n += 1
    return n


def _box(b, lx, ly, sx, sy, top, h, pitch=0.0, yaw_add=0.0):
    """Bino lokal koordinatasidagi beton bo'lak (Cube 100 sm) transformi."""
    p = b.dunyo(lx, ly)
    return ue.Transform(V(p.x, p.y, top - h / 2), ue.Rotator(roll=0, pitch=pitch, yaw=b.yaw + yaw_add),
                        V(sx / 100, sy / 100, h / 100))


def _arange(a, b, step):
    n = max(1, int(round((b - a) / step)))
    return [a + (b - a) * (i + 0.5) / n for i in range(n)], (b - a) / n


def uy_oldi(binolar, seed=11):
    import random
    rnd = random.Random(seed)
    cube = ue.load_asset("/Engine/BasicShapes/Cube")
    tagi = _guruh("Toshkent_Bino_Tagi")
    beton = None
    if tagi:
        for c in tagi.get_components_by_class(ue.InstancedStaticMeshComponent):
            if c.static_mesh and c.static_mesh.get_name() == "Cube":
                beton = c.get_material(0)
    beton = beton or ue.load_asset("/Game/Bozor/Materials/MI_M_Beton")
    yolak_mat = ue.load_asset("/Game/Bozor/Toshkent/Uy_Oldi/MI_Beton_Eski") or beton
    skam = ue.load_asset("/Game/Bozor/Meshes/SM_EB_Skameyka_Suyanchiqli")
    axlat = ue.load_asset("/Game/Bozor/Meshes/SM_SM_Axlat_Qutisi_LOD0")
    ot = ue.load_asset("/Game/Bozor/Meshes/SM_ATR_Ot_Yamoq_1")

    otm, yolak, skameyka, quti, otlar = [], [], [], [], []
    pal = {"bordyur": [], "tuproq": [], "panjara": [], "o'simlik": {}}
    for b in binolar:
        hl, hw = b.L / 2, b.W / 2
        w = OTMOSTKA + 12
        # 4 tomon: (lokal nuqta funksiyasi, tashqi normal burchagi)
        tomonlar = [
            (lambda t: (t, -hw - w / 2 + 12), 0, hl, True),    # kirish tomoni (-Y)
            (lambda t: (t, hw + w / 2 - 12), 0, hl, False),    # orqa (+Y)
            (lambda t: (-hl - w / 2 + 12, t), 90, hw + w - 12, False),  # chekka (-X)
            (lambda t: (hl + w / 2 - 12, t), 90, hw + w - 12, False),   # chekka (+X)
        ]
        for f, rot, half, kirish in tomonlar:
            ts, step = _arange(-half, half, 100.0)
            for t in ts:
                if kirish and any(abs(t - e) < 150 for e in b.eshiklar):
                    continue
                lx, ly = f(t)
                sx, sy = (w, step * 1.03) if rot == 0 else (step * 1.03, w)
                otm.append(_box(b, lx, ly, sx, sy, 4.0, 12.0))
                if rnd.random() < 0.3:     # otmostka chetida quruq o't tutami
                    nx, ny = (0, -1 if ly < 0 else 1) if rot == 0 else (-1 if lx < 0 else 1, 0)
                    d = w / 2 + rnd.uniform(5, 40)
                    p = b.dunyo(lx + nx * d + (rnd.uniform(-45, 45) if rot == 0 else 0),
                                ly + ny * d + (rnd.uniform(-45, 45) if rot != 0 else 0))
                    s = rnd.uniform(0.16, 0.32)
                    otlar.append(ue.Transform(V(p.x, p.y, -2), ue.Rotator(0, 0, rnd.uniform(0, 360)),
                                              V(s, s, s * rnd.uniform(0.7, 1.1))))
        for e in b.eshiklar:
            # podyezd kirishi (krilso): beton maydoncha + bitta zina, hovli yo'lagigacha yo'lcha
            otm.append(_box(b, e, -hw - 85, 300, 170, 16.0, 20.0))
            otm.append(_box(b, e, -hw - 190, 260, 40, 8.0, 12.0))
            if b.yolcha > 0:
                yolak.append(_box(b, e, -hw - 210 - b.yolcha / 2, 160, b.yolcha, 3.0, 8.0))
            # kirish yonida devorga suyalgan skameyka va kichik urna
            p = b.dunyo(e + PODYEZD_SKAMEYKA, -hw - 150)
            skameyka.append(ue.Transform(V(p.x, p.y, 0), ue.Rotator(0, 0, b.yaw + SKAMEYKA_YAW), V(1, 1, 1)))
            p = b.dunyo(e + PODYEZD_SKAMEYKA + 150, -hw - 135)
            quti.append(ue.Transform(V(p.x, p.y, 0), ue.Rotator(0, 0, b.yaw + rnd.uniform(-8, 8)),
                                     V(0.8, 0.8, 0.8)))
        palisadniklar(b, rnd, pal)

    eski = _guruh(ATROF_GURUH)
    if eski:
        ACTORS.destroy_actor(eski)
    g = ACTORS.spawn_actor_from_class(ue.BozorMeshGroup, V(), ue.Rotator())
    g.set_actor_label(ATROF_GURUH)
    g.set_folder_path(ATROF_FOLDER)
    g.add_instances(cube, [beton], otm, True, True)
    g.add_instances(cube, [yolak_mat], yolak, False, True)
    if skam:
        g.add_instances(skam, [], skameyka, True, True)
    if axlat:
        g.add_instances(axlat, [], quti, True, True)
    if ot:
        g.add_instances(ot, [], otlar, False, False)
    m = "/Game/Bozor/Toshkent/Uy_Oldi/"
    g.add_instances(cube, [ue.load_asset(m + "MI_Bordyur") or beton], pal["bordyur"], True, True)
    g.add_instances(cube, [ue.load_asset("/Game/Bozor/Materials/MI_M_Yer_Tuproq")], pal["tuproq"], False, False)
    g.add_instances(cube, [ue.load_asset(m + "MI_Panjara_Yashil")], pal["panjara"], True, True)
    for mesh_path, ts in pal["o'simlik"].items():
        mesh = ue.load_asset(mesh_path)
        g.add_instances(mesh, _osimlik_materiallari(mesh), ts, True, False)
    print("maysa bo'shliqlari yopildi:", maysa_boshliqlari(g))
    return {"otmostka": len(otm), "yolak": len(yolak), "skameyka": len(skameyka), "quti": len(quti),
            "o't": len(otlar), "palisadnik bo'lagi": len(pal["tuproq"]),
            "o'simlik": sum(len(v) for v in pal["o'simlik"].values())}


PODYEZD_SKAMEYKA = 290   # skameyka markazi eshikdan (lokal X), uzunligi 230 sm
OSIMLIKLAR = [  # (mesh, masshtab oralig'i, ehtimol) - Uy_Oldi_Gul_va_Butalar bilan bir xil
    ("/Game/Bozor/Meshes/SM_ATR_Buta_Toda_1", (0.30, 0.40), 0.35),
    ("/Game/Bozor/Meshes/SM_ATR_Buta_Toda_2", (0.24, 0.32), 0.30),
    ("/Game/Bozor/Meshes/SM_ATR_Gulzor_2", (0.24, 0.30), 0.35),
]


def _osimlik_materiallari(mesh):
    """Uy_Oldi_Gul_va_Butalar dagi shu mesh komponentining material o'rniga qo'yilganlari."""
    g = _guruh("Uy_Oldi_Gul_va_Butalar")
    if g:
        for c in g.get_components_by_class(ue.InstancedStaticMeshComponent):
            if c.static_mesh == mesh:
                return list(c.get_materials())
    return []


def palisadniklar(b, rnd, pal):
    """Podyezdlar orasida, derazalar tagida bordyurli, past yashil panjarali palisadnik."""
    hl, hw = b.L / 2, b.W / 2
    y0 = -hw - OTMOSTKA            # otmostka tashqi cheti
    y1 = y0 - b.palisadnik
    gaps = sorted((e - 175, e + PODYEZD_SKAMEYKA + 230) for e in b.eshiklar)
    segs, a = [], -hl + 15
    for g0, g1 in gaps:
        if g0 - a > 150:
            segs.append((a, g0))
        a = max(a, g1)
    if hl - 15 - a > 150:
        segs.append((a, hl - 15))
    for a, c in segs:
        mid, L = (a + c) / 2, c - a
        D = b.palisadnik
        # bordyur: old cheti va ikki yon
        pal["bordyur"].append(_box(b, mid, y1 + 5, L, 10, 14.0, 20.0))
        for x in (a + 5, c - 5):
            pal["bordyur"].append(_box(b, x, (y0 + y1) / 2, 10, D, 14.0, 20.0))
        pal["tuproq"].append(_box(b, mid, (y0 + y1) / 2, L - 12, D - 12, 4.0, 8.0))
        # past panjara (ustunchalar + 2 ta gorizontal truba)
        ts, _ = _arange(a, c, 180.0)
        for x in [a + 6] + ts + [c - 6]:
            pal["panjara"].append(_box(b, x, y1 + 5, 4, 4, 62.0, 50.0))
        for z in (32.0, 60.0):
            pal["panjara"].append(_box(b, mid, y1 + 5, L, 3, z, 3.0))
        # o'simliklar: butalar va gullar, derazalar tagida
        n = max(2, int(L / 65))
        for _ in range(n):
            r = rnd.random()
            for path, (s0, s1), w in OSIMLIKLAR:
                r -= w
                if r <= 0:
                    break
            lx = rnd.uniform(a + 30, c - 30)
            ly = rnd.uniform(y1 + 30, y0 - 25)
            p = b.dunyo(lx, ly)
            s = rnd.uniform(s0, s1)
            pal["o'simlik"].setdefault(path, []).append(
                ue.Transform(V(p.x, p.y, 4), ue.Rotator(0, 0, rnd.uniform(0, 360)), V(s, s, s)))


def saqlash(world):
    content = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_content_dir()))
    file = content / "Bozor/Maps/Bozor.umap"
    backup = SAVED / "XaritaZaxira" / f"Bozor_BinoAlmashtirishOldi_{time.time_ns()}.umap"
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(file, backup)
    file.chmod(file.stat().st_mode | stat.S_IWRITE)
    if not ue.EditorLoadingAndSavingUtils.save_map(world, "/Game/Bozor/Maps/Bozor"):
        raise RuntimeError("Xarita saqlanmadi")
    return str(backup)


def main(saqla=False):
    world = _world()
    if not world or not world.get_path_name().startswith("/Game/Bozor/Maps/Bozor"):
        raise RuntimeError("Bozor xaritasi ochiq emas")
    qismlar = shablon_qismlari()
    turlar = {id(UZUN): yigish(qismlar, UZUN), id(QISQA): yigish(qismlar, QISQA)}
    binolar = []
    with ue.ScopedEditorTransaction("Eski binolarni almashtirish"):
        n = eski_binolarni_olib_tashla()
        print("eski bino bo'laklari olib tashlandi:", n)
        for nom, bol, x, y, yaw, pal, yol in BINOLAR:
            qism, (L, W) = turlar[id(bol)]
            joylash(nom, qism, x, y, yaw)
            b = Bino(nom, x, y, yaw, L, W, eshiklar(qism))
            b.palisadnik, b.yolcha = pal, yol
            binolar.append(b)
            print(f"{nom}: {L/100:.1f} x {W/100:.1f} m, {sum(len(q[4]) for q in qism)} bo'lak")
        print("tozalandi:", tozalash(binolar))
        print("yo'llar tuzatildi:", yollarni_tuzat())
        print("eski kirishlar:", eski_kirishlarni_tozala(binolar))
        print("uy oldi:", uy_oldi(binolar))
    for k in yol_kesishmalari(binolar):
        print("bino ostida yo'l:", k)
    if saqla:
        print("saqlandi, zaxira:", saqlash(world))
