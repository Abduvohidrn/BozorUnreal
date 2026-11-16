"""Savdo animatsiyalarini o'lchaydi (faqat o'qiydi): Saved/savdo_anim.txt
  1) NPC menejerining umumiy klip ro'yxatida savdo kategoriyalari (V_Weigh, V_Handout, V_Offer ...) qaysi klipga ulangan
  2) uzatish/tortish kliplarida o'ng qo'l (hand_r) har 0.1 s da qayerda (ildizga nisbatan) va eng oldinga cho'zilgan payt
Qo'l holati suyak zanjiri bo'yicha (pelvis -> ... -> hand_r) mahalliy transformlardan yig'iladi.
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/savdo_anim_tekshir.py
"""
import os
import unreal

LOYIHA = os.path.abspath(unreal.Paths.project_dir())
chiq = []
KATEG = ("V_Greet", "V_Show", "V_Yes", "V_No", "V_Offer", "V_Handout", "V_Weigh", "V_Thanks", "V_Listen", "V_Talk")

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mgr = next(iter(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorNpcManager)), None)
if mgr:
    for a in mgr.get_editor_property("actions"):
        k = str(a.get_editor_property("category"))
        if k in KATEG:
            an = a.get_editor_property("anim")
            chiq.append("menejer %-10s -> %s (loop %s)" % (k, an.get_name() if an else None, a.get_editor_property("loop")))
else:
    chiq.append("NPC menejeri topilmadi")

ZANJIR = ["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"]


def qol(anim, t):
    tr = unreal.Transform()
    for b in ZANJIR:
        try:
            m = unreal.AnimationLibrary.get_bone_pose_for_time(anim, b, t, False)
        except Exception:
            continue
        tr = unreal.MathLibrary.compose_transforms(m, tr)
    return tr.translation


import glob
for nom in ("AnimationSequence_Cashier_Handout", "AnimationSequence_Server_PlaceFoodOnTable_RightSide",
            "AnimationSequence_Chef_WorkStation_Inspect_LowHeight"):
    yol = next((p for p in unreal.EditorAssetLibrary.list_assets("/Game", recursive=True) if p.split(".")[-1] == nom), None)
    if not yol:
        chiq.append("%s: topilmadi" % nom)
        continue
    anim = unreal.load_asset(yol)
    L = anim.get_play_length()
    chiq.append("\n%s (%.2f s) %s" % (nom, L, yol))
    eng = (-1e9, 0.0)
    t = 0.0
    while t <= L + 1e-3:
        p = qol(anim, t)
        # oldinga cho'zilish: ildizdan gorizontal uzoqlik
        uz = (p.x ** 2 + p.y ** 2) ** 0.5
        if uz > eng[0]:
            eng = (uz, t)
        chiq.append("  t=%.1f  qo'l=(%.0f, %.0f, %.0f)  gorizontal %.0f sm" % (t, p.x, p.y, p.z, uz))
        t += 0.1
    chiq.append("  ENG CHO'ZILGAN: t=%.2f s (%.0f%%), %.0f sm" % (eng[1], 100 * eng[1] / L, eng[0]))
open(os.path.join(LOYIHA, "Saved", "savdo_anim.txt"), "w", encoding="utf-8").write("\n".join(chiq))
