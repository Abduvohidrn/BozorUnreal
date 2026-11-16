"""Bino tagi kadrlari: SceneCapture2D bilan PNG (muharrir viewport'iga bog'liq emas).

Ishlatish (muharrirda): exec(open(path).read()); kadrlar("oldin")
"""
import unreal as ue

V = ue.Vector
OUT = "D:/Trk/BozorKit_hammasi/bino_tagi_skrin"
EKSPOZITSIYA = 0.0
KAMERALAR = [
    # nom, ko'z, qarash nuqtasi, fov
    ("shimol_skameyka", (5600, -2050, 260), (6650, -800, 20), 80),   # foydalanuvchi skrinshoti joyi
    ("shimol_uzoq", (4300, 900, 300), (6700, 2200, 40), 85),
    ("janub_bino", (-1200, -2600, 260), (-2450, -1000, 30), 82),
    ("hovli", (2600, -4300, 330), (2600, -1800, 0), 85),
]


def kadrlar(prefiks, kameralar=KAMERALAR):
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    natija = []
    for nom, eye, look, fov in kameralar:
        loc = V(*eye)
        a = actors.spawn_actor_from_class(ue.SceneCapture2D, loc, (V(*look) - loc).rotator())
        c = a.get_component_by_class(ue.SceneCaptureComponent2D)
        rt = ue.RenderingLibrary.create_render_target2d(world, 1600, 900, ue.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        c.set_editor_property("texture_target", rt)
        c.set_editor_property("capture_source", ue.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        c.set_editor_property("capture_every_frame", False)
        c.set_editor_property("fov_angle", fov)
        # Bir kadrlik capture'da ko'z moslashuvi (eye adaptation) qorong'i chiqaradi: qo'lda ekspozitsiya.
        pp = c.get_editor_property("post_process_settings")
        pp.set_editor_property("override_auto_exposure_method", True)
        pp.set_editor_property("auto_exposure_method", ue.AutoExposureMethod.AEM_MANUAL)
        pp.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
        pp.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
        pp.set_editor_property("override_auto_exposure_bias", True)
        pp.set_editor_property("auto_exposure_bias", EKSPOZITSIYA)
        c.set_editor_property("post_process_settings", pp)
        c.set_editor_property("post_process_blend_weight", 1.0)
        c.capture_scene()
        fayl = f"{prefiks}_{nom}.png"
        ue.RenderingLibrary.export_render_target(world, rt, OUT, fayl)
        actors.destroy_actor(a)
        natija.append(fayl)
    return natija
