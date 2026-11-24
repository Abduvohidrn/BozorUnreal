"""Mahsulotli yashiklarning pastki qismini to'ldiradi (yon teshiklardan ichi bo'sh ko'rinmasin).

mol_uyum.py mollarni faqat yashik ustiga 2 qavat teradi; yashik tubi bilan pastki qavat orasi bo'sh qoladi.
Bu skript xaritadagi har bir bo'sh yashik nusxasi (yog'och/plastik) uchun:
  - ichiga tushgan mollarni (SM_Mol_*) topadi -> mol turi(lari), o'lchami va eng pastki qavat balandligi;
  - yashik tubidan shu qavatgacha DEVOR BO'YLAB halqa qilib shu mollarni teradi (teshiklardan ko'rinadigan qism);
  - halqa ichini to'ldiruvchi blok (Cube + MI_M_Toldiruvchi) bilan to'ldiradi - o'rtadagi mollar shart emas (unumdorlik).
Mol yo'q yashiklarga (bo'sh taxlangan yashiklar) tegmaydi. Qayta ishga tushirsa ham ikkinchi marta qo'shmaydi
(pastki qavat tubga yetgan bo'ladi). Mavjud ISM komponentlariga qo'shadi (yangi draw call yo'q).

MUHARRIR YOPIQ bo'lishi kerak (xaritani saqlaydi). Zaxira: Saved/Zaxira/Bozor_yashik_toldir_oldi.umap. Natija: Saved/yashik_toldir.txt
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/yashik_toldir.py
Sinov (saqlamaydi): Saved/YashikTol/DRY fayli bor bo'lsa.
"""
import math
import os
import random
import shutil
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

LOYIHA = os.path.abspath(unreal.Paths.project_dir())
bc.LOG = os.path.join(LOYIHA, "Saved", "yashik_toldir.txt")
open(bc.LOG, "w").close()
log = bc.log
DRY = os.path.exists(os.path.join(LOYIHA, "Saved", "YashikTol", "DRY"))

YASHIKLAR = ("SM_SM_Yashik_LOD0", "SM_SM_Yashik_Plastik_Kok_LOD0", "SM_SM_Yashik_Plastik_Yashil_LOD0")
# mol bilan birga modellangan eski yashik (o'z mol meshi yo'q): faqat to'ldiruvchi blok
ESKI_YASHIK = {"SM_SM_Yashik_Pomidor_LOD0": 0.72}   # mesh ichidagi mol qatlami tagi, yashik balandligiga nisbatan
TOLDIRUVCHI = bc.ROOT + "/Materials/MI_M_Toldiruvchi"
DEVOR = 2.5      # devor qalinligi, sm
TUB = 1.5        # tub qalinligi, sm
QAVAT_QADAM = 0.78


class Mol:
    """Mol meshi: yotgan holatdagi L/W/H (mol_uyum.py bilan bir xil qoida)."""

    def __init__(self, sm, mi, kop):
        self.sm, self.mi, self.kop = sm, mi, kop
        b = sm.get_bounding_box()
        self.markaz = (b.min + b.max) * 0.5
        o = [b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z]
        uzun = max(range(3), key=lambda i: o[i])
        self.choziq = o[uzun] > 1.45 * sorted(o)[1]
        self.asos = unreal.Rotator(0, 0, 0) if not self.choziq or uzun == 0 else \
            (unreal.Rotator(0, 0, 90) if uzun == 1 else unreal.Rotator(0, 90, 0))
        if self.choziq:
            q = sorted(o)
            self.L, self.W, self.H = o[uzun] * kop, q[1] * kop, q[0] * kop
        else:
            d = max(o[0], o[1]) * kop
            self.L = self.W = d
            self.H = o[2] * kop


def kvat(rot):
    return rot.quaternion() if hasattr(rot, "quaternion") else unreal.MathLibrary.conv_rotator_to_quaternion(rot)


