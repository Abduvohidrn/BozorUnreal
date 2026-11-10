"""Bozor xaritasidagi NavMesh: saqlangan holatda bormi va qayta qurilsa bo'ladimi (commandlet'da ishlaydi).
BOZOR_NAV_SAVE=1 bo'lsa, qayta qurilgan NavMesh xarita bilan saqlanadi."""
import os
import unreal

OUT = r"D:\Unreal Projects\BozorUnreal\Saved\probe_nav.txt"
lines = []


def log(*a):
    lines.append(" ".join(str(x) for x in a))


def project(w, p):
    return unreal.NavigationSystemV1.project_point_to_navigation(w, unreal.Vector(*p), None, None, unreal.Vector(100, 100, 300))


PTS = ((1200, 0, 0), (2000, 300, 0), (-400, 0, 0), (1676, -28, 0))
world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
log("saqlangan:", [project(world, p) is not None for p in PTS])
floor = 0
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        if c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION and c.can_character_step_up_on != None:
            floor += 1
log("collision komponentlar:", floor)
hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(1200, 0, 300), unreal.Vector(1200, 0, -300),
                                             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
log("pol trace:", hit is not None and hit.to_tuple()[0], hit.to_tuple()[4] if hit else None, hit.to_tuple()[9] if hit else None)
ok = unreal.BozorLevelTools.build_navigation(world)
log("build:", ok, [project(world, p) is not None for p in PTS])
if os.environ.get("BOZOR_NAV_SAVE") == "1" and ok:
    log("saqlandi:", unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Bozor/Maps/Bozor"))
open(OUT, "w", encoding="utf-8").write("\n".join(lines))
