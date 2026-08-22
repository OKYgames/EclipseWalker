using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;

public class DemonLordEwskExporter : EditorWindow
{
    private const uint Magic = 0x4B535745; // EWSK
    private const uint Version = 1;

    public GameObject rootObject;
    public AnimationClip clip;
    public int fps = 30;
    public string outputPath =
        @"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker\Models\Boss\SK_DemonLord_UnityDirect.ewsk";

    [MenuItem("Tools/EclipseWalker/Export DemonLord EWSK")]
    public static void Open()
    {
        GetWindow<DemonLordEwskExporter>("DemonLord EWSK Exporter");
    }

    private void OnEnable()
    {
        if (rootObject == null)
            rootObject = GameObject.Find("SK_DemonLord");
        if (clip == null)
            clip = AssetDatabase.LoadAssetAtPath<AnimationClip>("Assets/boss/Idle.anim");

        outputPath = Path.GetFullPath(Path.Combine(
            Application.dataPath,
            "boss",
            "Exports",
            "SK_DemonLord_UnityDirect.ewsk"));
    }

    private void OnGUI()
    {
        rootObject = (GameObject)EditorGUILayout.ObjectField(
            "Root Object", rootObject, typeof(GameObject), true);
        clip = (AnimationClip)EditorGUILayout.ObjectField(
            "Animation Clip", clip, typeof(AnimationClip), false);
        fps = Mathf.Max(1, EditorGUILayout.IntField("FPS", fps));
        outputPath = EditorGUILayout.TextField("Output Path", outputPath);

        if (GUILayout.Button("Export EWSK"))
            Export();
    }

    private void Export()
    {
        if (rootObject == null || clip == null)
        {
            Debug.LogError("Root Object and Animation Clip are required.");
            return;
        }

        Transform root = rootObject.transform;
        Transform[] hierarchy = rootObject.GetComponentsInChildren<Transform>(true);
        SkinnedMeshRenderer[] renderers =
            rootObject.GetComponentsInChildren<SkinnedMeshRenderer>(true);
        if (renderers.Length == 0)
        {
            Debug.LogError("The selected root has no SkinnedMeshRenderer.");
            return;
        }

        var originals = CaptureOriginalTransforms(hierarchy);
        var bindGlobals = BuildBindGlobals(root, hierarchy, renderers);
        var bones = BuildBoneTable(renderers, bindGlobals);
        if (bones.Count == 0 || bones.Count > 256)
        {
            Debug.LogError("Invalid exported bone count: " + bones.Count);
            return;
        }

        var boneIndices = new Dictionary<Transform, uint>();
        for (int index = 0; index < bones.Count; ++index)
            boneIndices[bones[index].transform] = (uint)index;

        BuildGeometry(root, renderers, boneIndices, out var vertices, out var indices, out var subsets);
        var frames = SampleAnimation(rootObject, hierarchy, clip, fps, originals);

        string directory = Path.GetDirectoryName(outputPath);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        try
        {
            using (var stream = new FileStream(outputPath, FileMode.Create, FileAccess.Write))
            using (var writer = new BinaryWriter(stream, Encoding.UTF8))
            {
                writer.Write(Magic);
                writer.Write(Version);
                writer.Write((uint)vertices.Count);
                writer.Write((uint)indices.Count);
                writer.Write((uint)bones.Count);
                writer.Write((uint)subsets.Count);
                writer.Write(1u); // clip count

                foreach (ExportVertex vertex in vertices)
                    WriteVertex(writer, vertex);
                foreach (uint index in indices)
                    writer.Write(index);

                for (int index = 0; index < bones.Count; ++index)
                {
                    BoneRecord bone = bones[index];
                    writer.Write(index);
                    WriteString(writer, bone.transform.name);
                    WriteMatrix(writer, bone.bindGlobal.inverse);
                }

                foreach (SubsetRecord subset in subsets)
                {
                    writer.Write(subset.vertexStart);
                    writer.Write(subset.indexStart);
                    writer.Write(subset.indexCount);
                    writer.Write(subset.materialIndex);
                    WriteString(writer, subset.name);
                }

                WriteHierarchy(writer, root, root, bindGlobals);

                WriteString(writer, "SkeletonIdle");
                writer.Write((float)(frames.Count - 1));
                writer.Write((float)fps);
                writer.Write((byte)0); // lock root motion XZ
                writer.Write((uint)hierarchy.Length);

                foreach (Transform target in hierarchy)
                {
                    WriteString(writer, target.name);
                    writer.Write((uint)frames.Count);
                    for (int frame = 0; frame < frames.Count; ++frame)
                    {
                        SampledTransform sampled = frames[frame][target];
                        writer.Write(sampled.position.x);
                        writer.Write(sampled.position.y);
                        writer.Write(sampled.position.z);
                        writer.Write((double)frame);
                    }

                    writer.Write((uint)frames.Count);
                    for (int frame = 0; frame < frames.Count; ++frame)
                    {
                        SampledTransform sampled = frames[frame][target];
                        writer.Write(sampled.rotation.x);
                        writer.Write(sampled.rotation.y);
                        writer.Write(sampled.rotation.z);
                        writer.Write(sampled.rotation.w);
                        writer.Write((double)frame);
                    }

                    writer.Write((uint)frames.Count);
                    for (int frame = 0; frame < frames.Count; ++frame)
                    {
                        SampledTransform sampled = frames[frame][target];
                        writer.Write(sampled.scale.x);
                        writer.Write(sampled.scale.y);
                        writer.Write(sampled.scale.z);
                        writer.Write((double)frame);
                    }
                }
            }
        }
        finally
        {
            RestoreOriginalTransforms(originals);
        }

        AssetDatabase.Refresh();
        Debug.Log(
            "DemonLord EWSK export complete: " + outputPath + "\n" +
            $"vertices={vertices.Count}, indices={indices.Count}, bones={bones.Count}, " +
            $"subsets={subsets.Count}, frames={frames.Count}");
    }

