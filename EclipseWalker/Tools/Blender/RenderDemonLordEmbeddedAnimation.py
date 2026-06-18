from pathlib import Path

import bpy
from mathutils import Vector


WORKSPACE = Path(__file__).resolve().parents[2]
BOSS_DIR = WORKSPACE / "Models" / "Boss"
MODEL_PATH = BOSS_DIR / "SK_DemonLord_UnityBakedCompatiblePivot.glb"


bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
if MODEL_PATH.suffix.lower() == ".fbx":
    bpy.ops.import_scene.fbx(filepath=str(MODEL_PATH), automatic_bone_orientation=False)
else:
    bpy.ops.import_scene.gltf(filepath=str(MODEL_PATH))

armature = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
action = armature.animation_data.action if armature.animation_data else None
if action is None:
    raise RuntimeError("The converted GLB has no embedded animation Action.")

print(
    "EMBEDDED_ACTION "
    f"name={action.name} range={tuple(action.frame_range)} fcurves={len(action.fcurves)}"
)

mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
if not mesh_objects:
    raise RuntimeError("The converted GLB has no mesh objects.")

body = max(mesh_objects, key=lambda obj: obj.dimensions.length)
print(
    "RENDER_BODY "
    f"name={body.name} dimensions={tuple(round(value, 4) for value in body.dimensions)}"
)
scene = bpy.context.scene
scene.frame_start = max(1, int(action.frame_range[0]))
scene.frame_end = max(scene.frame_start, int(action.frame_range[1]))
scene.frame_set(scene.frame_start)
bpy.context.view_layer.update()

evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
corners = [evaluated.matrix_world @ Vector(corner) for corner in evaluated.bound_box]
minimum = Vector((min(v.x for v in corners), min(v.y for v in corners), min(v.z for v in corners)))
maximum = Vector((max(v.x for v in corners), max(v.y for v in corners), max(v.z for v in corners)))
center = (minimum + maximum) * 0.5
size = max((maximum - minimum).x, (maximum - minimum).z)

camera_data = bpy.data.cameras.new("DiagnosticCamera")
camera = bpy.data.objects.new("DiagnosticCamera", camera_data)
scene.collection.objects.link(camera)
camera.location = center + Vector((0.0, -size * 3.0, 0.0))
camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
camera_data.type = "ORTHO"
camera_data.ortho_scale = size * 1.25
camera_data.clip_end = size * 10.0
scene.camera = camera
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = 600
scene.render.resolution_y = 600
scene.render.resolution_percentage = 100

frames = (scene.frame_start, (scene.frame_start + scene.frame_end) // 2)
for frame in frames:
    scene.frame_set(frame)
    scene.render.filepath = str(BOSS_DIR / f"DemonLord_UnityBakedCompatiblePivot_Frame{frame}.png")
    bpy.ops.render.render(write_still=True)
