"""Avtobus intro uchun tahlil: xarita tuzilishi (yer, kirish, o'yinchi) va BP_City_Bus komponentlari.
UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/probe_avtobus.py
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "probe_avtobus.txt")
lines = []


def log(*a):
    lines.append(" ".join(str(x) for x in a))


def v(x):
    return "(%.0f, %.0f, %.0f)" % (x.x, x.y, x.z)


world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in actors:
    cls = a.get_class().get_name()
    if cls in ("BozorNpc",):
        continue
    o, e = a.get_actor_bounds(False)
    log("ACTOR", a.get_actor_label(), cls, "loc", v(a.get_actor_location()), "rot", a.get_actor_rotation(), "bounds", v(o), "ext", v(e),
        "folder", a.get_folder_path())
    if cls == "BozorMeshGroup":
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            n = c.get_instance_count()
            if n == 0:
                continue
            bo, be, _ = unreal.SystemLibrary.get_component_bounds(c)
            log("   ISM", c.static_mesh.get_name() if c.static_mesh else None, n, "ext", v(be), "o", v(bo))
    if cls == "BozorNpcManager":
        log("   gates", [v(p) for p in a.get_editor_property("gate_points")])
    if cls == "BozorFonZonalar":
        log("   area", a.get_editor_property("area"))

# BP_City_Bus
bp = unreal.load_asset("/Game/City_Bus/Blueprints/BP_City_Bus")
log("BP", bp, bp.generated_class() if bp else None)
if bp:
    bus = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, -50000))
    o, e = bus.get_actor_bounds(False)
    log("BUS bounds", v(o), v(e), "root", bus.root_component)
    for c in bus.get_components_by_class(unreal.SceneComponent):
        extra = ""
        if isinstance(c, unreal.StaticMeshComponent):
            extra = c.static_mesh.get_name() if c.static_mesh else "-"
        if isinstance(c, unreal.SkeletalMeshComponent):
            extra = "SK %s anim %s sim %s" % (c.get_skeletal_mesh_asset(), c.get_editor_property("anim_class"), c.is_simulating_physics())
        p = c.get_attach_parent()
        t = c.get_relative_transform()
        log("  C", c.get_name(), c.get_class().get_name(), "parent", p.get_name() if p else None, "rel", v(t.translation),
            t.rotation.rotator(), "vis", c.is_visible(), extra)
    for c in bus.get_components_by_class(unreal.ActorComponent):
        if not isinstance(c, unreal.SceneComponent):
            log("  AC", c.get_name(), c.get_class().get_name())
    cdo = unreal.get_default_object(bp.generated_class())
    for name in dir(cdo):
        if name.startswith("_"):
            continue
        try:
            val = cdo.get_editor_property(name)
        except Exception:
            continue
        log("  P", name, str(val)[:160])
    sk = unreal.load_asset("/Game/City_Bus/Meshes/Skeletal/SK_Bus")
    if sk:
        log("SK_Bus bones", [str(sk.get_bone_name(i)) if hasattr(sk, "get_bone_name") else "" for i in range(0)])
for p in ("Presets_Exterior", "Presets_Interior"):
    a = unreal.load_asset("/Game/City_Bus/Blueprints/Additional/" + p)
    log("PRESET", p, a, a.get_class().get_name() if a else None)
    if isinstance(a, unreal.DataTable):
        log("   rows", unreal.DataTableFunctionLibrary.get_data_table_row_names(a))
        log("   csv", unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(a)[:3000] if hasattr(unreal.DataTableFunctionLibrary, "export_data_table_to_csv_string") else "")

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