    private static Dictionary<Transform, Matrix4x4> BuildBindGlobals(
        Transform root,
        Transform[] hierarchy,
        SkinnedMeshRenderer[] renderers)
    {
        Matrix4x4 rootWorldToLocal = root.worldToLocalMatrix;
        var result = new Dictionary<Transform, Matrix4x4>();
        foreach (Transform target in hierarchy)
            result[target] = rootWorldToLocal * target.localToWorldMatrix;

        foreach (SkinnedMeshRenderer renderer in renderers)
        {
            Mesh mesh = renderer.sharedMesh;
            if (mesh == null)
                continue;

            Matrix4x4 rootFromMesh = rootWorldToLocal * renderer.transform.localToWorldMatrix;
            Matrix4x4[] bindPoses = mesh.bindposes;
            Transform[] rendererBones = renderer.bones;
            int count = Mathf.Min(bindPoses.Length, rendererBones.Length);
            for (int index = 0; index < count; ++index)
            {
                Transform bone = rendererBones[index];
                if (bone != null)
                    result[bone] = rootFromMesh * bindPoses[index].inverse;
            }
        }

        return result;
    }

    private static List<BoneRecord> BuildBoneTable(
        SkinnedMeshRenderer[] renderers,
        Dictionary<Transform, Matrix4x4> bindGlobals)
    {
        var records = new List<BoneRecord>();
        var seen = new HashSet<Transform>();
        foreach (SkinnedMeshRenderer renderer in renderers)
        {
            foreach (Transform bone in renderer.bones)
            {
                if (bone == null || !seen.Add(bone))
                    continue;
                if (!bindGlobals.TryGetValue(bone, out Matrix4x4 bindGlobal))
                    continue;
                records.Add(new BoneRecord { transform = bone, bindGlobal = bindGlobal });
            }
        }
        return records;
    }

