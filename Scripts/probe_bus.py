"""BP_City_Bus: Blueprint o'zgaruvchilari va hodisalari (Saved/probe_bus.txt)."""
import os

import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
bp = unreal.load_asset("/Game/City_Bus/Blueprints/BP_City_Bus")
bus = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, -50000))
text = unreal.BozorLevelTools.describe_object(bus)
for c in bus.get_components_by_class(unreal.StaticMeshComponent):
    sm = c.static_mesh
    if sm and ("Door_Frame" in sm.get_name() or "Wheel" in sm.get_name()):
        bb = sm.get_bounding_box()
        text += "\nMESH %s %s rel %s bbox %s..%s" % (c.get_name(), sm.get_name(), c.get_relative_transform().translation, bb.min, bb.max)
with open(os.path.join(unreal.Paths.project_saved_dir(), "probe_bus.txt"), "w", encoding="utf-8") as f:
    f.write(text)
