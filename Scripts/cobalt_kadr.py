"""Sahnadagi Cobalt mashinalarini muharrirda suratga oladi (Saved/Screenshots/Cobalt/*.png), keyin muharrirni yopadi.
UnrealEditor.exe BozorUnreal.uproject -Unattended "-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:+StartupScripts=<to'liq yo'l>/cobalt_kadr.py"
(-ExecutePythonScript skript tugashi bilan muharrirni yopadi - tick'lar ishlamaydi)
Kameralar vaqtinchalik - sahna saqlanmaydi.
"""
import math
import os

import unreal as ue

V = ue.Vector
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
OUT = os.path.join(PROJ, "Saved", "Screenshots", "Cobalt")
os.makedirs(OUT, exist_ok=True)
ACT = ue.get_editor_subsystem(ue.EditorActorSubsystem)

kadrlar = []


def tayyorla():
    ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level("/Game/Bozor/Maps/Bozor")
    mashinalar = []
    for a in ACT.get_all_level_actors():
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            if c.static_mesh and c.static_mesh.get_name() == "SM_Cobalt":
                for i in range(c.get_instance_count()):
                    mashinalar.append(c.get_instance_transform(i, True))

    kadrlar.clear()
    for n, t in enumerate(mashinalar):
        markaz = t.translation + V(0, 0, 70)
        yaw = t.rotation.rotator().yaw
        for k, (burchak, masofa, h) in enumerate(((180 + 35, 650, 230), (-50, 520, 160))):
            r = math.radians(yaw + burchak)   # Cobalt old tomoni lokal -X
            pos = markaz + V(math.cos(r) * masofa, math.sin(r) * masofa, h)
            rot = ue.MathLibrary.find_look_at_rotation(pos, markaz)
            cam = ACT.spawn_actor_from_class(ue.CameraActor, pos, rot)
            cam.camera_component.set_editor_property("field_of_view", 60.0)
            kadrlar.append((cam, os.path.join(OUT, "cobalt_%d_%d.png" % (n, k))))
    ue.log_warning("COBALT_KADR mashinalar %d, kadrlar %d" % (len(mashinalar), len(kadrlar)))


holat = {"tick": 0, "i": 0}


def tick(dt):
    holat["tick"] += 1
    t = holat["tick"]
    if t == 30:
        tayyorla()
    if t < 400:   # shader va Nanite oqimi tayyor bo'lsin
        return
    i = holat["i"]
    if i < len(kadrlar) and t % 60 == 0:
        cam, fayl = kadrlar[i]
        ue.AutomationLibrary.take_high_res_screenshot(1280, 720, fayl, cam)
        holat["i"] += 1
    elif i >= len(kadrlar) and t > 400 + 60 * (len(kadrlar) + 3):
        ue.unregister_slate_post_tick_callback(h)
        for cam, _ in kadrlar:
            cam.destroy_actor()
        ue.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")


h = ue.register_slate_post_tick_callback(tick)
