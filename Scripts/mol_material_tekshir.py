"""Mavjud Megascans mol materiali (MI_M_vewlad1iw - behi) qanday tuzilganini yozadi (faqat o'qiydi): Saved/mol_material.txt"""
import os
import unreal

LOYIHA = os.path.abspath(unreal.Paths.project_dir())
mel = unreal.MaterialEditingLibrary
qator = []
for nom in ("MI_M_vewlad1iw", "MI_M_vdsicd0jw", "MI_Mat_Olma"):
    mi = unreal.load_asset("/Game/Bozor/Materials/" + nom)
    if not mi:
        qator.append("%s: yo'q" % nom)
        continue
    ota = mi.get_editor_property("parent")
    qator.append("%s -> ota: %s" % (nom, ota.get_path_name() if ota else None))
    for p in mel.get_texture_parameter_names(mi):
        t = mel.get_material_instance_texture_parameter_value(mi, p)
        qator.append("   tex %s = %s" % (p, t.get_path_name() if t else None))
    for p in mel.get_scalar_parameter_names(mi):
        qator.append("   scalar %s = %s" % (p, mel.get_material_instance_scalar_parameter_value(mi, p)))
    for p in mel.get_vector_parameter_names(mi):
        qator.append("   vector %s = %s" % (p, mel.get_material_instance_vector_parameter_value(mi, p)))
t = unreal.load_asset("/Game/Bozor/Textures/T_vewlad1iw_2K_Normal")
if t:
    qator.append("normal: srgb=%s compression=%s flip_green=%s max=%s" % (t.get_editor_property("srgb"), t.get_editor_property("compression_settings"),
                                                                      t.get_editor_property("flip_green_channel"), t.get_editor_property("max_texture_size")))
sm = unreal.load_asset("/Game/Bozor/Meshes/SM_YUK_Behi_Yashik_Mahsulot")
if sm:
    nan = sm.get_editor_property("nanite_settings")
    qator.append("SM_YUK_Behi: nanite=%s, uchburchak LOD0=%s" % (nan.get_editor_property("enabled"), sm.get_num_triangles(0)))
open(os.path.join(LOYIHA, "Saved", "mol_material.txt"), "w", encoding="utf-8").write("\n".join(qator))
