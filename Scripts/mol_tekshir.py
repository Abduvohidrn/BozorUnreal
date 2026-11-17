"""Bozor xaritasidagi mol mesh'larini sanaydi (faqat o'qiydi, hech narsa saqlamaydi).

Har mesh uchun: nechta komponent/instance, qaysi aktorlarda, bir dona o'lchami (sm) va material(lar).
Natija: Saved/mol_tekshir.txt. Mol kalitlari sotuv.json "kalitlar"idan + yashik/yuk nomlari.
Ishga tushirish (muharrir ochiq bo'lsa ham bo'ladi):
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/mol_tekshir.py
"""
import json
import os
import re
from collections import defaultdict

import unreal

LOYIHA = os.path.abspath(unreal.Paths.project_dir())
CHIQ = os.path.join(LOYIHA, "Saved", "mol_tekshir.txt")
with open(os.path.join(LOYIHA, "Content", "Bozor", "Data", "sotuv.json"), encoding="utf-8") as f:
    d = json.load(f)
kalit = {k.lower(): m["id"] for m in d["mahsulotlar"] for k in m["kalitlar"]}
kalit.update({k: k for k in ("yashik", "yuk_", "_mahsulot", "cucumber", "eggplant", "zuchini", "tomato", "vegetable_box")})

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
world = unreal.EditorLevelLibrary.get_editor_world()
mesh = defaultdict(lambda: {"komp": 0, "inst": 0, "aktor": set(), "mat": set(), "mol": ""})
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        sm = c.get_editor_property("static_mesh")
        if not sm:
            continue
        nom = sm.get_name().lower()
        mol = next((v for k, v in kalit.items() if k in nom), None)
        if not mol:
            continue
        m = mesh[sm.get_path_name()]
        m["mol"] = mol
        m["komp"] += 1
        if isinstance(c, unreal.InstancedStaticMeshComponent):
            m["inst"] += c.get_instance_count()
        m["aktor"].add(a.get_actor_label())
        for i in range(c.get_num_materials()):
            mt = c.get_material(i)
            if mt:
                m["mat"].add(mt.get_name())

with open(CHIQ, "w", encoding="utf-8") as f:
    for p, m in sorted(mesh.items(), key=lambda x: (x[1]["mol"], x[0])):
        sm = unreal.load_asset(p)
        b = sm.get_bounding_box()
        o = b.max - b.min
        f.write("%-12s %-60s komp=%-3d inst=%-4d o'lcham=%.0fx%.0fx%.0f sm  aktorlar=%d (%s)  mat=%s\n" % (
            m["mol"], p.split(".")[0], m["komp"], m["inst"], o.x, o.y, o.z, len(m["aktor"]),
            ", ".join(sorted(m["aktor"])[:3]), ", ".join(sorted(m["mat"])[:3])))
unreal.log_warning("MOL_TEKSHIR tayyor: %d mesh -> %s" % (len(mesh), CHIQ))
