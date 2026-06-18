from pathlib import Path

import bpy
from mathutils import Vector


WORKSPACE = Path(__file__).resolve().parents[2]
BOSS_DIR = WORKSPACE / "Models" / "Boss"
MODEL_PATH = BOSS_DIR / "SK_DemonLord_Assimp.glb"
IMPORTER_PATH = BOSS_DIR / "ImportDemonLordUnityGlobalAnimation.py"
OUTPUT_PATH = BOSS_DIR / "DemonLord_GlobalMatrix_Test.blend"


bpy.ops.object.mode_set(mode="OBJECT") if bpy.context.object and bpy.context.object.mode != "OBJECT" else None
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

bpy.ops.import_scene.gltf(filepath=str(MODEL_PATH))

scope = {
    "__file__": str(IMPORTER_PATH),
    "__name__": "__main__",
}
source = IMPORTER_PATH.read_text(encoding="utf-8")
exec(compile(source, str(IMPORTER_PATH), "exec"), scope)

armatures = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
if not armatures:
    raise RuntimeError("The GLB import produced no Armature object.")

armature = armatures[0]
action = armature.animation_data.action if armature.animation_data else None
if action is None:
    raise RuntimeError("The global matrix importer produced no animation Action.")

key_count = sum(len(curve.keyframe_points) for curve in action.fcurves)
print(
    "GLOBAL_MATRIX_TEST_READY "
    f"armature={armature.name} bones={len(armature.pose.bones)} "
    f"fcurves={len(action.fcurves)} keys={key_count}"
)

body_mesh = bpy.data.objects.get("SK_DemonLord")
if body_mesh is not None:
    render_center = None
    render_size = None
    for frame in (1, 16, 31, 46, 61):
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()
        evaluated = body_mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        corners = [evaluated.matrix_world @ Vector(corner) for corner in evaluated.bound_box]
        minimum = Vector((min(v.x for v in corners), min(v.y for v in corners), min(v.z for v in corners)))
        maximum = Vector((max(v.x for v in corners), max(v.y for v in corners), max(v.z for v in corners)))
        dimensions = maximum - minimum
        if frame == 1:
            render_center = (minimum + maximum) * 0.5
            render_size = max(dimensions.x, dimensions.z)
        print(
            "BODY_BOUNDS "
            f"frame={frame} dimensions=({dimensions.x:.4f}, {dimensions.y:.4f}, {dimensions.z:.4f})"
        )

    camera_data = bpy.data.cameras.new("DiagnosticCamera")
    camera = bpy.data.objects.new("DiagnosticCamera", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = render_center + Vector((0.0, -render_size * 3.0, 0.0))
    camera.rotation_euler = (render_center - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = render_size * 1.25
    camera_data.clip_end = render_size * 10.0
    bpy.context.scene.camera = camera
    bpy.context.scene.render.engine = "BLENDER_WORKBENCH"
    bpy.context.scene.render.resolution_x = 600
    bpy.context.scene.render.resolution_y = 600
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.display.shading.light = "STUDIO"
    bpy.context.scene.display.shading.show_shadows = True

    for frame in (1, 31):
        bpy.context.scene.frame_set(frame)
        bpy.context.scene.render.filepath = str(
            BOSS_DIR / f"DemonLord_GlobalMatrix_Frame{frame}.png"
        )
        bpy.ops.render.render(write_still=True)

bpy.context.scene.frame_set(1)

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_PATH))
