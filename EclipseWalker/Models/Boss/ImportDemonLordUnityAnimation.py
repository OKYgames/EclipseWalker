import json
import re
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector


JSON_PATH = Path(
    r"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker"
    r"\Models\Boss\DemonLord_Idle_UnitySample.json"
)
ACTION_NAME = "DemonLord_Idle_UnityBake"


def normalized_name(value):
    value = re.sub(r"\.\d{3}$", "", value)
    return "".join(character.lower() for character in value if character.isalnum())


def count_mesh_users(armature):
    user_count = 0
    for candidate in bpy.data.objects:
        if candidate.type != "MESH":
            continue

        if candidate.find_armature() == armature:
            user_count += 1
            continue

        for modifier in candidate.modifiers:
            if modifier.type == "ARMATURE" and modifier.object == armature:
                user_count += 1
                break

    return user_count


def add_missing_armature_modifiers(armature):
    armature_bone_names = {
        normalized_name(bone.name)
        for bone in armature.data.bones
    }
    added_count = 0

    for candidate in bpy.data.objects:
        if candidate.type != "MESH" or candidate.find_armature() is not None:
            continue

        matching_group_count = sum(
            1
            for group in candidate.vertex_groups
            if normalized_name(group.name) in armature_bone_names
        )
        if matching_group_count == 0:
            continue

        modifier = candidate.modifiers.new(
            name="UnityAnimationArmature",
            type="ARMATURE",
        )
        modifier.object = armature
        modifier.use_deform_preserve_volume = True
        added_count += 1

    return added_count


def get_skin_weight_stats(armature):
    vertex_group_count = 0
    matching_group_count = 0
    weighted_vertex_count = 0
    total_vertex_count = 0
    armature_bone_names = {
        normalized_name(bone.name)
        for bone in armature.data.bones
    }

    for candidate in bpy.data.objects:
        if candidate.type != "MESH" or candidate.find_armature() != armature:
            continue

        vertex_group_count += len(candidate.vertex_groups)
        matching_group_count += sum(
            1
            for group in candidate.vertex_groups
            if normalized_name(group.name) in armature_bone_names
        )
        total_vertex_count += len(candidate.data.vertices)
        weighted_vertex_count += sum(
            1
            for vertex in candidate.data.vertices
            if len(vertex.groups) > 0
        )

        for modifier in candidate.modifiers:
            if modifier.type == "ARMATURE" and modifier.object == armature:
                modifier.show_viewport = True

    return (
        vertex_group_count,
        matching_group_count,
        weighted_vertex_count,
        total_vertex_count,
    )


def align_vertex_group_names(armature):
    bone_names = [bone.name for bone in armature.data.bones]
    normalized_bone_names = {
        bone_name: normalized_name(bone_name)
        for bone_name in bone_names
    }
    renamed_count = 0

    for candidate in bpy.data.objects:
        if candidate.type != "MESH" or candidate.find_armature() != armature:
            continue

        for group in candidate.vertex_groups:
            group_key = normalized_name(group.name)
            matches = [
                bone_name
                for bone_name, bone_key in normalized_bone_names.items()
                if group_key == bone_key
                or group_key.endswith(bone_key)
                or bone_key.endswith(group_key)
            ]
            matches.sort(
                key=lambda bone_name: len(normalized_bone_names[bone_name]),
                reverse=True,
            )

            if len(matches) == 1 or (
                len(matches) > 1
                and len(normalized_bone_names[matches[0]])
                > len(normalized_bone_names[matches[1]])
            ):
                target_name = matches[0]
                if group.name != target_name:
                    group.name = target_name
                    renamed_count += 1

    return renamed_count


def find_target_armature(source_names):
    active = bpy.context.view_layer.objects.active
    best_object = None
    best_score = -1
    normalized_sources = {normalized_name(name) for name in source_names}

    for candidate in bpy.data.objects:
        if candidate.type != "ARMATURE":
            continue

        match_count = sum(
            1
            for bone in candidate.pose.bones
            if normalized_name(bone.name) in normalized_sources
        )
        mesh_user_count = count_mesh_users(candidate)
        active_bonus = 1 if candidate == active else 0
        score = mesh_user_count * 10000 + match_count * 10 + active_bonus

        if score > best_score:
            best_object = candidate
            best_score = score

    return best_object


def show_result(title, lines, icon="INFO"):
    def draw(self, _context):
        for line in lines:
            self.layout.label(text=line)

    bpy.context.window_manager.popup_menu(draw, title=title, icon=icon)


if not JSON_PATH.exists():
    raise FileNotFoundError(f"Animation JSON was not found: {JSON_PATH}")

with JSON_PATH.open("r", encoding="utf-8") as file:
    animation = json.load(file)

frames = animation.get("frames", [])
if not frames:
    raise RuntimeError("The animation JSON contains no frames.")

reference_transforms = {
    transform["name"]: transform
    for transform in frames[0].get("transforms", [])
}
source_names = set(reference_transforms.keys())
armature = find_target_armature(source_names)
if armature is None:
    raise RuntimeError("No Armature object was found in the Blender scene.")

mesh_user_count = count_mesh_users(armature)
added_modifier_count = 0
if mesh_user_count == 0:
    added_modifier_count = add_missing_armature_modifiers(armature)
    mesh_user_count = count_mesh_users(armature)

