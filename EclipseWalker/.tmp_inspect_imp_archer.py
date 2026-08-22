import bpy

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(
    filepath=r"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker\Models\Imp\Model\Imp_Archer.fixed.fbx"
)

for obj in bpy.data.objects:
    if obj.type != "MESH":
        continue

    weighted_vertices = sum(1 for vertex in obj.data.vertices if vertex.groups)
    modifiers = [
        (modifier.type, getattr(getattr(modifier, "object", None), "name", None))
        for modifier in obj.modifiers
    ]
    print(
        "MESH",
        obj.name,
        "verts=",
        len(obj.data.vertices),
        "weighted=",
        weighted_vertices,
        "groups=",
        [group.name for group in obj.vertex_groups],
        "parent=",
        obj.parent.name if obj.parent else None,
        "parent_type=",
        obj.parent_type,
        "parent_bone=",
        obj.parent_bone,
        "modifiers=",
        modifiers,
        "location=",
        tuple(round(value, 6) for value in obj.location),
        "rotation=",
        tuple(round(value, 6) for value in obj.rotation_euler),
        "scale=",
        tuple(round(value, 6) for value in obj.scale),
        "local_matrix=",
        [[round(value, 6) for value in row] for row in obj.matrix_local],
    )
