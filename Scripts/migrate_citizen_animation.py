"""Bake NPC animation onto Citizen skeletons and migrate existing actors in place.

Run through the editor Python console. All validation happens before map edits.
"""
import collections
import unreal

E = unreal.EditorAssetLibrary
ROOT = '/Game/Bozor/CitizenAnim'


def bake(gender):
    suffix = 'Male' if gender == 'ma' else 'Female'
    target = unreal.load_asset('/Game/CitizenNPC/CharacterParts/Meshes/Basebody/SK_%s_body_average' % gender)
    source = unreal.load_asset('/Game/MetaHumans/Heads/Unpacked/MH_TalabaYigit/Body/SKM_MH_TalabaYigit_BodyMesh')
    rtg = unreal.load_asset('/Game/Bozor/Retarget/Citizen/RTG_MetaHuman_Citizen' + suffix)
    dest = ROOT + '/' + suffix
    assets = [E.find_asset_data(p) for p in E.list_assets('/Game/Bozor/MetaAnim', True, False)]
    out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        assets, source, target, rtg, target_path=dest, include_referenced_assets=True,
        overwrite_existing_files=True)
    assert out, 'Retarget failed: ' + suffix
    bp = unreal.load_asset(dest + '/ABP_Manny')
    assert bp and bp.generated_class(), 'Missing Citizen AnimBP'
    E.save_directory(dest, only_if_is_dirty=False, recursive=True)
    print('BAKED', suffix, len(out))