if mesh_user_count == 0:
    raise RuntimeError(
        "The meshes have no vertex groups matching the Armature bones. "
        "The imported FBX does not contain usable Blender skin weights."
    )

(
    vertex_group_count,
    matching_group_count,
    weighted_vertex_count,
    total_vertex_count,
) = get_skin_weight_stats(armature)

renamed_group_count = 0
if vertex_group_count > 0 and matching_group_count == 0:
    renamed_group_count = align_vertex_group_names(armature)
    (
        vertex_group_count,
        matching_group_count,
        weighted_vertex_count,
        total_vertex_count,
    ) = get_skin_weight_stats(armature)

if matching_group_count == 0 or weighted_vertex_count == 0:
    raise RuntimeError(
        "The Armature Modifier exists, but the mesh has no usable matching skin weights. "
        f"groups={vertex_group_count}, matching={matching_group_count}, "
        f"weighted={weighted_vertex_count}/{total_vertex_count}"
    )

exact_bones = {bone.name: bone for bone in armature.pose.bones}
normalized_bones = {}
for bone in armature.pose.bones:
    normalized_bones.setdefault(normalized_name(bone.name), []).append(bone)


def find_pose_bone(source_name):
    exact = exact_bones.get(source_name)
    if exact is not None:
        return exact

    candidates = normalized_bones.get(normalized_name(source_name), [])
    return candidates[0] if len(candidates) == 1 else None


matched_names = {
    name: find_pose_bone(name)
    for name in source_names
}
matched_names = {
    name: bone
    for name, bone in matched_names.items()
    if bone is not None
}

if len(matched_names) < 10:
    raise RuntimeError(
        f"Only {len(matched_names)} bones matched. Select the model Armature and run again."
    )

if armature.animation_data is None:
    armature.animation_data_create()

old_action = bpy.data.actions.get(ACTION_NAME)
if old_action is not None:
    bpy.data.actions.remove(old_action)

action = bpy.data.actions.new(ACTION_NAME)
armature.animation_data.action = action
armature.data.display_type = "STICK"
armature.show_in_front = True

for pose_bone in armature.pose.bones:
    pose_bone.rotation_mode = "QUATERNION"
    pose_bone.location = (0.0, 0.0, 0.0)
    pose_bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
    pose_bone.scale = (1.0, 1.0, 1.0)

# Unity Y-up coordinates to Blender Z-up coordinates.
unity_to_blender = Matrix(
    (
        (1.0, 0.0, 0.0),
        (0.0, 0.0, -1.0),
        (0.0, 1.0, 0.0),
    )
)
blender_to_unity = unity_to_blender.inverted()

rest_local_rotations = {}
for pose_bone in armature.pose.bones:
    if pose_bone.parent is None:
        rest_local_matrix = pose_bone.bone.matrix_local
    else:
        rest_local_matrix = (
            pose_bone.parent.bone.matrix_local.inverted()
            @ pose_bone.bone.matrix_local
        )
    rest_local_rotations[pose_bone.name] = (
        rest_local_matrix.to_quaternion().normalized().to_matrix()
    )

scene = bpy.context.scene
fps = max(1, int(animation.get("fps", 30)))
scene.render.fps = fps
scene.frame_start = 1
scene.frame_end = len(frames)

for frame_index, frame_data in enumerate(frames, start=1):
    scene.frame_set(frame_index)
    current_transforms = {
        transform["name"]: transform
        for transform in frame_data.get("transforms", [])
    }

    for source_name, pose_bone in matched_names.items():
        reference = reference_transforms.get(source_name)
        current = current_transforms.get(source_name)
        if reference is None or current is None:
            continue

        reference_rotation = Quaternion(
            (reference["rw"], reference["rx"], reference["ry"], reference["rz"])
        ).normalized()
        current_rotation = Quaternion(
            (current["rw"], current["rx"], current["ry"], current["rz"])
        ).normalized()

        unity_parent_delta = (
            current_rotation.to_matrix()
            @ reference_rotation.to_matrix().inverted()
        )
        blender_parent_delta = (
            unity_to_blender
            @ unity_parent_delta
            @ blender_to_unity
        )
        rest_local_rotation = rest_local_rotations[pose_bone.name]
        blender_bone_delta = (
            rest_local_rotation.inverted()
            @ blender_parent_delta
            @ rest_local_rotation
        )

        pose_bone.rotation_quaternion = blender_bone_delta.to_quaternion().normalized()
        pose_bone.location = Vector((0.0, 0.0, 0.0))
        pose_bone.scale = Vector((1.0, 1.0, 1.0))

        pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame_index)

scene.frame_set(1)
armature.select_set(True)
bpy.context.view_layer.objects.active = armature

show_result(
    "Unity Animation Imported",
    [
        f"Armature: {armature.name}",
        f"Skinned meshes: {mesh_user_count}",
        f"Added modifiers: {added_modifier_count}",
        f"Vertex groups: {vertex_group_count} (matching {matching_group_count})",
        f"Renamed groups: {renamed_group_count}",
        f"Weighted vertices: {weighted_vertex_count}/{total_vertex_count}",
        f"Matched bones: {len(matched_names)}",
        f"Frames: {len(frames)} at {fps} FPS",
        "Press Play to inspect the animation.",
    ],
)
