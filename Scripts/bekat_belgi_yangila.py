"""Bekat belgisi teksturasini (Scripts/bekat_belgi_chiz.ps1 chizadi) import qiladi va MI_Bekat_Belgi yaratadi.
ABozorBekat (BozorAvtobus.cpp) belgi plitasining ikki yuziga shu materialni qo'yadi. Xaritaga tegmaydi.
Editor'da: Tools > Execute Python Script... yoki
UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/bekat_belgi_yangila.py
"""
import os

import unreal

ROOT = "/Game/Bekat"
PNG = os.path.join(unreal.Paths.project_saved_dir(), "Yozuvlar", "out", "T_Bekat_Belgi.png")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

if not os.path.exists(PNG):
    raise RuntimeError("tekstura yo'q, avval Scripts/bekat_belgi_chiz.ps1: " + PNG)

task = unreal.AssetImportTask()
task.filename = PNG
task.destination_path = ROOT
task.destination_name = "T_Bekat_Belgi"
task.automated = True
task.replace_existing = True
task.save = True
tools.import_asset_tasks([task])
tex = unreal.load_asset(ROOT + "/T_Bekat_Belgi")
tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
tex.set_editor_property("max_texture_size", 1024)
eal.save_loaded_asset(tex)

path = ROOT + "/MI_Bekat_Belgi"
mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
    "MI_Bekat_Belgi", ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property("parent", unreal.load_asset("/Game/Bozor/Materials/M_BozorLit"))
mel.set_material_instance_texture_parameter_value(mi, "BaseMap", tex)
mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(1, 1, 1, 1))
mel.set_material_instance_scalar_parameter_value(mi, "Metallic", 0.0)
mel.set_material_instance_scalar_parameter_value(mi, "Smoothness", 0.45)   # retroreflektiv plyonka - biroz yaltiroq
eal.save_loaded_asset(mi)
unreal.log_warning("BOZOR bekat belgisi: " + path)
