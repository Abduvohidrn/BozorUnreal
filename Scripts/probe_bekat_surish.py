"""Sinov: bekat qismini (Ustun) surib, OnConstruction qayta chaqirilgach joyida qolishini tekshiradi. Xaritani saqlamaydi.
UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/probe_bekat_surish.py  -> Saved/probe_bekat_surish.txt
"""
import os

import unreal

lines = []
unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
b = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label() == "Bekat_DehqonBozori"][0]
lines.append("avto_joylash (yuklangach) %s" % b.get_editor_property("avto_joylash"))
u = b.ustun
oldin = u.get_relative_transform().translation
u.set_relative_location(oldin + unreal.Vector(0, 150, 0), False, False)
unreal.BozorLevelTools.rerun_construction(b)
keyin = u.get_relative_transform().translation
lines.append("oldin %s" % oldin)
lines.append("surildi +150 Y, qayta qurilgach %s" % keyin)
lines.append("NATIJA: %s" % ("joyida qoldi" if abs(keyin.y - oldin.y - 150) < 1 else "QAYTIB KETDI"))
with open(os.path.join(unreal.Paths.project_saved_dir(), "probe_bekat_surish.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
