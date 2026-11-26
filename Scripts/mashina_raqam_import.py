"""Raqam atlasini, meshlarni va Bozor xaritasidagi turgan mashinalar raqamlarini saqlaydi."""

from pathlib import Path
import runpy
import stat

import unreal as ue


PROJECT = Path(ue.Paths.project_dir())
SOURCES = PROJECT / "Saved" / "MashinaRaqam"
ROOT = "/Game/Bozor/Mashinalar/Raqamlar"
MAP = "/Game/Bozor/Maps/Bozor"
NAMES = ("Cobalt", "Damas", "Spark", "Nexia", "Malibu")
EAL = ue.EditorAssetLibrary
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
MATERIALS = ue.MaterialEditingLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def load(name):
    path = f"{ROOT}/{name}"
    return ue.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Saqlanmadi: {asset.get_path_name()}")


def atlas():
    task = ue.AssetImportTask()
    task.set_editor_property("filename", str(SOURCES / "T_Raqam_Atlas.png"))
    task.set_editor_property("destination_path", ROOT)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    TOOLS.import_asset_tasks([task])
    texture = load("T_Raqam_Atlas")
    if not texture:
        raise RuntimeError("Raqam atlasi import qilinmadi")
    save(texture)
    return texture


def material(texture):
    name = "M_Raqam_Shaxsiy"
    parent = load(name)
    if parent:
        return parent
    parent = TOOLS.create_asset(name, ROOT, ue.Material, ue.MaterialFactoryNew())
    parent.set_editor_property("used_with_instanced_static_meshes", True)
    coord = MATERIALS.create_material_expression(parent, ue.MaterialExpressionTextureCoordinate, -780, -200)
    index = MATERIALS.create_material_expression(parent, ue.MaterialExpressionScalarParameter, -780, -60)
    index.set_editor_property("parameter_name", "RaqamIndeksi")
    lookup = MATERIALS.create_material_expression(parent, ue.MaterialExpressionCustom, -550, -200)
    lookup.set_editor_property("description", "4 x 8 raqam atlasi")
    lookup.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT2)
    x, y = ue.CustomInput(), ue.CustomInput()
    x.set_editor_property("input_name", "UV")
    y.set_editor_property("input_name", "Index")
    lookup.set_editor_property("inputs", [x, y])
    lookup.set_editor_property("code", """
float Row = floor(Index / 4.0);
float2 Pixel = UV * float2(480.0, 104.0) + float2(16.0, 12.0);
Pixel += float2((Index - Row * 4.0) * 512.0, Row * 128.0);
return Pixel / float2(2048.0, 1024.0);
""")
    MATERIALS.connect_material_expressions(coord, "", lookup, "UV")
    MATERIALS.connect_material_expressions(index, "", lookup, "Index")
    sample = MATERIALS.create_material_expression(parent, ue.MaterialExpressionTextureSample, -300, -200)
    sample.set_editor_property("texture", texture)
    MATERIALS.connect_material_expressions(lookup, "", sample, "UVs")
    MATERIALS.connect_material_property(sample, "RGB", ue.MaterialProperty.MP_BASE_COLOR)
    roughness = MATERIALS.create_material_expression(parent, ue.MaterialExpressionConstant, -270, 80)
    roughness.set_editor_property("r", 0.58)
    MATERIALS.connect_material_property(roughness, "", ue.MaterialProperty.MP_ROUGHNESS)
    MATERIALS.recompile_material(parent)
    save(parent)
    return parent


def mesh_pairs(parent):
    import_glb = runpy.run_path(str(PROJECT / "Scripts" / "qatnov_import.py"))["import_glb"]
    meshes = {}
    for model in NAMES:
        name = f"SM_Raqam_{model}"
        mesh = import_glb(str(SOURCES / f"{name}.glb"), ROOT, name, False)
        if not mesh or mesh.get_num_triangles(0) != 36:
            raise RuntimeError(f"Old va orqa panellar tekshiruvdan o'tmadi: {name}")
        mesh.set_material(0, parent)
        save(mesh)
        meshes[model] = mesh
    return meshes


def parked_material(parent, index):
    name = f"MI_Raqam_{index:02d}"
    asset = load(name) or TOOLS.create_asset(name, ROOT, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
    MATERIALS.set_material_instance_parent(asset, parent)
    MATERIALS.set_material_instance_scalar_parameter_value(asset, "RaqamIndeksi", float(index))
    MATERIALS.update_material_instance(asset)
    save(asset)
    return asset


def parked_cars(meshes, parent):
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split(".")[0] != MAP:
        raise RuntimeError("Bozor xaritasi ochilmagan")
    parked = []
    existing = {}
    for actor in ACTORS.get_all_level_actors():
        for comp in actor.get_components_by_class(ue.InstancedStaticMeshComponent):
            if not comp.static_mesh:
                continue
            name = comp.static_mesh.get_name()
            if name.startswith("SM_Raqam_"):
                existing[comp.get_material(0).get_name()] = comp
            elif name.removeprefix("SM_") in NAMES:
                for i in range(comp.get_instance_count()):
                    transform = comp.get_instance_transform(i, True)
                    parked.append((name.removeprefix("SM_"), transform, actor))
    parked.sort(key=lambda car: (car[0], car[1].translation.x, car[1].translation.y))
    if len(parked) != 9:
        raise RuntimeError(f"Turgan mashinalar soni kutilganidan farq qildi: {len(parked)}")

    with ue.ScopedEditorTransaction("Mashinalarga shaxsiy davlat raqami"):
        for index, (model, transform, actor) in enumerate(parked):
            mesh, variant = meshes[model], parked_material(parent, index)
            component = existing.get(variant.get_name())
            if component:
                component.modify()
                component.set_static_mesh(mesh)
                component.update_instance_transform(0, transform, True, True, True)
            else:
                actor.add_instances(mesh, [variant], [transform], False, False)
    path = PROJECT / "Content" / "Bozor" / "Maps" / "Bozor.umap"
    if not path.stat().st_mode & stat.S_IWRITE:
        path.chmod(stat.S_IREAD | stat.S_IWRITE)
    if not ue.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError("Bozor xaritasi saqlanmadi")
    print(f"{len(parked)} mashinaga old va orqa raqamlar qo'yildi")


if __name__ == "__main__":
    base = material(atlas())
    parked_cars(mesh_pairs(base), base)
