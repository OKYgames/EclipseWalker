#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: AssimpSceneConvert <input> <output> [format-id]\n";
        return 1;
    }

    const char* formatId = argc >= 4 ? argv[3] : "glb2";
    const bool preservePivots =
        argc >= 5 && std::string(argv[4]) == "preserve-pivots";

    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, preservePivots);

    const aiScene* scene = importer.ReadFile(
        argv[1],
        aiProcess_Triangulate |
        aiProcess_LimitBoneWeights |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_ValidateDataStructure);

    if (scene == nullptr)
    {
        std::cerr << "Import failed: " << importer.GetErrorString() << "\n";
        return 2;
    }

    std::cout << "Imported meshes=" << scene->mNumMeshes
        << " animations=" << scene->mNumAnimations << "\n";

    Assimp::Exporter exporter;
    const aiReturn result = exporter.Export(scene, formatId, argv[2]);
    if (result != AI_SUCCESS)
    {
        std::cerr << "Export failed: " << exporter.GetErrorString() << "\n";
        std::cerr << "Available formats:\n";
        for (size_t i = 0; i < exporter.GetExportFormatCount(); ++i)
        {
            const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
            std::cerr << "  " << desc->id << " (." << desc->fileExtension << ")\n";
        }
        return 3;
    }

    std::cout << "Exported " << argv[2] << " using " << formatId << "\n";
    return 0;
}
