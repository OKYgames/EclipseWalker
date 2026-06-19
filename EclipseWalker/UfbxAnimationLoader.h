#pragma once

#include <string>

class AnimationLoader;

class UfbxAnimationLoader
{
public:
    static bool Load(
        const std::string& filePath,
        const std::string& alias,
        bool loadAnimations,
        AnimationLoader& destination);
};
