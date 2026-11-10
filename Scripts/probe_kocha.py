"""Bozor oldidagi ko'cha: yo'l, bordyur, yo'lak va mashinalar instansiyalari (dunyo bo'yicha to'rtburchak, sm).
UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/probe_kocha.py
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "probe_kocha.txt")
lines = []
KEYS = ("YOL_", "Bordyur", "Pol_Asfalt", "YER_", "Damas", "Labo", "Darvoza", "Reklama", "Elektr", "Ariq", "Skameyka", "Bino", "Daraxt",
        "Devor", "Panjara", "Chetan")

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_class().get_name() != "BozorMeshGroup":
        continue
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        sm = c.static_mesh
        if not sm or not any(k in sm.get_name() for k in KEYS):
            continue
        bb = sm.get_bounding_box()
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            pts = []
            for x in (bb.min.x, bb.max.x):
                for y in (bb.min.y, bb.max.y):
                    for z in (bb.min.z, bb.max.z):
                        pts.append(t.transform_location(unreal.Vector(x, y, z)))
            lo = [min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)]
            hi = [max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)]
            lines.append("%s %s x[%.0f..%.0f] y[%.0f..%.0f] z[%.0f..%.0f] yaw %.0f" % (a.get_actor_label(), sm.get_name(), lo[0], hi[0], lo[1], hi[1],
                                                                                     lo[2], hi[2], t.rotation.rotator().yaw))
with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
