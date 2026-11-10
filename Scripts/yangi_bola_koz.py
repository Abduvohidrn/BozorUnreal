# Ko'zlar: ustidagi shaffof qatlamlar (shox parda, ko'z soyasi, ko'z yoshi chizig'i) ko'rinmas materialga,
# ko'z materialining o'zi glTF blend'dan opaque'ga o'tkaziladi (aks holda qop-qora chiqadi)
import unreal

E = unreal.EditorAssetLibrary
ML = unreal.MaterialEditingLibrary
P = '/Game/BoshQahramon/YangiBola/M_Korinmas'
mat = unreal.load_asset(P)
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Korinmas', '/Game/BoshQahramon/YangiBola', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    c = ML.create_material_expression(mat, unreal.MaterialExpressionConstant, -200, 0)
    c.set_editor_property('r', 0.0)
    ML.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    ML.recompile_material(mat)
    E.save_loaded_asset(mat)

opaque = None
for p in E.list_assets('/InterchangeAssets/gltf/MaterialInstances', recursive=False):
    if p.split('.')[-1] in ('MI_Default_Opaque', 'MI_Default_Opaque_DS'):
        opaque = unreal.load_asset(p)
        if p.endswith('MI_Default_Opaque'):
            break
print('[KZ] opaque asos:', opaque.get_path_name() if opaque else None)

mats_eyelash = unreal.load_asset('/Game/BoshQahramon/YangiBola/GLB/cartoon_Young_Boy/Materials/Std_Eyelash')
mesh = unreal.load_asset('/Game/BoshQahramon/YangiBola/SKM_YangiBola')
slots = list(mesh.get_editor_property('materials'))
for s in slots:
    nom = str(s.get_editor_property('material_slot_name'))
    if any(k in nom for k in ('Cornea', 'Eye_Occlusion', 'Tearline')):
        s.set_editor_property('material_interface', mat)
    elif nom == 'Std_Eyelash':
        s.set_editor_property('material_interface', mats_eyelash)
    elif False:
        s.set_editor_property('material_interface', mat)
    elif nom.startswith('Std_Eye_') and opaque:
        m = s.get_editor_property('material_interface')
        m.set_editor_property('parent', opaque)
        ML.update_material_instance(m)
        E.save_loaded_asset(m)
        print('[KZ] ko\'z opaque:', nom)
mesh.set_editor_property('materials', slots)
print('[KZ] saqlandi', E.save_loaded_asset(mesh))
