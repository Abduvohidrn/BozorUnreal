"""Citizen kiyimlarini sinash: osmonda (z=20000) har bir tur/rol uchun bitta ABozorNpc qatori (_CitizenKorik papkasi)."""
import importlib
import sys

import unreal

sys.path.append(r"D:/Unreal Projects/BozorUnreal/Scripts")
import bozor_npc as b
importlib.reload(b)

TESTS = [("Sotuvchi_Jilet_01", "Sotuvchi"), ("Erkak_Kurtka_01", "Xaridor"), ("Yigit_01", "Xaridor"), ("Hammol_01", "Hammol"),
         ("Otaxon_Pidjak_01", "Xaridor"), ("Otaxon_Pidjak_01", "Sotuvchi"), ("Sotuvchi_Ayol_01", "Sotuvchi"),
         ("Ayol_Kofta_01", "Xaridor"), ("Momo_01", "Xaridor"), ("Qiz_01", "Xaridor")]


def run(variant=0, tests=None):
    acts = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in acts.get_all_level_actors():
        if str(a.get_folder_path()) == "_CitizenKorik":
            acts.destroy_actor(a)
    mh_abp = b.load(b.MH_ABP).generated_class()
    for k, (ch, role) in enumerate(tests or TESTS):
        a = acts.spawn_actor_from_class(unreal.BozorNpc, unreal.Vector(0, k * 80, 20000), unreal.Rotator(0, 0, 0))
        a.set_folder_path("_CitizenKorik")
        a.set_actor_label("T_%s_%s" % (ch, role))
        m = a.get_editor_property("mesh")
        mh = b.mh_parts(ch, role)
        cit = b.cit_parts(ch, role, variant)
        m.set_editor_property("skeletal_mesh_asset", mh[0])
        m.set_editor_property("anim_class", mh_abp)
        b.cit_dress(a, m, *cit)
        a.set_actor_location(unreal.Vector(0, k * 80, 20000), False, False)
    return len(tests or TESTS)
