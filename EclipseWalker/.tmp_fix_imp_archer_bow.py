import bpy

source_path = r"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker\Models\Imp\Model\Imp_Archer.fbx"
output_path = r"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker\Models\Imp\Model\Imp_Archer.fixed.fbx"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=source_path)

bow = bpy.data.objects.get("SKM_Demon_Bow")
if bow is None or bow.type != "MESH":
    raise RuntimeError("SKM_Demon_Bow mesh was not found")

bpy.ops.object.mode_set(mode="OBJECT") if bpy.context.object and bpy.context.object.mode != "OBJECT" else None
bpy.ops.object.select_all(action="DESELECT")
bow.select_set(True)
bpy.context.view_layer.objects.active = bow
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

if any(abs(value - 1.0) > 0.0001 for value in bow.scale):
    raise RuntimeError(f"Bow scale was not applied: {tuple(bow.scale)}")

bpy.ops.object.select_all(action="DESELECT")
for obj in bpy.data.objects:
    if obj.type in {"ARMATURE", "MESH"}:
        obj.select_set(True)

bpy.ops.export_scene.fbx(
    filepath=output_path,
    use_selection=True,
    object_types={"ARMATURE", "MESH"},
    axis_forward="-Z",
    axis_up="Y",
    apply_unit_scale=True,
    use_space_transform=True,
    use_mesh_modifiers=True,
    add_leaf_bones=False,
    bake_anim=False,
)

print("FIXED_BOW_SCALE", tuple(bow.scale))
print("EXPORTED", output_path)