def halqa(mollar, x0, x1, y0, y1, z0, qavat, rng):
    """Devor bo'ylab (bir mol qalinlikda) qavatlar: [(mol, mahalliy nuqta, Rotator, o'lcham)]"""
    out = []
    m0 = mollar[0]
    dx, dy = m0.L * 0.92, m0.W * 0.92
    ax, bx, ay, by = x0 + dx / 2, x1 - dx / 2, y0 + dy / 2, y1 - dy / 2
    if ax > bx or ay > by:
        return out
    nx, ny = int((bx - ax) // dx) + 1, int((by - ay) // dy) + 1
    sx = (bx - ax) / (nx - 1) if nx > 1 else 0
    sy = (by - ay) / (ny - 1) if ny > 1 else 0
    for k in range(qavat):
        z = z0 + m0.H * (0.5 + QAVAT_QADAM * k)
        for i in range(nx):
            for j in range(ny):
                if 0 < i < nx - 1 and 0 < j < ny - 1:
                    continue          # o'rtasi - to'ldiruvchi blok
                mol = rng.choice(mollar)
                s = mol.kop * rng.uniform(0.9, 1.1)
                x = (ax + i * sx if nx > 1 else (ax + bx) / 2) + rng.uniform(-0.1, 0.1) * dx + (dx * 0.25 if k % 2 else 0)
                y = (ay + j * sy if ny > 1 else (ay + by) / 2) + rng.uniform(-0.1, 0.1) * dy
                x, y = min(max(x, ax), bx), min(max(y, ay), by)
                if mol.choziq:
                    r = unreal.Rotator(rng.uniform(0, 360), 0, rng.uniform(-15, 15))
                else:
                    r = unreal.Rotator(rng.uniform(-35, 35), rng.uniform(-35, 35), rng.uniform(0, 360))
                out.append((mol, unreal.Vector(x, y, z + rng.uniform(-0.08, 0.08) * mol.H), r, s))
    return out


def dunyo(T, mol, p, r, s):
    q = T.rotation if not hasattr(T.rotation, "rotator") else kvat(T.rotation.rotator())
    rq = unreal.MathLibrary.multiply_quat_quat(q, unreal.MathLibrary.multiply_quat_quat(kvat(r), kvat(mol.asos)))
    joy = unreal.MathLibrary.transform_location(T, p)
    joy = joy - unreal.MathLibrary.quat_rotate_vector(rq, mol.markaz * s)
    return unreal.Transform(joy, unreal.MathLibrary.quat_rotator(rq), unreal.Vector(s, s, s))


class Mahalliy:
    """Yashik nusxasi mahalliy koordinatasi (faqat yaw burilish, masshtab bor) - tez teskari o'girish."""

    def __init__(self, T):
        self.T = T
        self.t = T.translation
        self.s = T.scale3d
        yaw = T.rotation.rotator().yaw if hasattr(T.rotation, "rotator") else unreal.MathLibrary.quat_rotator(T.rotation).yaw
        self.c, self.sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def ga(self, p):
        dx, dy, dz = p.x - self.t.x, p.y - self.t.y, p.z - self.t.z
        return (self.c * dx + self.sn * dy) / self.s.x, (-self.sn * dx + self.c * dy) / self.s.y, dz / self.s.z


# ---- xarita
xarita = os.path.join(LOYIHA, "Content", "Bozor", "Maps", "Bozor.umap")
if not DRY:
    zax = os.path.join(LOYIHA, "Saved", "Zaxira", "Bozor_yashik_toldir_oldi.umap")
    os.makedirs(os.path.dirname(zax), exist_ok=True)
    shutil.copy2(xarita, zax)
    log("zaxira:", zax)
unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
guruhlar = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorMeshGroup)]

kub = unreal.load_asset("/Engine/BasicShapes/Cube")
toldir = unreal.load_asset(TOLDIRUVCHI)