    private static void BuildGeometry(
        Transform root,
        SkinnedMeshRenderer[] renderers,
        Dictionary<Transform, uint> globalBoneIndices,
        out List<ExportVertex> vertices,
        out List<uint> indices,
        out List<SubsetRecord> subsets)
    {
        vertices = new List<ExportVertex>();
        indices = new List<uint>();
        subsets = new List<SubsetRecord>();
        Matrix4x4 rootWorldToLocal = root.worldToLocalMatrix;

        foreach (SkinnedMeshRenderer renderer in renderers)
        {
            Mesh mesh = renderer.sharedMesh;
            if (mesh == null)
                continue;

            uint baseVertex = (uint)vertices.Count;
            Matrix4x4 rootFromMesh = rootWorldToLocal * renderer.transform.localToWorldMatrix;
            Vector3[] positions = mesh.vertices;
            Vector3[] normals = mesh.normals;
            Vector4[] tangents = mesh.tangents;
            Vector2[] uv = mesh.uv;
            BoneWeight[] weights = mesh.boneWeights;
            Transform[] rendererBones = renderer.bones;

            for (int vertexIndex = 0; vertexIndex < positions.Length; ++vertexIndex)
            {
                var vertex = new ExportVertex();
                vertex.position = rootFromMesh.MultiplyPoint3x4(positions[vertexIndex]);
                vertex.normal = vertexIndex < normals.Length
                    ? rootFromMesh.MultiplyVector(normals[vertexIndex]).normalized
                    : Vector3.up;
                vertex.uv = vertexIndex < uv.Length
                    ? new Vector2(uv[vertexIndex].x, 1.0f - uv[vertexIndex].y)
                    : Vector2.zero;
                vertex.tangent = vertexIndex < tangents.Length
                    ? rootFromMesh.MultiplyVector(new Vector3(
                        tangents[vertexIndex].x,
                        tangents[vertexIndex].y,
                        tangents[vertexIndex].z)).normalized
                    : Vector3.right;

                if (vertexIndex < weights.Length)
                    FillWeights(ref vertex, weights[vertexIndex], rendererBones, globalBoneIndices);
                else
                    vertex.weights[0] = 1.0f;

                vertices.Add(vertex);
            }

            for (int submeshIndex = 0; submeshIndex < mesh.subMeshCount; ++submeshIndex)
            {
                int[] sourceIndices = mesh.GetIndices(submeshIndex);
                var subset = new SubsetRecord
                {
                    vertexStart = baseVertex,
                    indexStart = (uint)indices.Count,
                    indexCount = (uint)sourceIndices.Length,
                    materialIndex = (uint)submeshIndex,
                    name = renderer.name + "_" + submeshIndex
                };

                foreach (int sourceIndex in sourceIndices)
                    indices.Add(baseVertex + (uint)sourceIndex);
                subsets.Add(subset);
            }
        }
    }

    private static void FillWeights(
        ref ExportVertex target,
        BoneWeight source,
        Transform[] rendererBones,
        Dictionary<Transform, uint> globalBoneIndices)
    {
        int[] sourceIndices =
            { source.boneIndex0, source.boneIndex1, source.boneIndex2, source.boneIndex3 };
        float[] sourceWeights =
            { source.weight0, source.weight1, source.weight2, source.weight3 };
        float total = 0.0f;

        for (int slot = 0; slot < 4; ++slot)
        {
            int localIndex = sourceIndices[slot];
            float weight = sourceWeights[slot];
            if (weight <= 0.0f || localIndex < 0 || localIndex >= rendererBones.Length)
                continue;
            Transform bone = rendererBones[localIndex];
            if (bone == null || !globalBoneIndices.TryGetValue(bone, out uint globalIndex))
                continue;
            target.indices[slot] = globalIndex;
            target.weights[slot] = weight;
            total += weight;
        }

        if (total <= 0.0001f)
        {
            target.indices[0] = 0;
            target.weights[0] = 1.0f;
            return;
        }

        for (int slot = 0; slot < 4; ++slot)
            target.weights[slot] /= total;
    }

    private static List<Dictionary<Transform, SampledTransform>> SampleAnimation(
        GameObject rootObject,
        Transform[] hierarchy,
        AnimationClip clip,
        int fps,
        List<OriginalTransform> originals)
    {
        int frameCount = Mathf.CeilToInt(clip.length * fps) + 1;
        var frames = new List<Dictionary<Transform, SampledTransform>>(frameCount);
        AnimationMode.StartAnimationMode();

        try
        {
            for (int frame = 0; frame < frameCount; ++frame)
            {
                float time = Mathf.Min(frame / (float)fps, clip.length);
                AnimationMode.BeginSampling();
                AnimationMode.SampleAnimationClip(rootObject, clip, time);
                AnimationMode.EndSampling();

                var samples = new Dictionary<Transform, SampledTransform>();
                foreach (Transform target in hierarchy)
                {
                    samples[target] = new SampledTransform
                    {
                        position = target.localPosition,
                        rotation = target.localRotation,
                        scale = target.localScale
                    };
                }
                frames.Add(samples);
            }
        }
        finally
        {
            AnimationMode.StopAnimationMode();
            RestoreOriginalTransforms(originals);
        }

        return frames;
    }

