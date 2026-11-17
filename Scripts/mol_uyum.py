"""Rastalardagi eski (qo'lda modellangan) mollarni Megascans mollari uyumiga almashtiradi: Bozor_Mahsulotlar aktori.

Eski mesh nusxalari o'chiriladi (ISM tozalanadi), o'rniga shu joyga (shu burilish bilan) yangi mollar teriladi:
  YASHIK - mol bilan birga modellangan yashik: bo'sh yog'och yashik + ichida to'ldiruvchi tag + 2 qavat mol
  YUK    - yashik/savat ustidagi mol qatlami: shu chegarada 2 qavat uyum
  UYUM   - tarvuz/qovun uyumi: piramida
Mol nusxalari: tasodifiy burilish (dumaloqlar), cho'ziqlar yotqizilgan; o'lcham +-10%; har eski nusxa uchun bir xil seed.
Mollar import: Scripts/mol_megascans_import.py (/Game/Bozor/Mollar). Xarita zaxirasi: Saved/Zaxira/Bozor_mol_uyum_oldi.umap.
MUHARRIR YOPIQ bo'lishi kerak (xaritani saqlaydi). Natija: Saved/mol_uyum.txt
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/mol_uyum.py
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
bc.LOG = os.path.join(LOYIHA, "Saved", "mol_uyum.txt")
open(bc.LOG, "w").close()
log = bc.log
M = bc.ROOT + "/Meshes/"
MOL = bc.ROOT + "/Mollar/"

# eski mesh -> (tur, [yangi mollar], mol o'lchami ko'paytuvchisi)
ALMASH = {
    "SM_SM_Yashik_Piyoz_LOD0": ("YASHIK", ["Piyoz_1", "Piyoz_2"], 1.0),
    "SM_SM_Yashik_Bodring_LOD0": ("YASHIK", ["Bodring_1", "Bodring_2"], 1.0),
    "SM_SM_Yashik_Karam_LOD0": ("YASHIK", ["Karam_1"], 1.1),
    "SM_SM_Yashik_Kartoshka_LOD0": ("YASHIK", ["Kartoshka_1"], 1.0),
    "SM_SM_Yashik_Sabzi_LOD0": ("YASHIK", ["Sabzi_1"], 1.0),
    "SM_SM_Yashik_Uzum_LOD0": ("YASHIK", ["Uzum_1"], 1.6),
    "SM_SM_Yashik_Anor_LOD0": ("YASHIK", ["Anor_1"], 1.0),
    "SM_YUK_Apelsin_Plastik_Mahsulot": ("YUK", ["Apelsin_1"], 1.5),
    "SM_YUK_Baqlajon_Yashik_Mahsulot": ("YUK", ["Baqlajon_1"], 1.5),
    "SM_YUK_Behi_Yashik_Mahsulot": ("YUK", ["Behi_1"], 1.0),
    "SM_YUK_Kivi_Plastik_Mahsulot": ("YUK", ["Kivi_1"], 1.0),
    "SM_YUK_Limon_Savat_Mahsulot": ("YUK", ["Limon_1"], 1.1),
    "SM_YUK_Olma_Plastik_Mahsulot": ("YUK", ["Olma_1"], 1.0),
    "SM_YUK_Olma_Yashik_Mahsulot": ("YUK", ["Olma_1"], 1.0),
    "SM_YUK_Qalampir_Plastik_Mahsulot": ("YUK", ["Qalampir_1"], 0.8),
    "SM_YUK_Qovoqcha_Patnis_Mahsulot": ("YUK", ["Qovoqcha_1"], 1.0),
    "SM_YUK_Sarimsoq_Savat_Mahsulot": ("YUK", ["Sarimsoq_1"], 1.0),
    "SM_SM_Uyum_Tarvuz_LOD0": ("UYUM", ["Tarvuz_1"], 1.3),
    "SM_SM_Uyum_Qovun_LOD0": ("UYUM", ["Qovun_1"], 1.0),
}
BOSH_YASHIK = M + "SM_SM_Yashik_LOD0"
TOLDIRUVCHI = bc.ROOT + "/Materials/MI_M_Toldiruvchi"
DEVOR = 2.5     # yashik devori qalinligi, sm


def kvat(rot):
    return rot.quaternion() if hasattr(rot, "quaternion") else unreal.MathLibrary.conv_rotator_to_quaternion(rot)


class Mol:
    """Mesh va uning yotgan holatdagi o'lchami: cho'ziq bo'lsa eng uzun o'qi X ga yotqiziladi."""

    def __init__(self, nom, kop):
        self.sm = unreal.load_asset(MOL + "SM_Mol_" + nom)
        self.mi = self.sm.get_material(0)
        b = self.sm.get_bounding_box()
        self.markaz = (b.min + b.max) * 0.5
        o = [b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z]
        uzun = max(range(3), key=lambda i: o[i])
        self.choziq = o[uzun] > 1.45 * sorted(o)[1]   # baqlajon 13.5/7.8 = 1.73 - yotqiziladi
        # asosiy burilish: uzun o'q -> X (Y bo'lsa yaw 90, Z bo'lsa pitch 90)
        self.asos = unreal.Rotator(0, 0, 0) if not self.choziq or uzun == 0 else \
            (unreal.Rotator(0, 0, 90) if uzun == 1 else unreal.Rotator(0, 90, 0))
        if self.choziq:
            q = sorted(o)
            self.L, self.W, self.H = o[uzun] * kop, q[1] * kop, q[0] * kop
        else:
            d = max(o[0], o[1]) * kop
            self.L = self.W = d
            self.H = o[2] * kop
        self.kop = kop


def uyum(mollar, x0, x1, y0, y1, z0, qavat, rng, piramida=False):
    """Mahalliy (eski mesh) koordinatalarda mol joylari: [(mol, nuqta, Rotator, o'lcham)]"""
    out = []
    m0 = mollar[0]
    dx, dy = m0.L * 0.92, m0.W * 0.92
    for k in range(qavat):
        chet = k * min(dx, dy) * (0.5 if not piramida else 0.55)
        ax, bx, ay, by = x0 + chet + dx / 2, x1 - chet - dx / 2, y0 + chet + dy / 2, y1 - chet - dy / 2
        if ax > bx or ay > by:
            break
        nx, ny = int((bx - ax) // dx) + 1, int((by - ay) // dy) + 1
        sx = (bx - ax) / max(1, nx - 1) if nx > 1 else 0
        sy = (by - ay) / max(1, ny - 1) if ny > 1 else 0
        z = z0 + m0.H * (0.5 + 0.78 * k)
        for i in range(nx):
            for j in range(ny):
                mol = rng.choice(mollar)
                s = mol.kop * rng.uniform(0.9, 1.1)
                x = (ax + i * sx if nx > 1 else (ax + bx) / 2) + rng.uniform(-0.12, 0.12) * dx + (dx * 0.25 if k % 2 else 0)
                y = (ay + j * sy if ny > 1 else (ay + by) / 2) + rng.uniform(-0.12, 0.12) * dy
                if not (x0 <= x <= x1 and y0 <= y <= y1):
                    continue
                if mol.choziq:   # yotqizilgan, o'qi atrofida tasodifiy, yo'nalishi +-15 daraja
                    r = unreal.Rotator(rng.uniform(0, 360), 0, rng.uniform(-15, 15))
                else:            # dumaloq: istalgan tomoni bilan
                    r = unreal.Rotator(rng.uniform(-35, 35), rng.uniform(-35, 35), rng.uniform(0, 360))
                out.append((mol, unreal.Vector(x, y, z + rng.uniform(-0.1, 0.1) * mol.H), r, s))
    return out


def dunyo(T, mol, p, r, s):
    """Eski nusxa transformi T ichidagi mahalliy nuqta -> dunyo; mol o'lchami T masshtabiga bog'liq emas"""
    q = kvat(T.rotation.rotator()) if hasattr(T.rotation, "rotator") else T.rotation
    rq = unreal.MathLibrary.multiply_quat_quat(q, unreal.MathLibrary.multiply_quat_quat(kvat(r), kvat(mol.asos)))
    joy = unreal.MathLibrary.transform_location(T, p)
    # mesh pivot markazda bo'lmasa: bounding box markazi nuqtaga tushsin
    joy = joy - unreal.MathLibrary.quat_rotate_vector(rq, mol.markaz * s)
    return unreal.Transform(joy, unreal.MathLibrary.quat_rotator(rq), unreal.Vector(s, s, s))


# ---- xarita: birinchi marta - zaxira; qayta ishga tushirilsa - zaxiradan (eski mollar bor holat) tiklab, qaytadan teradi.
# Diqqat: zaxiradan keyin xaritaga qilingan boshqa o'zgarishlar ham shu bilan qaytib ketadi - qayta terishni darhol qiling
xarita = os.path.join(LOYIHA, "Content", "Bozor", "Maps", "Bozor.umap")
zax = os.path.join(LOYIHA, "Saved", "Zaxira", "Bozor_mol_uyum_oldi.umap")
os.makedirs(os.path.dirname(zax), exist_ok=True)
if os.path.exists(zax):
    shutil.copy2(zax, xarita)
    log("xarita zaxiradan tiklandi:", zax)
else:
    shutil.copy2(xarita, zax)
unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
guruh = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorMeshGroup) if a.get_actor_label() == "Bozor_Mahsulotlar")
guruh.modify()

bosh = unreal.load_asset(BOSH_YASHIK)
bb = bosh.get_bounding_box()
kub = unreal.load_asset("/Engine/BasicShapes/Cube")
toldir = unreal.load_asset(TOLDIRUVCHI)
mol_kesh = {}
yangi = {}      # mesh yo'li -> (mesh, material, [transform])
qosh = lambda sm, mi, t: yangi.setdefault(sm.get_path_name(), (sm, mi, []))[2].append(t)
jami_eski = 0
for c in guruh.get_components_by_class(unreal.InstancedStaticMeshComponent):
    sm = c.get_editor_property("static_mesh")
    if not sm or sm.get_name() not in ALMASH:
        continue
    tur, nomlar, kop = ALMASH[sm.get_name()]
    mollar = [mol_kesh.setdefault((n, kop), Mol(n, kop)) for n in nomlar]
    b = sm.get_bounding_box()
    n = c.get_instance_count()
    soni = 0
    for i in range(n):
        T = c.get_instance_transform(i, True)
        rng = random.Random(hash((sm.get_name(), i)) & 0xFFFFFFFF)
        if tur == "YASHIK":
            # eski yashik o'rnida bo'sh yashik (masshtabi o'zgarmaydi), ichida to'ldiruvchi va mollar
            qosh(bosh, None, T)
            x0, x1, y0, y1 = bb.min.x + DEVOR, bb.max.x - DEVOR, bb.min.y + DEVOR, bb.max.y - DEVOR
            z0 = bb.max.z - mollar[0].H * 0.9
            tag = unreal.Transform(unreal.MathLibrary.transform_location(T, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, z0 - 0.5)),
                                   T.rotation.rotator() if hasattr(T.rotation, "rotator") else T.rotation,
                                   unreal.Vector((x1 - x0) / 100.0 * T.scale3d.x, (y1 - y0) / 100.0 * T.scale3d.y, 0.01))
            qosh(kub, toldir, tag)
            joy = uyum(mollar, x0, x1, y0, y1, z0, 2, rng)
        elif tur == "YUK":
            joy = uyum(mollar, b.min.x, b.max.x, b.min.y, b.max.y, b.min.z, 2, rng)
        else:
            qavat = max(2, int((b.max.z - b.min.z) / (mollar[0].H * 0.78)))
            joy = uyum(mollar, b.min.x, b.max.x, b.min.y, b.max.y, b.min.z, qavat, rng, piramida=True)
        for mol, p, r, s in joy:
            qosh(mol.sm, mol.mi, dunyo(T, mol, p, r, s))
        soni += len(joy)
    log("%-36s %s  eski %2d nusxa -> %4d mol (%s)" % (sm.get_name(), tur, n, soni, "+".join(nomlar)))
    jami_eski += n
    c.modify()
    c.clear_instances()

for yol, (sm, mi, tlar) in yangi.items():
    guruh.add_instances(sm, [mi] if mi else [], tlar, True, False)
    log("  + %-40s %5d" % (sm.get_name(), len(tlar)))
log("jami: eski %d nusxa almashtirildi, yangi %d nusxa" % (jami_eski, sum(len(v[2]) for v in yangi.values())))
unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Bozor/Maps/Bozor")
log("TAYYOR")
