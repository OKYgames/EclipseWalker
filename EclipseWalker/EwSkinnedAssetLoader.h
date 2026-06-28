#pragma once

#include <string>

class AnimationLoader;

class EwSkinnedAssetLoader
{
public:
    static bool Load(const std::string& filePath, AnimationLoader& destination);
};