    private static void WriteHierarchy(
        BinaryWriter writer,
        Transform root,
        Transform target,
        Dictionary<Transform, Matrix4x4> bindGlobals)
    {
        WriteString(writer, target.name);
        Matrix4x4 local;
        if (target == root)
        {
            local = Matrix4x4.identity;
        }
        else
        {
            Matrix4x4 parentGlobal = bindGlobals[target.parent];
            local = parentGlobal.inverse * bindGlobals[target];
        }
        WriteMatrix(writer, local);
        writer.Write((uint)target.childCount);
        for (int index = 0; index < target.childCount; ++index)
            WriteHierarchy(writer, root, target.GetChild(index), bindGlobals);
    }

    private static void WriteVertex(BinaryWriter writer, ExportVertex vertex)
    {
        writer.Write(vertex.position.x); writer.Write(vertex.position.y); writer.Write(vertex.position.z);
        writer.Write(vertex.normal.x); writer.Write(vertex.normal.y); writer.Write(vertex.normal.z);
        writer.Write(vertex.uv.x); writer.Write(vertex.uv.y);
        writer.Write(vertex.tangent.x); writer.Write(vertex.tangent.y); writer.Write(vertex.tangent.z);
        for (int slot = 0; slot < 4; ++slot) writer.Write(vertex.weights[slot]);
        for (int slot = 0; slot < 4; ++slot) writer.Write(vertex.indices[slot]);
    }

    private static void WriteMatrix(BinaryWriter writer, Matrix4x4 matrix)
    {
        // Unity uses column vectors; DirectX animator uses row vectors.
        writer.Write(matrix.m00); writer.Write(matrix.m10); writer.Write(matrix.m20); writer.Write(matrix.m30);
        writer.Write(matrix.m01); writer.Write(matrix.m11); writer.Write(matrix.m21); writer.Write(matrix.m31);
        writer.Write(matrix.m02); writer.Write(matrix.m12); writer.Write(matrix.m22); writer.Write(matrix.m32);
        writer.Write(matrix.m03); writer.Write(matrix.m13); writer.Write(matrix.m23); writer.Write(matrix.m33);
    }

    private static void WriteString(BinaryWriter writer, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
        writer.Write((uint)bytes.Length);
        writer.Write(bytes);
    }

    private static List<OriginalTransform> CaptureOriginalTransforms(Transform[] transforms)
    {
        var originals = new List<OriginalTransform>(transforms.Length);
        foreach (Transform target in transforms)
        {
            originals.Add(new OriginalTransform
            {
                target = target,
                position = target.localPosition,
                rotation = target.localRotation,
                scale = target.localScale
            });
        }
        return originals;
    }

    private static void RestoreOriginalTransforms(List<OriginalTransform> originals)
    {
        foreach (OriginalTransform original in originals)
        {
            original.target.localPosition = original.position;
            original.target.localRotation = original.rotation;
            original.target.localScale = original.scale;
        }
    }

    private class BoneRecord
    {
        public Transform transform;
        public Matrix4x4 bindGlobal;
    }

    private class ExportVertex
    {
        public Vector3 position;
        public Vector3 normal;
        public Vector2 uv;
        public Vector3 tangent;
        public float[] weights = new float[4];
        public uint[] indices = new uint[4];
    }

    private struct SubsetRecord
    {
        public uint vertexStart;
        public uint indexStart;
        public uint indexCount;
        public uint materialIndex;
        public string name;
    }

    private struct SampledTransform
    {
        public Vector3 position;
        public Quaternion rotation;
        public Vector3 scale;
    }

    private class OriginalTransform
    {
        public Transform target;
        public Vector3 position;
        public Quaternion rotation;
        public Vector3 scale;
    }
}
