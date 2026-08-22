#include "UfbxAnimationLoader.h"

#include "AnimationLoader.h"
#include "ThirdParty/ufbx/ufbx.h"

#include <Windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr std::size_t kMaxBones = 256;
    constexpr double kAnimationSampleRate = 30.0;

    std::string ToString(ufbx_string value)
    {
        return std::string(value.data != nullptr ? value.data : "", value.length);
    }

    DirectX::XMFLOAT4X4 ToDirectX(const ufbx_matrix& matrix)
    {
        // ufbx uses column vectors; the renderer uses DirectX row vectors.
        return DirectX::XMFLOAT4X4(
            static_cast<float>(matrix.m00), static_cast<float>(matrix.m10), static_cast<float>(matrix.m20), 0.0f,
            static_cast<float>(matrix.m01), static_cast<float>(matrix.m11), static_cast<float>(matrix.m21), 0.0f,
            static_cast<float>(matrix.m02), static_cast<float>(matrix.m12), static_cast<float>(matrix.m22), 0.0f,
            static_cast<float>(matrix.m03), static_cast<float>(matrix.m13), static_cast<float>(matrix.m23), 1.0f);
    }

    bool IsSameMatrix(const DirectX::XMFLOAT4X4& lhs, const DirectX::XMFLOAT4X4& rhs)
    {
        constexpr float kEpsilon = 0.0001f;
        const float* left = &lhs._11;
        const float* right = &rhs._11;
        for (int index = 0; index < 16; ++index)
        {
            if (std::fabs(left[index] - right[index]) > kEpsilon)
            {
                return false;
            }
        }
        return true;
    }

    std::string FormatError(const ufbx_error& error)
    {
        std::array<char, 4096> buffer = {};
        ufbx_format_error(buffer.data(), buffer.size(), &error);
        return buffer.data();
    }

    void ResetLoader(AnimationLoader& loader)
    {
        loader.m_Vertices.clear();
        loader.m_Indices.clear();
        loader.m_BoneInfo.clear();
        loader.m_BoneMapping.clear();
        loader.m_NumBones = 0;
        loader.m_Animations.clear();
        loader.m_Subsets.clear();
        loader.m_RootNode = {};
    }

    void ReadHierarchy(const ufbx_node* source, NodeData& destination)
    {
        destination.Name = ToString(source->name);
        destination.LocalTransform = ToDirectX(source->node_to_parent);
        destination.Children.resize(source->children.count);
        for (std::size_t index = 0; index < source->children.count; ++index)
        {
            ReadHierarchy(source->children.data[index], destination.Children[index]);
        }
    }

    unsigned int FindOrAddBone(
        AnimationLoader& loader,
        const std::string& name,
        const DirectX::XMFLOAT4X4& offset)
    {
        for (unsigned int index = 0; index < loader.m_BoneInfo.size(); ++index)
        {
            const BoneInfo& existing = loader.m_BoneInfo[index];
            if (existing.Name == name && IsSameMatrix(existing.OffsetMatrix, offset))
            {
                return index;
            }
        }

        if (loader.m_BoneInfo.size() >= kMaxBones)
        {
            return std::numeric_limits<unsigned int>::max();
        }

        const unsigned int index = static_cast<unsigned int>(loader.m_BoneInfo.size());
        BoneInfo info;
        info.id = static_cast<int>(index);
        info.Name = name;
        info.OffsetMatrix = offset;
        loader.m_BoneInfo.push_back(info);
        loader.m_BoneMapping.try_emplace(name, index);
        loader.m_NumBones = static_cast<unsigned int>(loader.m_BoneInfo.size());
        return index;
    }

    void NormalizeWeights(SkinnedVertex& vertex)
    {
        float total = 0.0f;
        for (float weight : vertex.Weights)
        {
            total += weight;
        }

        if (total <= 0.0001f)
        {
            vertex.Weights[0] = 1.0f;
            return;
        }

        for (float& weight : vertex.Weights)
        {
            weight /= total;
        }
    }

    bool BuildMeshInstance(
        AnimationLoader& loader,
        const ufbx_mesh* mesh,
        const ufbx_node* instance)
    {
        const ufbx_skin_deformer* skin = mesh->skin_deformers.count > 0
            ? mesh->skin_deformers.data[0]
            : nullptr;

        std::vector<unsigned int> clusterToBone;
        unsigned int rigidBone = std::numeric_limits<unsigned int>::max();
        if (skin != nullptr)
        {
            clusterToBone.resize(skin->clusters.count, std::numeric_limits<unsigned int>::max());
            for (std::size_t clusterIndex = 0; clusterIndex < skin->clusters.count; ++clusterIndex)
            {
                const ufbx_skin_cluster* cluster = skin->clusters.data[clusterIndex];
                if (cluster->bone_node == nullptr)
                {
                    continue;
                }
                clusterToBone[clusterIndex] = FindOrAddBone(
                    loader,
                    ToString(cluster->bone_node->name),
                    ToDirectX(cluster->geometry_to_bone));
            }
        }
        else
        {
            rigidBone = FindOrAddBone(
                loader,
                ToString(instance->name),
                ToDirectX(instance->geometry_to_node));
        }

        if ((skin == nullptr && rigidBone == std::numeric_limits<unsigned int>::max()) ||
            mesh->max_face_triangles == 0)
        {
            return false;
        }

        std::vector<std::uint32_t> triangleIndices(mesh->max_face_triangles * 3);
        for (std::size_t partIndex = 0; partIndex < mesh->material_parts.count; ++partIndex)
        {
            const ufbx_mesh_part& part = mesh->material_parts.data[partIndex];
            if (part.num_triangles == 0)
            {
                continue;
            }

            SkinnedMeshSubset subset;
            subset.VertexStart = static_cast<unsigned int>(loader.m_Vertices.size());
            subset.IndexStart = static_cast<unsigned int>(loader.m_Indices.size());
            subset.MaterialIndex = static_cast<unsigned int>(part.index);
            subset.Name = ToString(mesh->name) + "_" + std::to_string(partIndex);
            const ufbx_material* material = nullptr;
            if (instance != nullptr && part.index < instance->materials.count)
            {
                material = instance->materials.data[part.index];
            }
            else if (part.index < mesh->materials.count)
            {
                material = mesh->materials.data[part.index];
            }
            if (material != nullptr)
            {
                subset.MaterialName = ToString(material->name);
            }

            for (std::size_t partFaceIndex = 0; partFaceIndex < part.face_indices.count; ++partFaceIndex)
            {
                const ufbx_face face = mesh->faces.data[part.face_indices.data[partFaceIndex]];
                const std::uint32_t triangleCount = ufbx_triangulate_face(
                    triangleIndices.data(), triangleIndices.size(), mesh, face);

                for (std::uint32_t corner = 0; corner < triangleCount * 3; ++corner)
                {
                    const std::uint32_t vertexIndex = triangleIndices[corner];
                    const std::uint32_t logicalVertex = mesh->vertex_indices.data[vertexIndex];
                    const ufbx_vec3 position = ufbx_get_vertex_vec3(&mesh->vertex_position, vertexIndex);
                    const ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, vertexIndex);
                    const ufbx_vec2 uv = mesh->vertex_uv.exists
                        ? ufbx_get_vertex_vec2(&mesh->vertex_uv, vertexIndex)
                        : ufbx_vec2{};
                    const ufbx_vec3 tangent = mesh->vertex_tangent.exists
                        ? ufbx_get_vertex_vec3(&mesh->vertex_tangent, vertexIndex)
                        : ufbx_vec3{ 1.0, 0.0, 0.0 };

                    SkinnedVertex vertex;
                    vertex.Position[0] = static_cast<float>(position.x);
                    vertex.Position[1] = static_cast<float>(position.y);
                    vertex.Position[2] = static_cast<float>(position.z);
                    vertex.Normal[0] = static_cast<float>(normal.x);
                    vertex.Normal[1] = static_cast<float>(normal.y);
                    vertex.Normal[2] = static_cast<float>(normal.z);
                    vertex.TexCoords[0] = static_cast<float>(uv.x);
                    vertex.TexCoords[1] = 1.0f - static_cast<float>(uv.y);
                    vertex.Tangent[0] = static_cast<float>(tangent.x);
                    vertex.Tangent[1] = static_cast<float>(tangent.y);
                    vertex.Tangent[2] = static_cast<float>(tangent.z);

                    if (skin != nullptr && logicalVertex < skin->vertices.count)
                    {
                        const ufbx_skin_vertex skinVertex = skin->vertices.data[logicalVertex];
                        int outputSlot = 0;
                        for (std::size_t weightIndex = 0;
                            weightIndex < skinVertex.num_weights && outputSlot < 4;
                            ++weightIndex)
                        {
                            const ufbx_skin_weight skinWeight =
                                skin->weights.data[skinVertex.weight_begin + weightIndex];
                            if (skinWeight.cluster_index >= clusterToBone.size())
                            {
                                continue;
                            }

                            const unsigned int boneIndex = clusterToBone[skinWeight.cluster_index];
                            if (boneIndex == std::numeric_limits<unsigned int>::max())
                            {
                                continue;
                            }

                            vertex.BoneIndices[outputSlot] = boneIndex;
                            vertex.Weights[outputSlot] = static_cast<float>(skinWeight.weight);
                            ++outputSlot;
                        }
                    }
                    else
                    {
                        vertex.BoneIndices[0] = rigidBone;
                        vertex.Weights[0] = 1.0f;
                    }

                    NormalizeWeights(vertex);
                    loader.m_Indices.push_back(static_cast<unsigned int>(loader.m_Vertices.size()));
                    loader.m_Vertices.push_back(vertex);
                }
            }

            subset.IndexCount = static_cast<unsigned int>(loader.m_Indices.size()) - subset.IndexStart;
            if (subset.IndexCount > 0)
            {
                loader.m_Subsets.push_back(std::move(subset));
            }
        }

        return true;
    }

    bool ProcessMeshes(const ufbx_scene* scene, AnimationLoader& loader)
    {
        bool loadedAnyMesh = false;
        for (std::size_t meshIndex = 0; meshIndex < scene->meshes.count; ++meshIndex)
        {
            const ufbx_mesh* mesh = scene->meshes.data[meshIndex];
            for (std::size_t instanceIndex = 0; instanceIndex < mesh->instances.count; ++instanceIndex)
            {
                loadedAnyMesh |= BuildMeshInstance(loader, mesh, mesh->instances.data[instanceIndex]);
            }
        }
        return loadedAnyMesh;
    }

    bool ProcessAnimations(
        const ufbx_scene* scene,
        const std::string& alias,
        AnimationLoader& loader)
    {
        for (std::size_t stackIndex = 0; stackIndex < scene->anim_stacks.count; ++stackIndex)
        {
            const ufbx_anim_stack* stack = scene->anim_stacks.data[stackIndex];
            ufbx_bake_opts bakeOptions = {};
            bakeOptions.trim_start_time = true;
            bakeOptions.resample_rate = kAnimationSampleRate;
            bakeOptions.maximum_sample_rate = kAnimationSampleRate;

            ufbx_error error = {};
            ufbx_baked_anim* baked = ufbx_bake_anim(scene, stack->anim, &bakeOptions, &error);
            if (baked == nullptr)
            {
                OutputDebugStringA(("[ufbx] Animation bake failed: " + FormatError(error) + "\n").c_str());
                return false;
            }

            AnimationClip clip;
            clip.Name = alias.empty() ? ToString(stack->name) : alias;
            clip.Duration = static_cast<float>((std::max)(0.0, stack->time_end - stack->time_begin));
            clip.TicksPerSecond = 1.0f;
            clip.RootNode = loader.m_RootNode;
            clip.BoneAnimations.reserve(baked->nodes.count);

            double maximumTime = 0.0;
            for (std::size_t nodeIndex = 0; nodeIndex < baked->nodes.count; ++nodeIndex)
            {
                const ufbx_baked_node& source = baked->nodes.data[nodeIndex];
                if (source.typed_id >= scene->nodes.count)
                {
                    continue;
                }

                BoneAnimation channel;
                channel.BoneName = ToString(scene->nodes.data[source.typed_id]->name);
                channel.Positions.reserve(source.translation_keys.count);
                channel.Rotations.reserve(source.rotation_keys.count);
                channel.Scales.reserve(source.scale_keys.count);

                for (std::size_t keyIndex = 0; keyIndex < source.translation_keys.count; ++keyIndex)
                {
                    const ufbx_baked_vec3& sourceKey = source.translation_keys.data[keyIndex];
                    KeyPosition key;
                    key.Position[0] = static_cast<float>(sourceKey.value.x);
                    key.Position[1] = static_cast<float>(sourceKey.value.y);
                    key.Position[2] = static_cast<float>(sourceKey.value.z);
                    key.TimeStamp = sourceKey.time;
                    maximumTime = (std::max)(maximumTime, sourceKey.time);
                    channel.Positions.push_back(key);
                }

                for (std::size_t keyIndex = 0; keyIndex < source.rotation_keys.count; ++keyIndex)
                {
                    const ufbx_baked_quat& sourceKey = source.rotation_keys.data[keyIndex];
                    KeyRotation key;
                    key.Orientation[0] = static_cast<float>(sourceKey.value.x);
                    key.Orientation[1] = static_cast<float>(sourceKey.value.y);
                    key.Orientation[2] = static_cast<float>(sourceKey.value.z);
                    key.Orientation[3] = static_cast<float>(sourceKey.value.w);
                    key.TimeStamp = sourceKey.time;
                    maximumTime = (std::max)(maximumTime, sourceKey.time);
                    channel.Rotations.push_back(key);
                }

                for (std::size_t keyIndex = 0; keyIndex < source.scale_keys.count; ++keyIndex)
                {
                    const ufbx_baked_vec3& sourceKey = source.scale_keys.data[keyIndex];
                    KeyScale key;
                    key.Scale[0] = static_cast<float>(sourceKey.value.x);
                    key.Scale[1] = static_cast<float>(sourceKey.value.y);
                    key.Scale[2] = static_cast<float>(sourceKey.value.z);
                    key.TimeStamp = sourceKey.time;
                    maximumTime = (std::max)(maximumTime, sourceKey.time);
                    channel.Scales.push_back(key);
                }

                clip.BoneAnimations.push_back(std::move(channel));
            }

            clip.Duration = static_cast<float>((std::max)(static_cast<double>(clip.Duration), maximumTime));
            loader.m_Animations.push_back(std::move(clip));
            ufbx_free_baked_anim(baked);
        }

        return true;
    }
}

