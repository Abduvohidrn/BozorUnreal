"""-game rejimida bino tagi kadrlari: o'yinchini nuqtalarga ko'chiradi va HighResShot oladi, so'ng chiqadi.

UnrealEditor.exe <uproject> [map] -game -windowed -ResX=1600 -ResY=900 -AvtobusYoq -MenyuYoq -AiYoq -SplashYoq
    -ExecutePythonScript="<bu fayl>"   (muhit: BINO_TAGI_PREFIKS=oldin|keyin)
"""
import os
import time

import unreal as ue

PREFIKS = os.environ.get("BINO_TAGI_PREFIKS", "keyin")
OUT = "D:/Trk/BozorKit_hammasi/bino_tagi_skrin"
NUQTALAR = [
    # nom, (x, y, z), yaw, pitch
    ("1_shimol_skameyka", (5250, -1500, 110), 10, -24),
    ("2_shimol_yon", (5600, -2050, 110), 45, -16),
    ("3_shimol_devor", (6250, 2300, 110), 70, -28),
    ("4_janub", (-1350, -2700, 110), 150, -14),
    ("5_toshkent_uy", (-1000, -5600, 110), -135, -10),
]
# 1-o'tish: har nuqtaga borib shader/teksturalarni "isitadi" (kadr yo'q), 2-o'tish: suratga oladi.
NUQTALAR = [("_isitish",) + n[1:] for n in NUQTALAR] + NUQTALAR
ue.EditorPythonScripting.set_keep_python_script_alive(True) if hasattr(ue, "EditorPythonScripting") else None
start = time.monotonic()
holat = {"i": 0, "t": None}


def _world():
    for w in ue.ObjectIterator(ue.World):
        if w.get_name() in ("Bozor", "_BinoTagiOldin") and ue.GameplayStatics.get_player_pawn(w, 0):
            return w
    return None


def tick(dt):
    now = time.monotonic()
    if now - start < 40:
        return
    w = _world()
    if not w:
        return
    i = holat["i"]
    if i >= len(NUQTALAR):
        if now - holat["t"] > 6:
            ue.unregister_slate_post_tick_callback(handle)
            ue.SystemLibrary.execute_console_command(w, "quit")
        return
    if holat["t"] is not None and now - holat["t"] < (10 if NUQTALAR[i][0] == "_isitish" else 4):
        return
    nom, (x, y, z), yaw, pitch = NUQTALAR[i]
    pawn = ue.GameplayStatics.get_player_pawn(w, 0)
    pc = ue.GameplayStatics.get_player_controller(w, 0)
    if not holat.get("kochdi"):
        pawn.set_actor_location(ue.Vector(x, y, z), False, True)
        pc.set_control_rotation(ue.Rotator(roll=0, pitch=pitch, yaw=yaw))
        holat["kochdi"] = True
        holat["t"] = now
        return
    pc.set_control_rotation(ue.Rotator(roll=0, pitch=pitch, yaw=yaw))
    if nom != "_isitish":
        ue.SystemLibrary.execute_console_command(w, f"HighResShot 1600x900 filename={OUT}/{PREFIKS}_{nom}.png")
    ue.log(f"BINO_TAGI_KADR {PREFIKS}_{nom} {pawn.get_actor_location()}")
    holat["kochdi"] = False
    holat["i"] = i + 1
    holat["t"] = now


handle = ue.register_slate_post_tick_callback(tick)
