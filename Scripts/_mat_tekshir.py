import unreal
M = unreal.MaterialEditingLibrary
for n in ['Std_Skin_Head', 'Toon_Hoodie', 'Std_Eye_R', 'mat_hair_short_straight_Transparency']:
    mi = unreal.load_asset('/Game/BoshQahramon/YangiBola/' + n)
    par = mi.get_editor_property('parent')
    print('[MT]', n, 'parent', par.get_path_name() if par else None)
    print('[MT]   tex', [str(x) for x in M.get_texture_parameter_names(mi)])
    print('[MT]   scalar', [str(x) for x in M.get_scalar_parameter_names(mi)][:25])
    print('[MT]   vector', [str(x) for x in M.get_vector_parameter_names(mi)][:15])
    print('[MT]   static', [str(x) for x in M.get_static_switch_parameter_names(mi)][:15])
print('[MT] M_BozorLit', unreal.EditorAssetLibrary.does_asset_exist('/Game/Bozor/Materials/M_BozorLit'))
