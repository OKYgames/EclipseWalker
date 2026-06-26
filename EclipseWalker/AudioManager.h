#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class AudioManager
{
public:
    using ClipHandle = std::uint32_t;
    static constexpr ClipHandle InvalidClipHandle = 0;

    static AudioManager& Get();

    void Update(float dt);
    ClipHandle PlayEffect(const std::wstring& path, float volumeScale = 1.0f);
    ClipHandle PlayLoop(const std::wstring& path, float volumeScale = 1.0f);
    void SetVolume(ClipHandle handle, float volumeScale);
    void StopEffect(ClipHandle handle);
    void StopAll();

private:
    struct ActiveClip
    {
        ClipHandle Handle = InvalidClipHandle;
        std::wstring Alias;
        float RemainingSeconds = 0.0f;
        bool Looping = false;
    };

private:
    ClipHandle PlayInternal(const std::wstring& path, float volumeScale, bool looping);
    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    std::wstring ResolvePath(const std::wstring& path) const;
    void CloseClip(size_t index);

private:
    std::vector<ActiveClip> mActiveClips;
    ClipHandle mNextAliasId = 1;
};
