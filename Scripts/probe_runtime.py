"""O'yin ichida (-game) 15 s kutib NPC'larning AI holatini yozadi va o'yindan chiqadi.
Ishga tushirish: UnrealEditor.exe BozorUnreal.uproject /Game/Bozor/Maps/Bozor -game -ExecCmds="py <shu fayl>"
"""
import time
import unreal

OUT = r"D:\Unreal Projects\BozorUnreal\Saved\probe_runtime.txt"
T0 = time.time()
WAIT = float(__import__("os").environ.get("BOZOR_PROBE_WAIT", "15"))
REBUILD = True
state = {"done": False, "h": None}


def game_world():
    for w in unreal.EditorLevelLibrary.get_game_world(), :
        if w:
            return w
    return None


def probe():
    lines = []
    w = unreal.UnrealEditorSubsystem().get_game_world() if hasattr(unreal, "UnrealEditorSubsystem") else None
    if not w:
        w = unreal.EditorLevelLibrary.get_game_world()
    lines.append("world %s" % (w.get_path_name() if w else None))
    nav = unreal.NavigationSystemV1.get_navigation_system(w)
    lines.append("navsys %s" % nav)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        cn = a.get_class().get_name()
        if "Nav" in cn or "Recast" in cn:
            b = a.get_actor_bounds(False)
            lines.append("navactor %s %s origin=%s extent=%s" % (cn, a.get_name(), b[0], b[1]))
    for p in ((1200, 0, 0), (0, 0, 0), (2000, 300, 0), (-400, 0, 0)):
        try:
            r = unreal.NavigationSystemV1.project_point_to_navigation(w, unreal.Vector(*p), None, None, unreal.Vector(300, 300, 500))
        except Exception as e:
            r = "?%s" % e
        lines.append("project %s -> %s" % (p, r))
    try:
        lines.append("building %s" % nav.is_navigation_being_built(w))
    except Exception as e:
        lines.append("building ? %s" % e)
    chars = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Character)
    for c in chars:
        n = c.get_class().get_name()
        if n not in ("BP_NPC_C", "BP_Customer_C"):
            continue
        ctrl = c.get_controller()
        mv = c.get_editor_property("character_movement")
        loc = c.get_actor_location()
        pf = None
        if ctrl:
            try:
                pf = ctrl.get_move_status()
            except Exception as e:
                pf = "?%s" % e
        proj = None
        try:
            r = unreal.NavigationSystemV1.project_point_to_navigation(w, loc, None, None, unreal.Vector(50, 50, 200))
            proj = r
        except Exception as e:
            proj = "?%s" % e
        extra = ""
        for prop in ("Waypoints", "Target", "SeatBool", "Ducking", "CanIStay", "Run", "HasEnteredQueue"):
            try:
                v = c.get_editor_property(prop)
                if isinstance(v, unreal.Array):
                    v = len(v)
                elif isinstance(v, unreal.Actor):
                    v = v.get_name()
                extra += " %s=%s" % (prop, v)
            except Exception:
                pass
        lines.append("%s ctrl=%s move=%s mode=%s speed=%.0f max=%.0f loc=%s nav=%s%s" % (
            n, ctrl.get_class().get_name() if ctrl else None, pf, mv.movement_mode, c.get_velocity().length(),
            mv.get_editor_property("max_walk_speed"), loc, proj, extra))
    open(OUT, "w", encoding="utf-8").write("\n".join(lines))


def tick(dt):
    if REBUILD and not state.get("rebuilt") and time.time() - T0 > 3:
        state["rebuilt"] = True
        w = unreal.EditorLevelLibrary.get_game_world()
        unreal.SystemLibrary.execute_console_command(w, "RebuildNavigation")
    if state["done"] or time.time() - T0 < WAIT:
        return
    state["done"] = True
    try:
        probe()
    except Exception as e:
        open(OUT, "w", encoding="utf-8").write("xato: %r" % e)
    unreal.SystemLibrary.quit_editor()


state["h"] = unreal.register_slate_post_tick_callback(tick)
