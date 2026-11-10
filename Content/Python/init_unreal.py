"""Muharrir ochilganda yuklanadi: Claude uchun MCP vositalari (Python bajarish va sahna haqida qisqa ma'lumot)."""
import contextlib
import io
import traceback

import unreal

try:
    import toolset_registry
    from toolset_registry.registration import Registration
except ImportError:          # ToolsetRegistry plagini yoqilmagan
    toolset_registry = None


if toolset_registry:
    @unreal.uclass()
    class BozorTools(unreal.ToolsetDefinition):
        """Bozor loyihasi: muharrirda ixtiyoriy Python kodini bajarish va sahna holatini o'qish."""

        @toolset_registry.tool_call
        @staticmethod
        def run_python(code: str) -> str:
            """Unreal muharririda Python kodini bajaradi (unreal moduli tayyor).

            print() chiqishi qaytariladi; kod `result` o'zgaruvchisini o'rnatsa, uning qiymati ham qo'shiladi.
            Xato bo'lsa, traceback qaytariladi.

            Args:
                code: Bajariladigan Python kodi.
            Returns:
                Chiqish matni.
            """
            out = io.StringIO()
            scope = {"unreal": unreal, "__name__": "__bozor_mcp__"}
            try:
                with contextlib.redirect_stdout(out):
                    exec(compile(code, "<mcp>", "exec"), scope)
                if "result" in scope:
                    out.write("\nresult = %r" % (scope["result"],))
            except Exception:
                out.write(traceback.format_exc())
            return out.getvalue()[-60000:] or "(chiqish yo'q)"

        @toolset_registry.tool_call
        @staticmethod
        def level_summary() -> str:
            """Ochiq xarita nomi va undagi aktorlar soni (klass bo'yicha).

            Returns:
                Xarita va aktorlar haqida matn.
            """
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            counts = {}
            for a in actors:
                n = a.get_class().get_name()
                counts[n] = counts.get(n, 0) + 1
            lines = ["xarita: %s, aktorlar: %d" % (world.get_path_name() if world else None, len(actors))]
            lines += ["  %4d %s" % (c, n) for n, c in sorted(counts.items(), key=lambda kv: -kv[1])]
            return "\n".join(lines)

    _registration = Registration([BozorTools])
    if _registration.register():
        unreal.log("BozorTools MCP vositalari ro'yxatdan o'tdi")
