"""Mavjud Bozor xaritasidagi PostProcess ekspozitsiyasini build_level.py dagi qiymatlarga keltiradi (to'liq qayta qurishsiz)."""
import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Bozor/Maps/Bozor")
acts = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
unreal.log_warning("BOZOR aktorlar: %d %s" % (len(acts), sorted({a.get_class().get_name() for a in acts if "Post" in a.get_class().get_name()})))
for a in acts:
    if a.get_class().get_name() == "PostProcessVolume":
        s = a.get_editor_property("settings")
        for k, v in (("auto_exposure_bias", 0.0), ("auto_exposure_min_brightness", -2.2), ("auto_exposure_max_brightness", -1.5)):
            s.set_editor_property("override_" + k, True)
            s.set_editor_property(k, v)
        a.set_editor_property("settings", s)
        unreal.log_warning("BOZOR ekspozitsiya: %s" % a.get_actor_label())
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
