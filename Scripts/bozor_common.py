"""BozorUnreal import skriptlari uchun umumiy yordamchilar."""
import json
import os
import unreal

EXP = r"D:\Trk\BozorKit_hammasi\Unreal_Export"
ROOT = "/Game/Bozor"
LOG = r"D:\Unreal Projects\BozorUnreal\Saved\bozor_import.txt"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(*a):
    msg = " ".join(str(x) for x in a)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(msg + "\n")
    unreal.log_warning("BOZOR " + msg)


def load_scene():
    with open(os.path.join(EXP, "scene.json"), encoding="utf-8") as f:
        return json.load(f)


def make_pipeline(name, static=True, skeleton=None):
    """Interchange pipeline asset (Python'da faqat asset yo'li orqali uzatiladi)."""
    path = "%s/Pipelines/%s" % (ROOT, name)
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    src = unreal.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline.DefaultAssetsPipeline")
    p = asset_tools.duplicate_asset(name, "%s/Pipelines" % ROOT, src)
    p.set_editor_property("asset_type_sub_folders", False)
    p.set_editor_property("scene_name_sub_folder", False)
    p.material_pipeline.set_editor_property("import_materials", False)
    p.material_pipeline.texture_pipeline.set_editor_property("import_textures", False)
    cm = p.common_meshes_properties
    cm.set_editor_property("recompute_normals", False)
    cm.set_editor_property("import_lods", False)
    mp = p.mesh_pipeline
    mp.set_editor_property("collision", False)
    mp.set_editor_property("create_physics_asset", False)
    mp.set_editor_property("build_nanite", static)
    if static:
        mp.set_editor_property("import_skeletal_meshes", False)
    else:
        mp.set_editor_property("import_static_meshes", False)
        p.animation_pipeline.set_editor_property("import_animations", False)
        if skeleton:
            p.common_skeletal_meshes_and_animations_properties.set_editor_property("skeleton", skeleton)
    eal.save_loaded_asset(p)
    return path


def interchange_import(file, dest, pipeline_path):
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines.append(unreal.SoftObjectPath(pipeline_path))
    return mgr.import_asset(dest, mgr.create_source_data(file), params) or []


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
