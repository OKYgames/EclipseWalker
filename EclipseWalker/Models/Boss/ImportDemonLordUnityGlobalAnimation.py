import json
import re
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


JSON_PATH = Path(__file__).with_name("DemonLord_Idle_UnityGlobalSample.json")
ACTION_NAME = "DemonLord_Idle_UnityGlobalBake"


def normalized_name(value):
    value = re.sub(r"\.\d{3}$", "", value)
    return "".join(character.lower() for character in value if character.isalnum())


def matrix_from_json(value):
    return Matrix(
        (
            (value["m00"], value["m01"], value["m02"], value["m03"]),
            (value["m10"], value["m11"], value["m12"], value["m13"]),
            (value["m20"], value["m21"], value["m22"], value["m23"]),
            (value["m30"], value["m31"], value["m32"], value["m33"]),
        )
    )


def mesh_users(armature):
    users = []
    for candidate in bpy.data.objects:
        if candidate.type != "MESH":
            continue
        if candidate.find_armature() == armature:
            users.append(candidate)
            continue
        if any(
            modifier.type == "ARMATURE" and modifier.object == armature
            for modifier in candidate.modifiers
        ):
            users.append(candidate)
    return users


def find_target_armature(source_names):
    active = bpy.context.view_layer.objects.active
    normalized_sources = {normalized_name(name) for name in source_names}
    best = None
    best_score = -1

    for candidate in bpy.data.objects:
        if candidate.type != "ARMATURE":
            continue
        matches = sum(
            normalized_name(bone.name) in normalized_sources
            for bone in candidate.pose.bones
        )
        score = len(mesh_users(candidate)) * 10000 + matches * 10
        if candidate == active:
            score += 1
        if score > best_score:
            best = candidate
            best_score = score

    return best


def find_pose_bones(armature, source_names):
    exact = {bone.name: bone for bone in armature.pose.bones}
    normalized = {}
    for bone in armature.pose.bones:
        normalized.setdefault(normalized_name(bone.name), []).append(bone)

    result = {}
    for source_name in source_names:
        bone = exact.get(source_name)
        if bone is None:
            candidates = normalized.get(normalized_name(source_name), [])
            if len(candidates) == 1:
                bone = candidates[0]
        if bone is not None:
            result[source_name] = bone
    return result


def bone_depth(pose_bone):
    depth = 0
    parent = pose_bone.parent
    while parent is not None:
        depth += 1
        parent = parent.parent
    return depth


def show_result(title, lines, icon="INFO"):
    if bpy.app.background:
        print(title + ": " + " | ".join(lines))
        return

    def draw(self, _context):
        for line in lines:
            self.layout.label(text=line)

    bpy.context.window_manager.popup_menu(draw, title=title, icon=icon)


if not JSON_PATH.exists():
    raise FileNotFoundError(f"Global matrix JSON was not found: {JSON_PATH}")

with JSON_PATH.open("r", encoding="utf-8") as file:
    animation = json.load(file)

if animation.get("formatVersion") != 2:
    raise RuntimeError("This script requires the formatVersion 2 global matrix JSON.")

frames = animation.get("frames", [])
bind_entries = animation.get("bindTransforms", [])
if not frames or not bind_entries:
    raise RuntimeError("The global matrix JSON has no bind transforms or animation frames.")

bind_matrices = {
    entry["name"]: matrix_from_json(entry["matrix"])
    for entry in bind_entries
}
source_names = set(bind_matrices)
armature = find_target_armature(source_names)
if armature is None:
    raise RuntimeError("No Armature object was found in the Blender scene.")

skinned_meshes = mesh_users(armature)
if not skinned_meshes:
    raise RuntimeError(
        "The selected Armature has no skinned mesh. Import SK_DemonLord_Assimp.glb first."
    )

matched = find_pose_bones(armature, source_names)
if len(matched) < 10:
    raise RuntimeError(f"Only {len(matched)} bones matched the Unity skeleton.")

