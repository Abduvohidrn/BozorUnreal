"""NPC ssenariylari uchun kerakli, lekin hali MetaHuman skeletiga o'tkazilmagan kliplarni retarget qiladi.

Manba paketlar arxivdan (D:\\Unreal Projects\\BozorUnreal_Arxiv\\Content) vaqtincha /Game ga nusxalanadi; natija
/Game/Bozor/MetaAnim ga o'sha nom bilan yoziladi (npc_ssenariy.json kliplarni shu nom bilan qidiradi).
Hammasi UE4 manekeni (SK_Mannequin). Maqsad - MetaHuman tanasi va /Game/Bozor/Retarget/IK_MetaHuman.
"""
import unreal

ROOT = "/Game/Bozor"
DEST = ROOT + "/MetaAnim"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET

# (paket, manba manekeni, kliplar)
JOBS = [
    ("Vendors", "/Game/Vendors_And_Customers/Demo/Mannequin/Character/Mesh/SK_Mannequin", [
        "/Game/Vendors_And_Customers/Animations/Cashier/AnimationSequence_Cashier_Talk_HeadNod_No",
        "/Game/Vendors_And_Customers/Animations/Bartender/AnimationSequence_Bartender_Idle_Standing_HandsOnTable_WipeCounter_Loop",
        "/Game/Vendors_And_Customers/Animations/Customer/AnimationSequence_Customer_Standing_ShakeHug",
    ]),
    ("City2", "/Game/CityAnimations_pack2/Mannequin/Character/Mesh/SK_Mannequin", [
        "/Game/CityAnimations_pack2/Animation/Collect_in_basket_%d" % i for i in range(1, 6)
    ]),
    ("Dialog", "/Game/DialogAnimations/Character/Mesh/SK_Mannequin", [
        "/Game/DialogAnimations/Animations/" + n for n in
        ("Dialog_Intense_Medium_01", "Dialog_Intense_Medium_02", "Dialog_Intense_Short_02", "Dialog_Intense_Short_03")
    ]),
]


def target_mesh():
    """Mavjud MetaAnim klipi ishlatgan MetaHuman tanasi (skeleti bir xil bo'lishi uchun)."""
    sk = unreal.load_asset(DEST + "/Dialog_Calm_Short_01").get_editor_property("skeleton")
    for n in ("MH_TalabaYigit", "MH_ZamonaviyXaridor", "MH_Onaxon"):
        m = unreal.load_asset("/Game/MetaHumans/Heads/Unpacked/%s/Body/SKM_%s_BodyMesh" % (n, n))
        if m and m.get_editor_property("skeleton") == sk:
            return m
    raise RuntimeError("MetaAnim skeletiga mos MetaHuman tanasi topilmadi: %s" % sk.get_path_name())


def retargeter(name, src_mesh, tgt_mesh, tgt_rig):
    path = "%s/Retarget/RTG_%s_MetaHuman" % (ROOT, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    rig = tools.create_asset("IK_Src_" + name, ROOT + "/Retarget", unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(src_mesh)
    c.apply_auto_generated_retarget_definition()
    c.apply_auto_fbik()
    rtg = tools.create_asset("RTG_%s_MetaHuman" % name, ROOT + "/Retarget", unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(S, rig)
    rc.set_ik_rig(T, tgt_rig)
    rc.set_preview_mesh(S, src_mesh)
    rc.set_preview_mesh(T, tgt_mesh)
    if rc.get_num_retarget_ops() == 0:
        rc.add_default_ops()
    rc.assign_ik_rig_to_all_ops(S, rig)
    rc.assign_ik_rig_to_all_ops(T, tgt_rig)
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    rc.auto_align_all_bones(T)
    eal.save_loaded_asset(rig)
    eal.save_loaded_asset(rtg)
    return rtg


def run():
    tgt = target_mesh()
    tgt_rig = unreal.load_asset(ROOT + "/Retarget/IK_MetaHuman")
    print("maqsad:", tgt.get_path_name(), "rig:", tgt_rig.get_path_name())
    made = []
    for name, src_path, anims in JOBS:
        src = unreal.load_asset(src_path)
        rtg = retargeter(name, src, tgt, tgt_rig)
        ads = [eal.find_asset_data(p) for p in anims]
        ads = [a for a in ads if a and a.is_valid()]
        res = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            ads, src, tgt, rtg, search="", replace="", prefix="", suffix="", target_path=DEST,
            include_referenced_assets=False, overwrite_existing_files=True)
        for r in res:
            made.append(str(r.asset_name))
        print(name, len(ads), "->", [str(r.asset_name) for r in res])
    for n in made:
        eal.save_asset(DEST + "/" + n, only_if_is_dirty=False)
    return made


if __name__ == "__main__":
    run()
