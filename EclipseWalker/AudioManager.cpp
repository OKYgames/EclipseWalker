#include "AudioManager.h"

#include <Windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <filesystem>

namespace
{
    int ToMciVolume(float volumeScale)
    {
        const float clampedVolumeScale = (std::clamp)(volumeScale, 0.0f, 1.0f);
        return static_cast<int>(clampedVolumeScale * 1000.0f);
    }

    std::wstring SanitizeForMci(std::wstring value)
    {
        std::replace(value.begin(), value.end(), L'"', L'\'');
        return value;
    }

    bool PathExists(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        return std::filesystem::exists(path, errorCode);
    }

    std::filesystem::path GetModuleDirectory()
    {
        std::array<wchar_t, MAX_PATH> buffer = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
        {
            return std::filesystem::current_path();
        }

        return std::filesystem::path(buffer.data()).parent_path();
    }

    bool SendMci(const std::wstring& command, wchar_t* outBuffer = nullptr, size_t outBufferCount = 0)
    {
        return mciSendStringW(
            command.c_str(),
            outBuffer,
            static_cast<UINT>(outBufferCount),
            nullptr) == 0;
    }
}

AudioManager& AudioManager::Get()
{
    static AudioManager instance;
    return instance;
}

void AudioManager::Update(float dt)
{
    for (size_t index = mActiveClips.size(); index > 0; --index)
    {
        ActiveClip& clip = mActiveClips[index - 1];
        if (clip.Looping)
        {
            continue;
        }

        clip.RemainingSeconds -= dt;
        if (clip.RemainingSeconds <= 0.0f)
        {
            CloseClip(index - 1);
        }
    }
}

AudioManager::ClipHandle AudioManager::PlayEffect(const std::wstring& path, float volumeScale)
{
    return PlayInternal(path, volumeScale, false);
}

AudioManager::ClipHandle AudioManager::PlayLoop(const std::wstring& path, float volumeScale)
{
    return PlayInternal(path, volumeScale, true);
}

void AudioManager::SetVolume(ClipHandle handle, float volumeScale)
{
    if (handle == InvalidClipHandle)
    {
        return;
    }

    for (const ActiveClip& clip : mActiveClips)
    {
        if (clip.Handle == handle)
        {
            SendMci(L"setaudio " + clip.Alias + L" volume to " + std::to_wstring(ToMciVolume(volumeScale)));
            return;
        }
    }
}

AudioManager::ClipHandle AudioManager::PlayInternal(const std::wstring& path, float volumeScale, bool looping)
{
    const std::wstring resolvedPath = ResolvePath(path);
    if (resolvedPath.empty())
    {
        return InvalidClipHandle;
    }

    const ClipHandle handle = mNextAliasId++;
    const std::wstring alias = L"sfx_" + std::to_wstring(handle);
    const std::wstring openCommand =
        L"open \"" + SanitizeForMci(resolvedPath) + L"\" type mpegvideo alias " + alias;
    if (!SendMci(openCommand))
    {
        return InvalidClipHandle;
    }

    SendMci(L"setaudio " + alias + L" volume to " + std::to_wstring(ToMciVolume(volumeScale)));

    std::array<wchar_t, 64> lengthBuffer = {};
    float clipLengthSeconds = 5.0f;
    if (SendMci(L"status " + alias + L" length", lengthBuffer.data(), lengthBuffer.size()))
    {
        const long lengthMs = std::wcstol(lengthBuffer.data(), nullptr, 10);
        if (lengthMs > 0)
        {
            clipLengthSeconds = static_cast<float>(lengthMs) / 1000.0f + 0.2f;
        }
    }

    const std::wstring playCommand = looping
        ? (L"play " + alias + L" repeat")
        : (L"play " + alias + L" from 0");
    if (!SendMci(playCommand))
    {
        SendMci(L"close " + alias);
        return InvalidClipHandle;
    }

    mActiveClips.push_back({ handle, alias, clipLengthSeconds, looping });
    return handle;
}

void AudioManager::StopEffect(ClipHandle handle)
{
    if (handle == InvalidClipHandle)
    {
        return;
    }

    for (size_t index = 0; index < mActiveClips.size(); ++index)
    {
        if (mActiveClips[index].Handle == handle)
        {
            CloseClip(index);
            return;
        }
    }
}

void AudioManager::StopAll()
{
    for (size_t index = mActiveClips.size(); index > 0; --index)
    {
        CloseClip(index - 1);
    }
}

std::wstring AudioManager::ResolvePath(const std::wstring& path) const
{
    std::filesystem::path requested(path);
    if (requested.is_absolute() && PathExists(requested))
    {
        return std::filesystem::absolute(requested).make_preferred().wstring();
    }

    const std::array<std::filesystem::path, 2> bases =
    {
        std::filesystem::current_path(),
        GetModuleDirectory()
    };

    for (const auto& base : bases)
    {
        std::filesystem::path probeBase = base;
        for (int depth = 0; depth < 6; ++depth)
        {
            const std::filesystem::path candidate = probeBase / requested;
            if (PathExists(candidate))
            {
                return std::filesystem::absolute(candidate).make_preferred().wstring();
            }

            if (!probeBase.has_parent_path())
            {
                break;
            }

            const std::filesystem::path parent = probeBase.parent_path();
            if (parent == probeBase)
            {
                break;
            }

            probeBase = parent;
        }
    }

    return L"";
}

void AudioManager::CloseClip(size_t index)
{
    if (index >= mActiveClips.size())
    {
        return;
    }

    SendMci(L"close " + mActiveClips[index].Alias);
    mActiveClips.erase(mActiveClips.begin() + static_cast<std::ptrdiff_t>(index));
}