ordered_matches = sorted(matched.items(), key=lambda item: bone_depth(item[1]))
target_rest_matrices = {
    source_name: pose_bone.bone.matrix_local.copy()
    for source_name, pose_bone in ordered_matches
}

# Assimp's FBX -> glTF conversion maps this asset from Unity meters to
# Blender centimeters as (x, y, z) -> (z, -y, -x). Estimate the scale and
# translation from all matching bind-pose bone positions instead of assuming
# that the imported armature starts at Unity's root origin.
axis_conversion = Matrix(
    (
        (0.0, 0.0, 1.0),
        (0.0, -1.0, 0.0),
        (-1.0, 0.0, 0.0),
    )
)

anchor_name = next(
    (
        source_name
        for source_name in matched
        if normalized_name(source_name).endswith("pelvis")
    ),
    None,
)
if anchor_name is None:
    raise RuntimeError("The Unity/Blender skeleton match has no Pelvis anchor bone.")

source_anchor = bind_matrices[anchor_name].to_translation()
target_anchor = target_rest_matrices[anchor_name].to_translation()
scale_numerator = 0.0
scale_denominator = 0.0

for source_name in matched:
    source_offset = bind_matrices[source_name].to_translation() - source_anchor
    target_offset = target_rest_matrices[source_name].to_translation() - target_anchor
    converted_offset = axis_conversion @ source_offset
    scale_numerator += target_offset.dot(converted_offset)
    scale_denominator += converted_offset.length_squared

if scale_denominator <= 1.0e-8:
    raise RuntimeError("Could not estimate the Unity-to-Blender skeleton scale.")

source_to_target_scale = scale_numerator / scale_denominator
source_to_target = Matrix.Identity(4)
source_to_target[0][0] = 0.0
source_to_target[0][1] = 0.0
source_to_target[0][2] = source_to_target_scale
source_to_target[1][0] = 0.0
source_to_target[1][1] = -source_to_target_scale
source_to_target[1][2] = 0.0
source_to_target[2][0] = -source_to_target_scale
source_to_target[2][1] = 0.0
source_to_target[2][2] = 0.0
source_to_target.translation = (
    target_anchor
    - source_to_target.to_3x3() @ source_anchor
)
target_to_source = source_to_target.inverted_safe()

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

scene = bpy.context.scene
fps = max(1, int(animation.get("fps", 30)))
scene.render.fps = fps
scene.frame_start = 1
scene.frame_end = len(frames)

for frame_index, frame_data in enumerate(frames, start=1):
    scene.frame_set(frame_index)
    frame_matrices = {
        entry["name"]: matrix_from_json(entry["matrix"])
        for entry in frame_data.get("transforms", [])
    }

    for source_name, pose_bone in ordered_matches:
        source_bind = bind_matrices.get(source_name)
        source_frame = frame_matrices.get(source_name)
        if source_bind is None or source_frame is None:
            continue

        # Skinning moves a vertex with Frame * inverse(Bind). Convert that
        # global deformation into the GLB armature coordinate system, then
        # apply it to Blender's corresponding rest bone.
        source_deformation = source_frame @ source_bind.inverted_safe()
        target_deformation = (
            source_to_target
            @ source_deformation
            @ target_to_source
        )
        target_matrix = target_deformation @ target_rest_matrices[source_name]
        pose_bone.matrix = target_matrix

    bpy.context.view_layer.update()

    for _source_name, pose_bone in ordered_matches:
        pose_bone.keyframe_insert(data_path="location", frame=frame_index)
        pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame_index)
        pose_bone.keyframe_insert(data_path="scale", frame=frame_index)

scene.frame_set(1)
bpy.context.view_layer.objects.active = armature
armature.select_set(True)

show_result(
    "Unity Global Matrix Animation Imported",
    [
        f"Armature: {armature.name}",
        f"Skinned meshes: {len(skinned_meshes)}",
        f"Matched bones: {len(matched)}",
        f"Unity-to-Blender scale: {source_to_target_scale:.6f}",
        f"Frames: {len(frames)} at {fps} FPS",
        "Press Play to inspect the full-matrix animation.",
    ],
)
