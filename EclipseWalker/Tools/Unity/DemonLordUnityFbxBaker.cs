using System.IO;
using UnityEditor;
using UnityEditor.Formats.Fbx.Exporter;
using UnityEngine;

public class DemonLordUnityFbxBaker : EditorWindow
{
    public GameObject rootObject;
    public AnimationClip clip;
    public string outputPath =
        @"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker\Models\Boss\SK_DemonLord_UnityBakedCompatible.fbx";

    [MenuItem("Tools/EclipseWalker/Export DemonLord Baked FBX")]
    public static void Open()
    {
        GetWindow<DemonLordUnityFbxBaker>("DemonLord FBX Baker");
    }

    private void OnGUI()
    {
        rootObject = (GameObject)EditorGUILayout.ObjectField(
            "Root Object", rootObject, typeof(GameObject), true);
        clip = (AnimationClip)EditorGUILayout.ObjectField(
            "Animation Clip", clip, typeof(AnimationClip), false);
        outputPath = EditorGUILayout.TextField("Output Path", outputPath);

        if (GUILayout.Button("Export Model + Selected Animation"))
            Export();
    }

    private void Export()
    {
        if (rootObject == null || clip == null)
        {
            Debug.LogError("Root Object and Animation Clip are required.");
            return;
        }

        string directory = Path.GetDirectoryName(outputPath);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        Animator animator = rootObject.GetComponent<Animator>();
        RuntimeAnimatorController originalController =
            animator != null ? animator.runtimeAnimatorController : null;
        bool originalAnimatorEnabled = animator != null && animator.enabled;

        Animation legacyAnimation = rootObject.GetComponent<Animation>();
        bool addedAnimationComponent = legacyAnimation == null;
        if (addedAnimationComponent)
            legacyAnimation = rootObject.AddComponent<Animation>();

        AnimationClip exportClip = Object.Instantiate(clip);
        exportClip.name = "DemonLord_Idle_UnityBaked";
        exportClip.legacy = true;

        try
        {
            if (animator != null)
            {
                animator.runtimeAnimatorController = null;
                animator.enabled = false;
            }

            legacyAnimation.playAutomatically = false;
            legacyAnimation.AddClip(exportClip, exportClip.name);
            legacyAnimation.clip = exportClip;

            var options = new ExportModelOptions
            {
                ExportFormat = ExportFormat.Binary,
                ModelAnimIncludeOption = Include.ModelAndAnim,
                AnimateSkinnedMesh = true,
                ObjectPosition = ObjectPosition.Reset,
                ExportUnrendered = true,
                KeepInstances = false,
                UseMayaCompatibleNames = true,
                EmbedTextures = false
            };

            string result = ModelExporter.ExportObject(outputPath, rootObject, options);
            if (string.IsNullOrEmpty(result))
                throw new IOException("Unity FBX Exporter returned no output path.");

            Debug.Log(
                "DemonLord baked FBX export complete: " + result + "\n" +
                "Clip: " + clip.name + ", length: " + clip.length + " seconds");
        }
        finally
        {
            if (legacyAnimation != null)
                legacyAnimation.RemoveClip(exportClip.name);

            if (addedAnimationComponent && legacyAnimation != null)
                Object.DestroyImmediate(legacyAnimation);

            if (animator != null)
            {
                animator.runtimeAnimatorController = originalController;
                animator.enabled = originalAnimatorEnabled;
            }

            Object.DestroyImmediate(exportClip);
            AssetDatabase.Refresh();
        }
    }
}
