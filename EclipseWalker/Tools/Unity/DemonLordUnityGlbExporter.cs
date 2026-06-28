using System.IO;
using UnityEditor;
using UnityEngine;
using UnityGLTF;

public class DemonLordUnityGlbExporter : EditorWindow
{
    public GameObject rootObject;
    public AnimationClip clip;
    public string outputPath =
        @"C:\Users\김범진\Desktop\BumJin\KPU\졸업작품\EclipseWalker\EclipseWalker\Models\Boss\SK_DemonLord_UnityDirect.glb";

    [MenuItem("Tools/EclipseWalker/Export DemonLord Direct GLB")]
    public static void Open()
    {
        GetWindow<DemonLordUnityGlbExporter>("DemonLord GLB Exporter");
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
            "SK_DemonLord_UnityDirect.glb"));
    }

    private void OnGUI()
    {
        rootObject = (GameObject)EditorGUILayout.ObjectField(
            "Root Object", rootObject, typeof(GameObject), true);
        clip = (AnimationClip)EditorGUILayout.ObjectField(
            "Animation Clip", clip, typeof(AnimationClip), false);
        outputPath = EditorGUILayout.TextField("Output Path", outputPath);

        if (GUILayout.Button("Export Direct GLB"))
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
        string fileName = Path.GetFileNameWithoutExtension(outputPath);
        if (string.IsNullOrEmpty(directory) || string.IsNullOrEmpty(fileName))
        {
            Debug.LogError("Output Path must include a directory and file name.");
            return;
        }
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
        exportClip.name = "DemonLord_Idle_UnityDirect";
        exportClip.legacy = true;

        GLTFSettings settings = GLTFSettings.GetDefaultSettings();
        settings.ExportAnimations = true;
        settings.BakeAnimationSpeed = true;
        settings.BakeSkinnedMeshes = false;
        settings.ExportNames = true;
        settings.ExportDisabledGameObjects = true;
        settings.UniqueAnimationNames = true;

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

            var context = new ExportContext(settings);
            var exporter = new GLTFSceneExporter(
                new[] { rootObject.transform },
                context);
            exporter.SaveGLB(directory, fileName);

            Debug.Log(
                "DemonLord direct GLB export complete: " + outputPath + "\n" +
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
            Object.DestroyImmediate(settings);
            AssetDatabase.Refresh();
        }
    }
}