# barcha mollar (dunyo nuqtasi bilan), 1 m to'r bo'yicha
molkomp = {}      # mesh nomi -> (sm, mi)
tor = {}
for g in guruhlar:
    for c in g.get_components_by_class(unreal.InstancedStaticMeshComponent):
        sm = c.get_editor_property("static_mesh")
        if not sm or not sm.get_name().startswith("SM_Mol_"):
            continue
        molkomp.setdefault(sm.get_name(), (sm, c.get_material(0)))
        b = sm.get_bounding_box()
        for i in range(c.get_instance_count()):
            T = c.get_instance_transform(i, True)
            s = T.scale3d.x
            # bounding box markazi (pivot markazda bo'lmasligi mumkin)
            p = unreal.MathLibrary.transform_location(T, (b.min + b.max) * 0.5)
            tor.setdefault((int(p.x // 100), int(p.y // 100)), []).append((sm.get_name(), p, s))

yangi = {}        # (mesh yo'li, material yo'li) -> (sm, mi, [transform])
def qosh(sm, mi, t):
    yangi.setdefault((sm.get_path_name(), mi.get_path_name() if mi else ""), (sm, mi, []))[2].append(t)

jami_yashik = toldirildi = 0
for g in guruhlar:
    for c in g.get_components_by_class(unreal.InstancedStaticMeshComponent):
        sm = c.get_editor_property("static_mesh")
        if not sm:
            continue
        nom = sm.get_name()
        if nom not in YASHIKLAR and nom not in ESKI_YASHIK:
            continue
        bb = sm.get_bounding_box()
        x0, x1 = bb.min.x + DEVOR, min(bb.max.x, 0.0 if nom in YASHIKLAR else bb.max.x) - DEVOR
        y0, y1 = bb.min.y + DEVOR, bb.max.y - DEVOR
        h = bb.max.z - bb.min.z
        for i in range(c.get_instance_count()):
            T = c.get_instance_transform(i, True)
            jami_yashik += 1
            rng = random.Random(hash((g.get_actor_label(), nom, i, "toldir")) & 0xFFFFFFFF)
            z0 = bb.min.z + TUB
            if nom in ESKI_YASHIK:
                past, mollar, ichida = bb.min.z + h * ESKI_YASHIK[nom], None, []
            else:
                L = Mahalliy(T)
                kx, ky = int(T.translation.x // 100), int(T.translation.y // 100)
                ichida = []
                for gx in range(kx - 1, kx + 2):
                    for gy in range(ky - 1, ky + 2):
                        for mn, p, s in tor.get((gx, gy), ()):
                            lx, ly, lz = L.ga(p)
                            if bb.min.x - 1 <= lx <= bb.max.x + 1 and bb.min.y - 1 <= ly <= bb.max.y + 1 and bb.min.z <= lz <= bb.max.z + 12:
                                ichida.append((mn, lz, s))
                if not ichida:
                    continue
                # mol turlari (ko'pchilik), o'rtacha o'lcham
                sanoq = {}
                for mn, lz, s in ichida:
                    sanoq.setdefault(mn, []).append(s)
                turlar = sorted(sanoq, key=lambda k: -len(sanoq[k]))
                mollar = []
                for mn in turlar:
                    if len(sanoq[mn]) * 4 < len(ichida):
                        continue
                    kop = sum(sanoq[mn]) / len(sanoq[mn])
                    msm, mmi = molkomp[mn]
                    mollar.append(Mol(msm, mmi, kop))
                m0 = mollar[0]
                # eng pastki qavat tagi (mol markazi - yarim balandlik)
                past = min(lz - Mol(molkomp[mn][0], None, s).H * 0.5 for mn, lz, s in ichida)
            if past - z0 < 3.0:
                continue      # allaqachon to'la
            toldirildi += 1
            ichki = (x0, x1, y0, y1)
            n_mol = 0
            if mollar:
                qavat = max(1, int(math.ceil((past - z0) / (m0.H * QAVAT_QADAM) - 0.15)))
                joy = halqa(mollar, x0, x1, y0, y1, z0, qavat, rng)
                for mol, p, r, s in joy:
                    qosh(mol.sm, mol.mi, dunyo(T, mol, p, r, s))
                n_mol = len(joy)
                ich = 0.8 * min(m0.L, m0.W)         # halqa ichidagi blok (halqa ortidan ko'rinmaydi)
                ichki = (x0 + ich, x1 - ich, y0 + ich, y1 - ich)
                if ichki[0] >= ichki[1] or ichki[2] >= ichki[3]:
                    ichki = None
            if ichki:
                bx0, bx1, by0, by1 = ichki
                ust = past + 1.0
                tq = T.rotation.rotator() if hasattr(T.rotation, "rotator") else T.rotation
                blok = unreal.Transform(
                    unreal.MathLibrary.transform_location(T, unreal.Vector((bx0 + bx1) / 2, (by0 + by1) / 2, (z0 + ust) / 2)),
                    tq, unreal.Vector((bx1 - bx0) / 100.0 * T.scale3d.x, (by1 - by0) / 100.0 * T.scale3d.y, (ust - z0) / 100.0 * T.scale3d.z))
                qosh(kub, toldir, blok)
            log("%-18s %-34s #%-2d %s  ichida %3d mol, tag %.1f -> +%d mol%s" % (
                g.get_actor_label(), nom, i, "+".join(m.sm.get_name()[7:] for m in mollar) if mollar else "-",
                len(ichida), past, n_mol, " +blok" if ichki else ""))

log("yashiklar %d, to'ldirildi %d" % (jami_yashik, toldirildi))

# mavjud ISM komponentlariga qo'shish (bir xil mesh + material)
mg = next(g for g in guruhlar if g.get_actor_label() == "Bozor_Mahsulotlar")
for (yol, myol), (sm, mi, tlar) in yangi.items():
    log("  + %-28s %-22s %5d" % (sm.get_name(), mi.get_name() if mi else "-", len(tlar)))
    if DRY:
        continue
    komp = None
    for c in mg.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if c.get_editor_property("static_mesh") == sm and c.get_material(0) == mi:
            komp = c
            break
    if komp:
        komp.modify()
        komp.add_instances(tlar, False, True)
    else:
        mg.add_instances(sm, [mi] if mi else [], tlar, False, False)
log("jami yangi nusxa:", sum(len(v[2]) for v in yangi.values()))
if not DRY:
    unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Bozor/Maps/Bozor")
log("TAYYOR" + (" (DRY - saqlanmadi)" if DRY else ""))
