# ABP ishlatadigan blendspace'ni harakatlanadigan ("...1") kliplarga ulaydi
import unreal
E = unreal.EditorAssetLibrary
D = '/Game/BoshQahramon/YangiBola/Anim/'
b = unreal.load_asset(D + 'BS_Idle_Walk_Run_YangiBola1')
samples = list(b.get_editor_property('sample_data'))
for s in samples:
    a = s.get_editor_property('animation')
    if a and not a.get_name().endswith('1'):
        yangi = unreal.load_asset(D + a.get_name() + '1')
        if yangi:
            s.set_editor_property('animation', yangi)
b.set_editor_property('sample_data', samples)
print('[BS]', [s.get_editor_property('animation').get_name() for s in b.get_editor_property('sample_data')][:6], E.save_loaded_asset(b))
