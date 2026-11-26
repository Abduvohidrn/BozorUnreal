"""Damas'ni muharrirda suratga oladi (Saved/Screenshots/Damas/*.png), keyin muharrirni yopadi. Sahna saqlanmaydi.
Sahnada SM_Damas nusxalari bo'lsa - ularni, bo'lmasa vaqtinchalik bitta Damas'ni (shimoliy yo'l, Cobalt yonida) suratga oladi.
UnrealEditor.exe BozorUnreal.uproject -Unattended "-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:+StartupScripts=<to'liq yo'l>/damas_kadr.py"
"""
import math
import os

import unreal as ue

V = ue.Vector
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
OUT = os.path.join(PROJ, "Saved", "Screenshots", "Damas")
os.makedirs(OUT, exist_ok=True)
ACT = ue.get_editor_subsystem(ue.EditorActorSubsystem)
kadrlar, vaqtinchalik = [], []


def tayyorla():
    ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level("/Game/Bozor/Maps/Bozor")
    joylar = []
    for a in ACT.get_all_level_actors():
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            if c.static_mesh and c.static_mesh.get_name() == "SM_Damas":
                joylar += [c.get_instance_transform(i, True) for i in range(c.get_instance_count())]
    if not joylar:
        sm = ue.load_asset("/Game/Bozor/Mashinalar/Damas/SM_Damas")
        a = ACT.spawn_actor_from_object(sm, V(1400, 4530, 0), ue.Rotator(0, 0, 90))
        vaqtinchalik.append(a)
        joylar.append(a.get_actor_transform())
    for n, t in enumerate(joylar):
        markaz = t.translation + V(0, 0, 90)
        yaw = t.rotation.rotator().yaw   # Damas old tomoni lokal -Y => dunyoda yaw-90
        old = yaw - 90
        for k, (burchak, masofa, h) in enumerate(((90, 600, 80), (-90, 600, 80), (35, 650, 220), (180 + 30, 650, 200))):
            r = math.radians(old + burchak)
            pos = markaz + V(math.cos(r) * masofa, math.sin(r) * masofa, h)
            cam = ACT.spawn_actor_from_class(ue.CameraActor, pos, ue.MathLibrary.find_look_at_rotation(pos, markaz))
            cam.camera_component.set_editor_property("field_of_view", 60.0)
            vaqtinchalik.append(cam)
            kadrlar.append((cam, os.path.join(OUT, "damas_%d_%d.png" % (n, k))))
    ue.log_warning("DAMAS_KADR joylar %d, kadrlar %d" % (len(joylar), len(kadrlar)))


holat = {"tick": 0, "i": 0}


def tick(dt):
    holat["tick"] += 1
    t = holat["tick"]
    if t == 30:
        tayyorla()
    if t < 900:   # shader'lar tayyor bo'lsin
        return
    i = holat["i"]
    if i < len(kadrlar) and t % 60 == 0:
        cam, fayl = kadrlar[i]
        ue.AutomationLibrary.take_high_res_screenshot(1280, 720, fayl, cam)
        holat["i"] += 1
    elif i >= len(kadrlar) and t > 900 + 60 * (len(kadrlar) + 3):
        ue.unregister_slate_post_tick_callback(h)
        for a in vaqtinchalik:
            a.destroy_actor()
        ue.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")


# startup skript ikki marta bajarilishi mumkin - ikkinchi load_level birinchi kameralarni yo'q qiladi
if not getattr(ue, "_damas_kadr", False):
    setattr(ue, "_damas_kadr", True)
    h = ue.register_slate_post_tick_callback(tick)
