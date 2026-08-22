import json
import re
from pathlib import Path

import bpy
from mathutils import Matrix


WORKSPACE = Path(__file__).resolve().parents[2]
JSON_PATH = WORKSPACE / "Models" / "Boss" / "DemonLord_Idle_UnityGlobalSample.json"


def normalize(value):
    value = re.sub(r"\.\d{3}$", "", value)
    return "".join(character.lower() for character in value if character.isalnum())


def read_matrix(value):
    return Matrix(tuple(
        tuple(value[f"m{row}{column}"] for column in range(4))
        for row in range(4)
    ))


data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
source = {entry["name"]: read_matrix(entry["matrix"]) for entry in data["bindTransforms"]}
armature = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
targets = {normalize(bone.name): bone for bone in armature.data.bones}
pose_targets = {normalize(bone.name): bone for bone in armature.pose.bones}

anchor_source_name = next(name for name in source if normalize(name).endswith("pelvis"))
source_anchor = source[anchor_source_name].to_translation()
target_anchor = targets[normalize(anchor_source_name)].matrix_local.to_translation()
axis_conversion = Matrix(((0.0, 0.0, 1.0), (0.0, -1.0, 0.0), (-1.0, 0.0, 0.0)))
numerator = 0.0
denominator = 0.0
for source_name, source_matrix in source.items():
    target = targets.get(normalize(source_name))
    if target is None:
        continue
    converted = axis_conversion @ (source_matrix.to_translation() - source_anchor)
    target_offset = target.matrix_local.to_translation() - target_anchor
    numerator += target_offset.dot(converted)
    denominator += converted.length_squared
scale = numerator / denominator
conversion = Matrix.Identity(4)
conversion[0][0], conversion[0][1], conversion[0][2] = 0.0, 0.0, scale
conversion[1][0], conversion[1][1], conversion[1][2] = 0.0, -scale, 0.0
conversion[2][0], conversion[2][1], conversion[2][2] = -scale, 0.0, 0.0
conversion.translation = target_anchor - conversion.to_3x3() @ source_anchor

for source_name, source_matrix in source.items():
    target = targets.get(normalize(source_name))
    if target is None:
        continue
    if any(token in normalize(source_name) for token in ("pelvis", "head", "hand", "foot", "wing")):
        sp = source_matrix.to_translation()
        tp = target.matrix_local.to_translation()
        print(
            "BIND_PAIR "
            f"name={source_name!r} "
            f"source=({sp.x:.6f},{sp.y:.6f},{sp.z:.6f}) "
            f"target=({tp.x:.6f},{tp.y:.6f},{tp.z:.6f})"
        )

for frame in (1, 16, 31, 46, 61):
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    scale_rows = []
    for pose_bone in armature.pose.bones:
        scale = pose_bone.scale
        maximum = max(abs(scale.x), abs(scale.y), abs(scale.z))
        minimum = min(abs(scale.x), abs(scale.y), abs(scale.z))
        scale_rows.append((maximum, minimum, pose_bone.name, tuple(scale)))
    scale_rows.sort(reverse=True)
    print(f"POSE_SCALE frame={frame} largest={scale_rows[:5]}")

    length_rows = []
    for pose_bone in armature.pose.bones:
        if pose_bone.parent is None:
            continue
        rest_length = (
            pose_bone.bone.matrix_local.to_translation()
            - pose_bone.parent.bone.matrix_local.to_translation()
        ).length
        pose_length = (
            pose_bone.matrix.to_translation()
            - pose_bone.parent.matrix.to_translation()
        ).length
        if rest_length > 1.0e-5:
            length_rows.append((pose_length / rest_length, pose_bone.name))
    length_rows.sort(reverse=True)
    print(f"BONE_LENGTH_RATIO frame={frame} largest={length_rows[:10]}")

    source_frame = {
        entry["name"]: read_matrix(entry["matrix"])
        for entry in data["frames"][frame - 1]["transforms"]
    }
    position_errors = []
    for source_name, source_matrix in source_frame.items():
        pose_bone = pose_targets.get(normalize(source_name))
        if pose_bone is None:
            continue
        expected = conversion @ source_matrix.to_translation()
        error = (pose_bone.matrix.to_translation() - expected).length
        position_errors.append((error, source_name))
    position_errors.sort(reverse=True)
    print(f"POSE_POSITION_ERROR frame={frame} largest={position_errors[:10]}")

    matrix_errors = []
    inverse_conversion = conversion.inverted_safe()
    for source_name, source_frame_matrix in source_frame.items():
        pose_bone = pose_targets.get(normalize(source_name))
        target_bone = targets.get(normalize(source_name))
        source_bind_matrix = source.get(source_name)
        if pose_bone is None or target_bone is None or source_bind_matrix is None:
            continue
        intended_deformation = (
            conversion
            @ source_frame_matrix
            @ source_bind_matrix.inverted_safe()
            @ inverse_conversion
        )
        intended_pose = intended_deformation @ target_bone.matrix_local
        difference = pose_bone.matrix.inverted_safe() @ intended_pose
        translation_error = difference.to_translation().length
        rotation_error = difference.to_quaternion().angle
        matrix_errors.append((translation_error, rotation_error, source_name))
    matrix_errors.sort(reverse=True)
    print(f"POSE_MATRIX_ERROR frame={frame} largest={matrix_errors[:10]}")