bool UfbxAnimationLoader::Load(
    const std::string& filePath,
    const std::string& alias,
    bool loadAnimations,
    bool allowAnimationOnly,
    AnimationLoader& destination)
{
    ufbx_load_opts options = {};
    options.generate_missing_normals = true;
    options.clean_skin_weights = true;
    options.ignore_missing_external_files = true;
    options.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_PRESERVE;
    options.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    options.pivot_handling = UFBX_PIVOT_HANDLING_RETAIN;

    ufbx_error error = {};
    ufbx_scene* scene = ufbx_load_file(filePath.c_str(), &options, &error);
    if (scene == nullptr)
    {
        OutputDebugStringA(("[ufbx] Load failed: " + FormatError(error) + "\n").c_str());
        return false;
    }

    AnimationLoader loaded;
    ResetLoader(loaded);
    ReadHierarchy(scene->root_node, loaded.m_RootNode);
    const bool meshLoaded = ProcessMeshes(scene, loaded);
    const bool animationLoaded = !loadAnimations || ProcessAnimations(scene, alias, loaded);
    const bool hasAnimationClips = !loaded.m_Animations.empty();

    std::ostringstream log;
    log << "[ufbx] Parsed " << filePath
        << " nodes=" << scene->nodes.count
        << " meshes=" << scene->meshes.count
        << " animStacks=" << scene->anim_stacks.count
        << " outputVertices=" << loaded.m_Vertices.size()
        << " outputBones=" << loaded.m_BoneInfo.size()
        << " outputClips=" << loaded.m_Animations.size()
        << " animationOnly=" << (allowAnimationOnly ? "true" : "false") << "\n";
    OutputDebugStringA(log.str().c_str());

    ufbx_free_scene(scene);
    const bool validContent = meshLoaded || (allowAnimationOnly && hasAnimationClips);
    if (!validContent || !animationLoaded)
    {
        return false;
    }

    destination = std::move(loaded);
    return true;
}
