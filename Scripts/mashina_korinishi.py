"""Bozordagi mashinalarning materiallari va Nanite'siz ko'rinishini sozlaydi."""

from pathlib import Path
import stat

import unreal as ue


ROOT = "/Game/Bozor/Mashinalar"
EDITOR = ue.EditorAssetLibrary
MATERIAL = ue.MaterialEditingLibrary
PROPERTY = ue.MaterialProperty
STATIC_MESH = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)

PAINT = {
    "Spark/MIC_Spark_carpaint": ((0.69, 0.72, 0.72), 0.0, 0.24, 0.64),
    "Nexia/MIC_Nexia_carpaint": ((0.36, 0.38, 0.41), 0.42, 0.28, 0.65),
    "Malibu/MIC_Malibu_carpaint_mat": ((0.022, 0.025, 0.030), 0.28, 0.24, 0.7),
}


def load(name):
    asset = ue.load_asset(f"{ROOT}/{name}")
    if not asset:
        raise RuntimeError(f"Asset topilmadi: {name}")
    return asset


def save(asset):
    content = Path(ue.Paths.project_content_dir())
    package = asset.get_path_name().split(".", 1)[0]
    path = content / (package.removeprefix("/Game/") + ".uasset")
    if path.exists() and not path.stat().st_mode & stat.S_IWRITE:
        path.chmod(stat.S_IREAD | stat.S_IWRITE)
    if not EDITOR.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Saqlanmadi: {package}")


def paint_materials():
    for name, (color, metallic, roughness, specular) in PAINT.items():
        material = load(name)
        material.modify()
        MATERIAL.set_material_instance_vector_parameter_value(material, "Rang", ue.LinearColor(*color, 1))
        for parameter, value in (("Metallik", metallic), ("Gadir", roughness), ("Spekulyar", specular)):
            MATERIAL.set_material_instance_scalar_parameter_value(material, parameter, value)
        MATERIAL.update_material_instance(material)
        save(material)


def glass_and_lights():
    glass = load("Cobalt/M_Cobalt_Oyna")
    glass.modify()
    opacity = MATERIAL.get_material_property_input_node(glass, PROPERTY.MP_OPACITY)
    opacity.set_editor_property("const_a", 0.32)
    opacity.set_editor_property("const_b", 0.78)
    MATERIAL.get_material_property_input_node(glass, PROPERTY.MP_ROUGHNESS).set_editor_property("r", 0.075)
    MATERIAL.get_material_property_input_node(glass, PROPERTY.MP_SPECULAR).set_editor_property("r", 0.68)
    MATERIAL.recompile_material(glass)
    save(glass)

    # The clear cap covers the reflectors and the orange and red segments.
    lens = load("Spark/M_Spark_Chiroq")
    lens.modify()
    for prop, value in ((PROPERTY.MP_OPACITY, 0.075), (PROPERTY.MP_SPECULAR, 0.7), (PROPERTY.MP_ROUGHNESS, 0.075)):
        MATERIAL.get_material_property_input_node(lens, prop).set_editor_property("r", value)
    MATERIAL.recompile_material(lens)
    save(lens)

    red = load("Spark/MI_Spark_redglass")
    red.modify()
    color = MATERIAL.get_material_property_input_node(red, PROPERTY.MP_BASE_COLOR)
    if not isinstance(color, ue.MaterialExpressionConstant3Vector):
        color = MATERIAL.create_material_expression(red, ue.MaterialExpressionConstant3Vector, -340, 160)
        MATERIAL.connect_material_property(color, "", PROPERTY.MP_BASE_COLOR)
    color.set_editor_property("constant", ue.LinearColor(0.28, 0.009, 0.018, 1))
    roughness = MATERIAL.get_material_property_input_node(red, PROPERTY.MP_ROUGHNESS)
    if not isinstance(roughness, ue.MaterialExpressionConstant):
        roughness = MATERIAL.create_material_expression(red, ue.MaterialExpressionConstant, -340, 240)
        MATERIAL.connect_material_property(roughness, "", PROPERTY.MP_ROUGHNESS)
    roughness.set_editor_property("r", 0.38)
    MATERIAL.recompile_material(red)
    save(red)


def damas_stripe():
    material = load("Damas/M_Damas_Boyoq")
    material.modify()
    node = MATERIAL.get_material_property_input_node(material, PROPERTY.MP_BASE_COLOR)
    code = node.get_editor_property("code")
    code = code.replace("float3(0.42, 0.004, 0.008)", "float3(0.72, 0.18, 0.026)")
    code = code.replace("float3(0.008, 0.04, 0.32)", "float3(0.019, 0.024, 0.031)")
    node.set_editor_property("code", code)
    MATERIAL.recompile_material(material)
    save(material)


def fallback_meshes(names=("Cobalt", "Spark", "Nexia", "Malibu")):
    # Bozor_Yengil starts in DX11. A lower fallback error keeps lamp housings,
    # wheel spokes and panel seams visible on the non-Nanite rendering path.
    for name in names:
        meshes = [(f"{name}/SM_{name}", 0.17), (f"{name}/Qatnov/SM_{name}_Qatnov", 0.17)]
        meshes.extend((f"{name}/Qatnov/SM_{name}_G{i}", 0.25) for i in range(4))
        for path, error in meshes:
            mesh = load(path)
            settings = STATIC_MESH.get_nanite_settings(mesh)
            if not settings.enabled:
                raise RuntimeError(f"Nanite o'chirilgan: {path}")
            target = ue.NaniteFallbackTarget.RELATIVE_ERROR
            before = mesh.get_num_triangles(0)
            if settings.fallback_target != target or abs(settings.fallback_relative_error - error) > 0.0001:
                mesh.modify()
                settings.fallback_target = target
                settings.fallback_relative_error = error
                STATIC_MESH.set_nanite_settings(mesh, settings, True)
                save(mesh)
            ue.log(f"MASHINA_KORINISHI {mesh.get_name()}: {before} -> {mesh.get_num_triangles(0)}")


def main():
    paint_materials()
    glass_and_lights()
    damas_stripe()
    fallback_meshes()


if __name__ == "__main__":
    main()
