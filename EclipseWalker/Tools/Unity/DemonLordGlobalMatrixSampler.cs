using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;

public class DemonLordGlobalMatrixSampler : EditorWindow
{
    public GameObject rootObject;
    public AnimationClip clip;
    public int fps = 30;
    public string outputPath =
        @"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker\Models\Boss\DemonLord_Idle_UnityGlobalSample.json";

    [MenuItem("Tools/EclipseWalker/Export Animation Global Matrix JSON")]
    public static void Open()
    {
        GetWindow<DemonLordGlobalMatrixSampler>("Global Matrix Sampler");
    }

    private void OnGUI()
    {
        rootObject = (GameObject)EditorGUILayout.ObjectField(
            "Root Object", rootObject, typeof(GameObject), true);
        clip = (AnimationClip)EditorGUILayout.ObjectField(
            "Animation Clip", clip, typeof(AnimationClip), false);
        fps = Mathf.Max(1, EditorGUILayout.IntField("FPS", fps));
        outputPath = EditorGUILayout.TextField("Output Path", outputPath);

        if (GUILayout.Button("Export Global Matrix JSON"))
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
        Transform[] transforms = rootObject.GetComponentsInChildren<Transform>(true);
        Matrix4x4 fixedRootWorldToLocal = root.worldToLocalMatrix;
        var originalTransforms = CaptureOriginalTransforms(transforms);
        var bindMatrices = BuildBindMatrices(root, transforms, fixedRootWorldToLocal);

        var export = new ExportData
        {
            formatVersion = 2,
            matrixLayout = "Unity Matrix4x4 fields (m00..m33), column-vector multiplication",
            clipName = clip.name,
            rootName = root.name,
            fps = fps,
            length = clip.length,
            bindTransforms = new List<TransformMatrixData>(),
            frames = new List<FrameData>()
        };

        foreach (Transform target in transforms)
        {
            export.bindTransforms.Add(new TransformMatrixData
            {
                name = target.name,
                path = GetPath(root, target),
                matrix = MatrixData.From(bindMatrices[target])
            });
        }

        int frameCount = Mathf.CeilToInt(clip.length * fps) + 1;
        AnimationMode.StartAnimationMode();

        try
        {
            for (int frame = 0; frame < frameCount; ++frame)
            {
                float time = Mathf.Min(frame / (float)fps, clip.length);

                AnimationMode.BeginSampling();
                AnimationMode.SampleAnimationClip(rootObject, clip, time);
                AnimationMode.EndSampling();

                var frameData = new FrameData
                {
                    frame = frame,
                    time = time,
                    transforms = new List<TransformMatrixData>()
                };

                foreach (Transform target in transforms)
                {
                    Matrix4x4 rootRelative =
                        fixedRootWorldToLocal * target.localToWorldMatrix;

                    frameData.transforms.Add(new TransformMatrixData
                    {
                        name = target.name,
                        path = GetPath(root, target),
                        matrix = MatrixData.From(rootRelative)
                    });
                }

                export.frames.Add(frameData);
            }
        }
        finally
        {
            AnimationMode.StopAnimationMode();
            RestoreOriginalTransforms(originalTransforms);
        }

        string directory = Path.GetDirectoryName(outputPath);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        File.WriteAllText(outputPath, JsonUtility.ToJson(export, true));
        AssetDatabase.Refresh();

        Debug.Log(
            $"Global matrix export complete: {outputPath}\n" +
            $"Transforms: {transforms.Length}, frames: {frameCount}, bind matrices: {bindMatrices.Count}");
    }

    private static Dictionary<Transform, Matrix4x4> BuildBindMatrices(
        Transform root,
        Transform[] transforms,
        Matrix4x4 rootWorldToLocal)
    {
        var result = new Dictionary<Transform, Matrix4x4>();

        // Hierarchy transforms are the fallback for non-skinned helper nodes.
        foreach (Transform target in transforms)
            result[target] = rootWorldToLocal * target.localToWorldMatrix;

        // Mesh bind poses are the authoritative rest matrices for skinning bones.
        foreach (SkinnedMeshRenderer renderer in root.GetComponentsInChildren<SkinnedMeshRenderer>(true))
        {
            Mesh mesh = renderer.sharedMesh;
            if (mesh == null)
                continue;

            Matrix4x4[] bindPoses = mesh.bindposes;
            Transform[] bones = renderer.bones;
            int count = Mathf.Min(bindPoses.Length, bones.Length);

            for (int index = 0; index < count; ++index)
            {
                Transform bone = bones[index];
                if (bone == null)
                    continue;

                Matrix4x4 bindInRoot =
                    rootWorldToLocal
                    * renderer.transform.localToWorldMatrix
                    * bindPoses[index].inverse;

                result[bone] = bindInRoot;
            }
        }

        return result;
    }

    private static List<OriginalTransform> CaptureOriginalTransforms(Transform[] transforms)
    {
        var originals = new List<OriginalTransform>(transforms.Length);
        foreach (Transform target in transforms)
        {
            originals.Add(new OriginalTransform
            {
                target = target,
                localPosition = target.localPosition,
                localRotation = target.localRotation,
                localScale = target.localScale
            });
        }
        return originals;
    }

    private static void RestoreOriginalTransforms(List<OriginalTransform> originals)
    {
        foreach (OriginalTransform original in originals)
        {
            original.target.localPosition = original.localPosition;
            original.target.localRotation = original.localRotation;
            original.target.localScale = original.localScale;
        }
    }

    private static string GetPath(Transform root, Transform target)
    {
        if (root == target)
            return root.name;

        var names = new List<string>();
        Transform current = target;
        while (current != null)
        {
            names.Add(current.name);
            if (current == root)
                break;
            current = current.parent;
        }

        names.Reverse();
        return string.Join("/", names);
    }

    [Serializable]
    private class OriginalTransform
    {
        public Transform target;
        public Vector3 localPosition;
        public Quaternion localRotation;
        public Vector3 localScale;
    }

    [Serializable]
    public class ExportData
    {
        public int formatVersion;
        public string matrixLayout;
        public string clipName;
        public string rootName;
        public int fps;
        public float length;
        public List<TransformMatrixData> bindTransforms;
        public List<FrameData> frames;
    }

    [Serializable]
    public class FrameData
    {
        public int frame;
        public float time;
        public List<TransformMatrixData> transforms;
    }

    [Serializable]
    public class TransformMatrixData
    {
        public string name;
        public string path;
        public MatrixData matrix;
    }

    [Serializable]
    public class MatrixData
    {
        public float m00, m01, m02, m03;
        public float m10, m11, m12, m13;
        public float m20, m21, m22, m23;
        public float m30, m31, m32, m33;

        public static MatrixData From(Matrix4x4 value)
        {
            return new MatrixData
            {
                m00 = value.m00, m01 = value.m01, m02 = value.m02, m03 = value.m03,
                m10 = value.m10, m11 = value.m11, m12 = value.m12, m13 = value.m13,
                m20 = value.m20, m21 = value.m21, m22 = value.m22, m23 = value.m23,
                m30 = value.m30, m31 = value.m31, m32 = value.m32, m33 = value.m33
            };
        }
    }
}