def migrate():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    npcs = [a for a in actors if isinstance(a, unreal.BozorNpc)]
    assert npcs, 'Open Bozor first'
    # Validate all replacements before touching a single actor.
    def replacement(anim, gender):
        if not anim:
            return None
        p = ROOT + '/' + ('Male' if gender == 'ma' else 'Female') + '/' + anim.get_name()
        result = unreal.load_asset(p)
        assert result, 'Missing animation: ' + p
        return result
    for a in npcs:
        g = 'fe' if a.get_editor_property('female') else 'ma'
        for action in a.get_editor_property('own_actions'):
            replacement(action.anim, g)
    managers = [a for a in actors if isinstance(a, unreal.BozorNpcManager)]
    for mgr in managers:
        for action in mgr.get_editor_property('actions'):
            for g in ('ma', 'fe'):
                replacement(action.anim, g)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for a in npcs:
        a.modify()
        g = 'fe' if a.get_editor_property('female') else 'ma'
        mesh = a.get_editor_property('mesh')
        parts = a.get_components_by_class(unreal.SkeletalMeshComponent)
        citizen = next((p for p in parts if p.component_has_tag('CitizenPose')), None)
        assert citizen, 'Expected Citizen actor: ' + a.get_actor_label()
        visible = [p for p in parts if p != mesh and p != citizen]
        for p in visible:
            p.set_leader_pose_component(None)
            p.attach_to_component(mesh, '', unreal.AttachmentRule.KEEP_RELATIVE,
                                  unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE, False)
        mesh.set_skeletal_mesh_asset(citizen.get_skeletal_mesh_asset())
        bp = unreal.load_asset(ROOT + '/' + ('Male' if g == 'ma' else 'Female') + '/ABP_Manny')
        mesh.set_anim_instance_class(bp.generated_class())
        mesh.set_editor_property('disable_post_process_blueprint', True)
        # Keep the source body hidden: garments contain the visible body surface.
        mesh.set_visibility(False, False)
        mesh.set_editor_property('visibility_based_anim_tick_option', unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        for p in visible:
            p.set_leader_pose_component(mesh)
            p.set_editor_property('disable_post_process_blueprint', True)
        handles = sds.k2_gather_subobject_data_for_instance(a)
        h = next(h for h in handles if lib.get_object(lib.get_data(h)) == citizen)
        assert sds.k2_delete_subobject_from_instance(handles[0], h) == 1
        # Manager categories become gender-specific actor overrides.
        merged = {}
        for mgr in managers:
            for action in mgr.get_editor_property('actions'):
                merged.setdefault(str(action.category), []).append(action)
        own = a.get_editor_property('own_actions')
        for cat in {str(x.category) for x in own}:
            merged[cat] = [x for x in own if str(x.category) == cat]
        a.set_editor_property('own_actions', [unreal.BozorNpcAction(
            category=x.category, anim=replacement(x.anim, g), loop=x.loop)
            for actions in merged.values() for x in actions])
    for mgr in managers:
        mgr.set_editor_property('actions', [])
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    print('MIGRATED', len(npcs))


def bake_originals(gender):
    """Use original package clips, avoiding accumulated MetaHuman retarget errors.

    FK-only offline retarget preserves authored knee bends; mannequin IK goals
    must not drive a different body's legs a second time.
    """
    suffix = 'Male' if gender == 'ma' else 'Female'
    target = unreal.load_asset('/Game/CitizenNPC/CharacterParts/Meshes/Basebody/SK_%s_body_average' % gender)
    rig = unreal.load_asset('/Game/Bozor/Retarget/Citizen/IK_Citizen' + suffix)
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    originals = collections.defaultdict(list)
    for ad in reg.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence'), True):
        p = str(ad.package_name)
        if p.startswith('/Game/') and not p.startswith(('/Game/Bozor/', '/Game/BoshQahramon/')):
            originals[str(ad.asset_name)].append(ad)
    groups = collections.defaultdict(list)
    missing = []
    for p in E.list_assets('/Game/Bozor/MetaAnim', True, False):
        anim = unreal.load_asset(p)
        if not isinstance(anim, unreal.AnimSequence):
            continue
        options = sorted(originals.get(anim.get_name(), []), key=lambda x: (not str(x.package_name).startswith('/Game/NPCQueueSystem/'), str(x.package_name)))
        if not options:
            missing.append(anim.get_name())
            continue
        original = options[0].get_asset()
        mesh = original.get_editor_property('skeleton').get_skeleton_preview_mesh()
        assert mesh, 'No source preview mesh: ' + original.get_path_name()
        groups[mesh.get_path_name()].append(options[0])
    assert not missing, 'Original clips missing: ' + str(missing)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for mesh_path, ads in groups.items():
        source = unreal.load_asset(mesh_path)
        name = source.get_editor_property('skeleton').get_name() + '_' + suffix
        folder = ROOT + '/Rigs'
        src_rig = unreal.load_asset(folder + '/IK_' + name)
        if not src_rig:
            src_rig = tools.create_asset('IK_' + name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
            sc = unreal.IKRigController.get_controller(src_rig)
            sc.set_skeletal_mesh(source)
            sc.apply_auto_generated_retarget_definition()
        rtg = unreal.load_asset(folder + '/RTG_' + name)
        if not rtg:
            rtg = tools.create_asset('RTG_' + name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
        rc = unreal.IKRetargeterController.get_controller(rtg)
        s, t = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
        rc.set_ik_rig(s, src_rig)
        rc.set_ik_rig(t, rig)
        rc.set_preview_mesh(s, source)
        rc.set_preview_mesh(t, target)
        if rc.get_num_retarget_ops() == 0:
            rc.add_default_ops()
        rc.assign_ik_rig_to_all_ops(s, src_rig)
        rc.assign_ik_rig_to_all_ops(t, rig)
        rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        rc.auto_align_all_bones(t)
        for i in range(rc.get_num_retarget_ops()):
            if 'IK Rig' in str(rc.get_op_name(i)):
                rc.set_retarget_op_enabled(i, False)
        result = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            ads, source, target, rtg, target_path=ROOT + '/' + suffix,
            include_referenced_assets=False, overwrite_existing_files=True)
        assert len(result) == len(ads), 'Incomplete batch: ' + mesh_path
        for ad in result:
            a = ad.get_asset()
            a.set_editor_property('enable_root_motion', False)
            unreal.BozorLevelTools.set_anim_root_lock(a, True)
        print('ORIGINAL', suffix, source.get_name(), len(result))
    E.save_directory(ROOT, only_if_is_dirty=False, recursive=True)


if __name__ == '__main__':
    bake('ma')
    bake('fe')
    migrate()
