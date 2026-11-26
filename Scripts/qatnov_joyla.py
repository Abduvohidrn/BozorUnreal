"""Bozor ko'chasiga mashinalar qatnovi (ABozorQatnov) aktorini qo'yadi/yangilaydi va sahnani saqlaydi.

Ko'cha: x -1400..-400 (10 m), y -3600..3600; janub va shimol uchlarida ko'ndalang yo'llar (y ~ -4700 va ~4700).
O'ng tomonlama harakat: g'arbiy bo'lak (x -1150) shimolga, sharqiy bo'lak (x -650) janubga.
 Bo'laklar (YOLAKLAR) - quyida; burilishlar yoy bilan silliqlanadi.
Piyodalar o'tish joyi - Zebra_* aktorlari (y = 0) qamrab olgan quti.

UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/qatnov_joyla.py   (muharrir yopiq)
"""
import json
import os
import stat

import unreal as ue

V = ue.Vector
MAP = "/Game/Bozor/Maps/Bozor"
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
M = "/Game/Bozor/Mashinalar/"
# (kuzov, oyna, old tomoni lokal yaw, ulush) - Toshkent ko'chalarida Cobalt/Nexia/Spark ko'p
TURLAR = [
    ("Cobalt/SM_Cobalt", "Cobalt/SM_Cobalt_Oyna", 180.0, 3.0),
    ("Nexia/SM_Nexia", "Nexia/SM_Nexia_Oyna", 90.0, 3.0),
    ("Spark/SM_Spark", "Spark/SM_Spark_Oyna", 90.0, 2.5),
    ("Damas/SM_Damas", None, -90.0, 1.5),   # Damas oynalari o'z mesh'ida (shaffof material)
    ("Malibu/SM_Malibu", "Malibu/SM_Malibu_Oyna", 90.0, 0.7),
    ("BMW/SM_BMW", "BMW/SM_BMW_Oyna", 90.0, 0.4),   # "01 S 100 SS" - qatnovda bir vaqtda bittadan ko'p emas
]
# Bo'laklar - burchak nuqtalari va burilish radiusi (sm). O'ng tomonlama harakat:
#  A: janubiy yo'lning janubiy yarmi (g'arbga) -> chapga burilib ko'chaning g'arbiy bo'lagi (shimolga) -> chapga burilib
#     shimoliy yo'lning shimoliy yarmi (sharqqa). Avtobus (intro) ham shu bo'lakdan yuradi.
#  B: shimoliy yo'lning janubiy yarmi (g'arbga) -> o'ngga burilib ko'chaning sharqiy bo'lagi (janubga) -> chapga burilib
#     janubiy yo'lning shimoliy yarmi (sharqqa).
# Yo'l chetida turgan mashinalar x >= 3600 da shimoliy yo'lning janubiy va janubiy yo'lning shimoliy yarmida
# (Scripts/mashina_tuzat.py): A ular yonidan o'tadi (boshqa yarimda, x 8000 gacha), B ulardan oldin (x 3300) tugaydi.
YOLAKLAR = [
    ([(8000, -4850), (-1150, -4850), (-1150, 4880), (8000, 4880)], 600, 6, True),
    ([(3300, 4550), (-650, 4550), (-650, -4550), (3300, -4550)], 350, 4, False),
]
QADAM = 50.0   # nuqtalar oralig'i: zich nuqtalarda splayn avtomatik tangentlari halqa/silkinish bermaydi


def silliq_yol(burchaklar, radius):
    """Siniq chiziq burchaklarini yoy (kvadratik Bezye) bilan yumaloqlab, har QADAM sm da nuqta."""
    P = [ue.Vector(x, y, 0) for x, y in burchaklar]
    bolaklar = []   # (boshi, nazorat, oxiri) - to'g'ri qism uchun nazorat = None
    joriy = P[0]
    for i in range(1, len(P) - 1):
        kir = (P[i] - P[i - 1]).normal()
        chiq = (P[i + 1] - P[i]).normal()
        r = min(radius, 0.45 * (P[i] - P[i - 1]).length(), 0.45 * (P[i + 1] - P[i]).length())
        t1, t2 = P[i] - kir * r, P[i] + chiq * r
        bolaklar.append((joriy, None, t1))
        bolaklar.append((t1, P[i], t2))
        joriy = t2
    bolaklar.append((joriy, None, P[-1]))
    out = [P[0]]
    for a, c, b in bolaklar:
        if c is None:
            n = max(1, int((b - a).length() / QADAM))
            out += [a + (b - a) * (k / n) for k in range(1, n + 1)]
        else:
            uz = (c - a).length() + (b - c).length()
            n = max(2, int(uz / QADAM))
            for k in range(1, n + 1):
                t = k / n
                out.append(a * ((1 - t) ** 2) + c * (2 * (1 - t) * t) + b * (t * t))
    return out


