#include "../ThirdParty/ufbx/ufbx.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "Usage: UfbxAssetInspector <file>\n");
        return 2;
    }

    ufbx_load_opts options = {};
    options.generate_missing_normals = true;
    options.clean_skin_weights = true;
    options.ignore_missing_external_files = true;
    options.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_PRESERVE;
    options.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    options.pivot_handling = UFBX_PIVOT_HANDLING_RETAIN;

    ufbx_error error = {};
    ufbx_scene* scene = ufbx_load_file(argv[1], &options, &error);
    if (scene == nullptr)
    {
        char buffer[4096] = {};
        ufbx_format_error(buffer, sizeof(buffer), &error);
        std::fprintf(stderr, "%s\n", buffer);
        return 1;
    }

    std::printf(
        "nodes=%zu meshes=%zu animStacks=%zu fps=%.3f\n",
        scene->nodes.count,
        scene->meshes.count,
        scene->anim_stacks.count,
        scene->settings.frames_per_second);

    std::size_t totalClusters = 0;
    std::size_t totalWeightedVertices = 0;
    for (std::size_t meshIndex = 0; meshIndex < scene->meshes.count; ++meshIndex)
    {
        const ufbx_mesh* mesh = scene->meshes.data[meshIndex];
        std::size_t clusters = 0;
        std::size_t weightedVertices = 0;
        for (std::size_t skinIndex = 0; skinIndex < mesh->skin_deformers.count; ++skinIndex)
        {
            const ufbx_skin_deformer* skin = mesh->skin_deformers.data[skinIndex];
            clusters += skin->clusters.count;
            for (std::size_t vertexIndex = 0; vertexIndex < skin->vertices.count; ++vertexIndex)
            {
                if (skin->vertices.data[vertexIndex].num_weights > 0)
                {
                    ++weightedVertices;
                }
            }
        }

        totalClusters += clusters;
        totalWeightedVertices += weightedVertices;
        std::printf(
            "mesh[%zu]=%.*s vertices=%zu triangles=%zu instances=%zu parts=%zu skins=%zu clusters=%zu weighted=%zu\n",
            meshIndex,
            static_cast<int>(mesh->name.length),
            mesh->name.data,
            mesh->num_vertices,
            mesh->num_triangles,
            mesh->instances.count,
            mesh->material_parts.count,
            mesh->skin_deformers.count,
            clusters,
            weightedVertices);
    }

    std::size_t totalBakedNodes = 0;
    std::size_t totalBakedKeys = 0;
    for (std::size_t stackIndex = 0; stackIndex < scene->anim_stacks.count; ++stackIndex)
    {
        const ufbx_anim_stack* stack = scene->anim_stacks.data[stackIndex];
        ufbx_bake_opts bakeOptions = {};
        bakeOptions.trim_start_time = true;
        bakeOptions.resample_rate = 30.0;
        bakeOptions.maximum_sample_rate = 30.0;

        ufbx_baked_anim* baked = ufbx_bake_anim(scene, stack->anim, &bakeOptions, &error);
        if (baked == nullptr)
        {
            char buffer[4096] = {};
            ufbx_format_error(buffer, sizeof(buffer), &error);
            std::fprintf(stderr, "Bake failed: %s\n", buffer);
            ufbx_free_scene(scene);
            return 1;
        }

        std::size_t keys = 0;
        for (std::size_t nodeIndex = 0; nodeIndex < baked->nodes.count; ++nodeIndex)
        {
            const ufbx_baked_node& node = baked->nodes.data[nodeIndex];
            keys += node.translation_keys.count;
            keys += node.rotation_keys.count;
            keys += node.scale_keys.count;
        }
        totalBakedNodes += baked->nodes.count;
        totalBakedKeys += keys;
        std::printf(
            "animation[%zu]=%.*s duration=%.3f bakedNodes=%zu keys=%zu\n",
            stackIndex,
            static_cast<int>(stack->name.length),
            stack->name.data,
            stack->time_end - stack->time_begin,
            baked->nodes.count,
            keys);
        ufbx_free_baked_anim(baked);
    }

    std::printf(
        "summary clusters=%zu weightedVertices=%zu bakedNodes=%zu bakedKeys=%zu\n",
        totalClusters,
        totalWeightedVertices,
        totalBakedNodes,
        totalBakedKeys);

    ufbx_free_scene(scene);
    return totalClusters > 0 && totalWeightedVertices > 0 ? 0 : 3;
}
