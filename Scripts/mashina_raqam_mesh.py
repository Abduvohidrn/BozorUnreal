"""Blender fon rejimida har bir modelning old va orqa raqam mesh'ini eksport qiladi."""

from math import cos, pi, sin
from pathlib import Path
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


SAVED = Path(__file__).resolve().parents[1] / "Saved"
OUT = SAVED / "MashinaRaqam"
# Uzun o'q, old yo'nalish va ikkala raqam o'rtasining balandligi (m).
MOUNTS = {
    "Cobalt": (0, -1, 0.45, 0.49),
    "Damas": (1, 1, 0.49, 0.89),
    "Spark": (1, -1, 0.43, 0.73),
    "Nexia": (1, -1, 0.41, 0.73),
    "Malibu": (1, -1, 0.45, 0.81),
    "BMW": (1, -1, 0.40, 0.76),
}
WIDTH = 0.52
HEIGHT = 0.112


def body_surface(name):
    vertices, triangles = [], []
    for suffix in ("", "_Qatnov"):
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete(use_global=False)
        path = SAVED / f"{name}Source" / f"SM_{name}{suffix}.glb"
        if not path.exists():   # BMW: faqat turgan mashina, qatnov (g'ildiraksiz) varianti yo'q
            continue
        bpy.ops.import_scene.gltf(filepath=str(path))
        for obj in bpy.context.scene.objects:
            if obj.type == "MESH":
                start = len(vertices)
                vertices.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
                triangles.extend(tuple(start + index for index in face.vertices) for face in obj.data.polygons)
    return BVHTree.FromPolygons(vertices, triangles, all_triangles=True)


def plate(surface, axis, sign, height):
    normal = Vector((0, 0, 0))
    normal[axis] = sign
    right = Vector((0, 0, 1)).cross(normal)
    up = Vector((0, 0, 1))

    def depth(x, y):
        hit, _, _, _ = surface.ray_cast(4 * normal + x * right + (height + y) * up, -normal)
        if hit is None or hit.dot(normal) < 1.3:
            raise ValueError(f"Raqam ostidagi kuzov topilmadi: {axis}, {sign}, {height}, {x}, {y}")
        return hit.dot(normal)

    slope = (depth(0, 0.035) - depth(0, -0.035)) / 0.07
    slope = max(-0.3, min(0.3, slope))
    offset = max(
        depth(WIDTH * i / 24, HEIGHT * j / 12) - slope * HEIGHT * j / 12
        for i in range(-12, 13) for j in range(-6, 7)
    ) + 0.004

    radius = 0.008
    points = []
    uv = []
    for corner, (sx, sy) in enumerate(((1, 1), (-1, 1), (-1, -1), (1, -1))):
        for step in range(5):
            angle = (corner + step / 4) * pi / 2
            x = sx * (WIDTH / 2 - radius) + cos(angle) * radius
            y = sy * (HEIGHT / 2 - radius) + sin(angle) * radius
            points.append((offset + slope * y) * normal + x * right + (height + y) * up)
            uv.append((0.5 + x / WIDTH, 0.5 + y / HEIGHT))
    return points, uv


def export_pair(name, mount):
    surface = body_surface(name)
    axis, front_sign, front_height, rear_height = mount
    front, front_uv = plate(surface, axis, front_sign, front_height)
    rear, rear_uv = plate(surface, axis, -front_sign, rear_height)
    mesh_name = f"SM_Raqam_{name}"
    mesh = bpy.data.meshes.new(mesh_name)
    mesh.from_pydata(front + rear, [], [list(range(len(front))), list(range(len(front), len(front) + len(rear)))])
    mesh.update()
    uv = mesh.uv_layers.new(name="Raqam")
    for loop in mesh.loops:
        uv.data[loop.index].uv = (front_uv + rear_uv)[loop.vertex_index]
    obj = bpy.data.objects.new(mesh_name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    assert len(mesh.polygons) == 2 and mesh.validate() is False
    bpy.ops.export_scene.gltf(filepath=str(OUT / f"{mesh_name}.glb"), export_format="GLB",
                              use_selection=True, export_yup=True, export_materials="NONE")
    print(mesh_name, "old/orqa", [round(p[axis] * 100, 1) for p in (front[0], rear[0])], flush=True)


OUT.mkdir(exist_ok=True)
models = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else MOUNTS
for model in models:
    export_pair(model, MOUNTS[model])
