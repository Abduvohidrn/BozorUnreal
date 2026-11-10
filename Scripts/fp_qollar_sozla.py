# Epic FP shabloni (ABP_FP_Copy + CtrlRig_FPWarp) bola modelida ishlashi uchun:
# bola skeletini Manny skeleti bilan mos (compatible) qiladi va assetlarni tekshiradi.
import unreal

E = unreal.EditorAssetLibrary
bola = unreal.load_asset('/Game/BoshQahramon/Bola/Rig/SKM_Bola_Skeleton')
manny = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
abp = unreal.load_asset('/Game/FirstPerson/Anims/ABP_FP_Copy')
rig = unreal.load_asset('/Game/FirstPerson/Anims/CtrlRig_FPWarp')
print('[FP] yuklandi:', bool(bola), bool(manny), bool(abp), bool(rig))

mos = list(bola.get_editor_property('compatible_skeletons'))
if not any(str(m).endswith('SK_Mannequin') or (hasattr(m, 'get_path_name') and m.get_path_name().endswith('SK_Mannequin')) for m in mos):
    mos.append(manny)
    bola.set_editor_property('compatible_skeletons', mos)
    E.save_loaded_asset(bola)
print('[FP] mos skeletlar:', [str(m) for m in bola.get_editor_property('compatible_skeletons')])

gen = unreal.load_object(None, '/Game/FirstPerson/Anims/ABP_FP_Copy.ABP_FP_Copy_C')
print('[FP] ABP klass:', gen)