def main():
    ue.EditorLoadingAndSavingUtils.load_map(MAP)
    act = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    actors = act.get_all_level_actors()
    qa = next((a for a in actors if a.get_class().get_name() == "BozorQatnov"), None)
    if not qa:
        qa = act.spawn_actor_from_class(ue.BozorQatnov, V(-900, 0, 0), ue.Rotator())
        qa.set_actor_label("Bozor_Qatnov")
        qa.set_folder_path("Bozor")

    # g'ildiraksiz kuzov va aylanadigan g'ildiraklar (Scripts/qatnov_import.py) bo'lsa - shular
    jf = os.path.join(PROJ, "Saved", "qatnov_mashinalar.json")
    qatnov = json.load(open(jf, encoding="utf-8")) if os.path.exists(jf) else {}
    turlar = []
    for kuzov, oyna, yaw, ulush in TURLAR:
        t = ue.BozorMashinaTuri()
        q = qatnov.get(kuzov.split("/")[0])
        t.set_editor_property("kuzov", ue.load_asset(q["kuzov"] if q else M + kuzov))
        t.set_editor_property("oyna", ue.load_asset(M + oyna) if oyna else None)
        t.set_editor_property("old_yaw", yaw)
        t.set_editor_property("ulush", ulush)
        if q:
            t.set_editor_property("gildiraklar", [ue.load_asset(g) for g in q["gildiraklar"]])
            t.set_editor_property("gildirak_joylari", [V(*j) for j in q["joylar"]])
            t.set_editor_property("gildirak_radius", q["radius"])
        turlar.append(t)
    # ovozlar: City_Bus paketidagi dizel dvigatel (pitch bilan yengil mashina), signal va masofa bo'yicha so'nish
    qa.set_editor_property("dvigatel_ovozi", ue.load_asset("/Game/City_Bus/Sound/S_Engine_Diesel_1000"))
    qa.set_editor_property("signal_ovozi", ue.load_asset("/Game/City_Bus/Sound/S_Horn"))
    qa.set_editor_property("ovoz_sonishi", ue.load_asset("/Game/City_Bus/Sound/ATT_Engine"))
    yolaklar = []
    for burchaklar, radius, soni, avtobus in YOLAKLAR:
        y = ue.BozorYolak()
        nuqtalar = silliq_yol(burchaklar, radius)
        ue.log_warning("QATNOV bo'lak: %d nuqta" % len(nuqtalar))
        y.set_editor_property("nuqtalar", nuqtalar)
        y.set_editor_property("max_mashina", soni)
        y.set_editor_property("avtobus_yoli", avtobus)
        yolaklar.append(y)
    # zebra: Zebra_* aktorlari chegarasi
    zebra = [a for a in actors if a.get_actor_label().startswith("Zebra_")]
    lo = [1e9, 1e9]
    hi = [-1e9, -1e9]
    for z in zebra:
        o, e = z.get_actor_bounds(False)
        lo = [min(lo[0], o.x - e.x), min(lo[1], o.y - e.y)]
        hi = [max(hi[0], o.x + e.x), max(hi[1], o.y + e.y)]
    otish = ue.BozorOtishJoyi()
    otish.set_editor_property("markaz", V((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, 0))
    otish.set_editor_property("yarim", V((hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2, 150))
    qa.set_editor_property("turlar", turlar)
    qa.set_editor_property("yolaklar", yolaklar)
    qa.set_editor_property("otish_joylari", [otish])
    ue.log_warning("QATNOV zebra %d ta: markaz (%d,%d) yarim (%d,%d)" % (len(zebra), (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2,
                                                                       (hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2))
    umap = os.path.join(PROJ, "Content", "Bozor", "Maps", "Bozor.umap")
    os.chmod(umap, stat.S_IREAD | stat.S_IWRITE)   # git LFS 'lockable'
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    ue.log_warning("QATNOV saqlandi: %s" % ue.EditorLoadingAndSavingUtils.save_map(world, MAP))


main()
