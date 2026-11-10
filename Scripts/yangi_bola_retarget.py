# Eski bola animatsiyalari (ABP_Unarmed_Bola va kliplar) yangi bolaga IK Retargeter bilan ko'chiriladi.
import unreal

E = unreal.EditorAssetLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
DEST = '/Game/BoshQahramon/YangiBola'
mesh_old = unreal.load_asset('/Game/BoshQahramon/Bola/Rig/SKM_Bola')
mesh_new = unreal.load_asset(DEST + '/SKM_YangiBola')
ik_old = unreal.load_asset('/Game/BoshQahramon/Bola/Rig/IK_Bola')
print('[RT] manba:', bool(mesh_old), bool(mesh_new), bool(ik_old))

# yangi IK rig: retarget zanjirlari avtomatik
ik_new = unreal.load_asset(DEST + '/IK_YangiBola')
if not ik_new:
    ik_new = T.create_asset('IK_YangiBola', DEST, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
c = unreal.IKRigController.get_controller(ik_new)
c.set_skeletal_mesh(mesh_new)
c.apply_auto_generated_retarget_definition()
print('[RT] yangi zanjirlar:', [str(ch.chain_name) for ch in c.get_retarget_chains()])
print('[RT] eski zanjirlar:', [str(ch.chain_name) for ch in unreal.IKRigController.get_controller(ik_old).get_retarget_chains()])
E.save_loaded_asset(ik_new)

rtg = unreal.load_asset(DEST + '/RTG_Bola_YangiBola')
if not rtg:
    rtg = T.create_asset('RTG_Bola_YangiBola', DEST, unreal.IKRetargeter, unreal.IKRetargetFactory())
rc = unreal.IKRetargeterController.get_controller(rtg)
# UE 5.8: retargeter operatsiyalar to'plami bilan ishlaydi; yangi assetda ular yo'q - standartlari qo'shiladi
if rc.get_num_retarget_ops() == 0:
    rc.add_default_ops()
print('[RT] ops:', rc.get_num_retarget_ops())
rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, ik_old)
rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik_new)
rc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, mesh_old)
rc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, mesh_new)
try:
    rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, ik_old)
    rc.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, ik_new)
except Exception as e:
    print('[RT] ops:', e)
try:
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
except Exception as e:
    print('[RT] automap:', e)
try:
    rc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
except Exception as e:
    print('[RT] align:', e)
print('[RT] moslik:', {c: str(rc.get_source_chain(c)) for c in ['Spine', 'LeftArm', 'RightArm', 'LeftLeg', 'Head']})
E.save_loaded_asset(rtg)

# Eslatma: retarget faqat to'liq editor seansida ishlaydi (-ExecutePythonScript); commandlet'da kliplar T-pozada chiqadi.
# Qayta ishlatishdan oldin DEST/Anim papkasini alohida o'chiring, aks holda nomlarga "1" qo'shiladi.

# ABP va u ishlatadigan kliplar
abp = unreal.EditorAssetLibrary.find_asset_data('/Game/BoshQahramon/Bola/Anim/ABP_Unarmed_Bola')
inp = unreal.IKRetargetBatchOperationInputs()
inp.assets_to_retarget = [abp]
inp.source_mesh = mesh_old
inp.target_mesh = mesh_new
inp.ik_retarget_asset = rtg
inp.search = '_Bola'
inp.replace = '_YangiBola'
inp.target_path = DEST + '/Anim'
inp.use_source_path = False
inp.include_referenced_assets = True
inp.overwrite_existing_files = True
res = unreal.IKRetargetBatchOperation.run_batch_retarget(inp)
print('[RT] natija:', [str(r.package_name) for r in res])
for r in res:
    E.save_asset(str(r.package_name))
# AnimBlueprint natija ro'yxatiga kirmaydi - alohida saqlanadi
ABP_NEW = DEST + '/Anim/ABP_Unarmed_YangiBola'
if unreal.find_object(None, ABP_NEW + '.ABP_Unarmed_YangiBola') or E.does_asset_exist(ABP_NEW):
    print('[RT] ABP saqlandi:', E.save_asset(ABP_NEW, only_if_is_dirty=False))
