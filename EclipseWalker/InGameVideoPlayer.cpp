#include "InGameVideoPlayer.h"

#include <mfapi.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfuuid.lib")

namespace
{
    std::filesystem::path GetExecutableDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
        if (length == 0 || length >= std::size(path))
        {
            return {};
        }

        return std::filesystem::path(path).parent_path();
    }

    std::filesystem::path ResolveVideoPath(const std::wstring& videoPath)
    {
        const std::filesystem::path requested(videoPath);
        if (requested.is_absolute() && std::filesystem::exists(requested))
        {
            return requested;
        }

        const std::filesystem::path currentRelative = std::filesystem::absolute(requested);
        if (std::filesystem::exists(currentRelative))
        {
            return currentRelative;
        }

        const std::filesystem::path exeDir = GetExecutableDirectory();
        if (!exeDir.empty())
        {
            const std::filesystem::path exeRelative = exeDir / requested;
            if (std::filesystem::exists(exeRelative))
            {
                return exeRelative;
            }

            const std::filesystem::path projectRelative = exeDir / L"..\\.." / requested;
            if (std::filesystem::exists(projectRelative))
            {
                return std::filesystem::weakly_canonical(projectRelative);
            }
        }

        return currentRelative;
    }
}

InGameVideoPlayer::InGameVideoPlayer() = default;

InGameVideoPlayer::~InGameVideoPlayer()
{
    Stop();

    if (mVideoWindow != nullptr)
    {
        DestroyWindow(mVideoWindow);
        mVideoWindow = nullptr;
    }
    if (mFadeWindow != nullptr)
    {
        DestroyWindow(mFadeWindow);
        mFadeWindow = nullptr;
    }

    if (mMfStarted)
    {
        MFShutdown();
        mMfStarted = false;
    }
}

bool InGameVideoPlayer::Initialize(HWND parentWindow)
{
    if (parentWindow == nullptr)
    {
        return false;
    }

    mParentWindow = parentWindow;

    if (!mMfStarted)
    {
        if (FAILED(MFStartup(MF_VERSION)))
        {
            OutputDebugStringA("[InGameVideoPlayer] MFStartup failed\n");
            return false;
        }
        mMfStarted = true;
    }

    if (mVideoWindow == nullptr)
    {
        mVideoWindow = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD,
            0,
            0,
            1,
            1,
            mParentWindow,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (mVideoWindow == nullptr)
        {
            OutputDebugStringA("[InGameVideoPlayer] CreateWindowEx failed\n");
            return false;
        }
    }

    ResizeToParent();
    ShowWindow(mVideoWindow, SW_HIDE);
    return true;
}

bool InGameVideoPlayer::Play(const std::wstring& videoPath, float videoDurationSeconds, float preBlackSeconds, float postBlackSeconds)
{
    UNREFERENCED_PARAMETER(preBlackSeconds);
    UNREFERENCED_PARAMETER(postBlackSeconds);

    if (!Initialize(mParentWindow))
    {
        return false;
    }

    Stop();

    std::filesystem::path absolutePath = ResolveVideoPath(videoPath);
    if (!std::filesystem::exists(absolutePath))
    {
        OutputDebugStringA("[InGameVideoPlayer] video file not found\n");
        return false;
    }

    mPendingVideoPath = absolutePath.wstring();
    mVideoDurationSeconds = videoDurationSeconds > 0.0f ? videoDurationSeconds : 0.1f;
    mPreBlackSeconds = 0.0f;
    mPostBlackSeconds = 0.0f;
    mPlaying = true;
    mPrepared = false;
    mVideoStarted = false;

    ResizeToParent();
    ShowWindow(mVideoWindow, SW_SHOW);
    SetWindowPos(mVideoWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    IMFPMediaPlayer* rawPlayer = nullptr;
    const HRESULT hr = MFPCreateMediaPlayer(
        mPendingVideoPath.c_str(),
        TRUE,
        0,
        this,
        mVideoWindow,
        &rawPlayer);
    if (FAILED(hr) || rawPlayer == nullptr)
    {
        ShowWindow(mVideoWindow, SW_HIDE);
        mPhase = PlaybackPhase::None;
        mPlaying = false;
        OutputDebugStringA("[InGameVideoPlayer] MFPCreateMediaPlayer failed\n");
        return false;
    }

    mPlayer.Attach(rawPlayer);
    mPrepared = true;
    mVideoStarted = true;
    BeginPhase(PlaybackPhase::Video, mVideoDurationSeconds);
    return true;
}

bool InGameVideoPlayer::PrepareVideo()
{
    return mPrepared || mPlayer != nullptr;
}

bool InGameVideoPlayer::StartPreparedVideo()
{
    if (!mPrepared && !PrepareVideo())
    {
        return false;
    }

    if (mPlayer == nullptr)
    {
        return false;
    }

    if (!mVideoStarted)
    {
        const HRESULT hr = mPlayer->Play();
        if (FAILED(hr))
        {
            OutputDebugStringA("[InGameVideoPlayer] Play failed; continuing because autoplay may already be active\n");
        }
        mVideoStarted = true;
    }
    return true;
}

void InGameVideoPlayer::StopVideoOnly()
{
    if (mPlayer != nullptr)
    {
        mPlayer->Stop();
        mPlayer->Shutdown();
        mPlayer.Reset();
    }
    mPrepared = false;
    mVideoStarted = false;
}

void InGameVideoPlayer::Stop()
{
    StopVideoOnly();

    if (mVideoWindow != nullptr)
    {
        ShowWindow(mVideoWindow, SW_HIDE);
    }
    if (mFadeWindow != nullptr)
    {
        ShowFadeWindow(false);
        SetFadeAlpha(0);
    }

    mPendingVideoPath.clear();
    mRemainingSeconds = 0.0f;
    mPhaseDurationSeconds = 0.0f;
    mVideoDurationSeconds = 0.0f;
    mPreBlackSeconds = 0.0f;
    mPostBlackSeconds = 0.0f;
    mPlaying = false;
    mPhase = PlaybackPhase::None;
}

void InGameVideoPlayer::Update(float dt)
{
    if (!mPlaying)
    {
        return;
    }

    ResizeToParent();

    mRemainingSeconds -= dt;
    if (mRemainingSeconds <= 0.0f)
    {
        Stop();
        return;
    }
}

void InGameVideoPlayer::ResizeToParent()
{
    if (mParentWindow == nullptr || mVideoWindow == nullptr)
    {
        return;
    }

    RECT rect = {};
    if (!GetClientRect(mParentWindow, &rect))
    {
        return;
    }

    SetWindowPos(
        mVideoWindow,
        HWND_TOP,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOACTIVATE);

    if (mFadeWindow != nullptr)
    {
        SetWindowPos(
            mFadeWindow,
            HWND_TOP,
            0,
            0,
            rect.right - rect.left,
            rect.bottom - rect.top,
            SWP_NOACTIVATE);
    }
}

void InGameVideoPlayer::SetFadeAlpha(BYTE alpha)
{
    if (mFadeWindow != nullptr)
    {
        SetLayeredWindowAttributes(mFadeWindow, 0, alpha, LWA_ALPHA);
        InvalidateRect(mFadeWindow, nullptr, TRUE);
    }
}

void InGameVideoPlayer::ShowFadeWindow(bool show)
{
    if (mFadeWindow != nullptr)
    {
        ShowWindow(mFadeWindow, show ? SW_SHOW : SW_HIDE);
    }
}

void InGameVideoPlayer::BeginPhase(PlaybackPhase phase, float durationSeconds)
{
    mPhase = phase;
    mPhaseDurationSeconds = (std::max)(0.0f, durationSeconds);
    mRemainingSeconds = mPhaseDurationSeconds;
}

STDMETHODIMP InGameVideoPlayer::QueryInterface(REFIID riid, void** ppvObject)
{
    if (ppvObject == nullptr)
    {
        return E_POINTER;
    }

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFPMediaPlayerCallback))
    {
        *ppvObject = static_cast<IMFPMediaPlayerCallback*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) InGameVideoPlayer::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&mRefCount));
}

STDMETHODIMP_(ULONG) InGameVideoPlayer::Release()
{
    const long count = InterlockedDecrement(&mRefCount);
    return static_cast<ULONG>((std::max)(count, 1L));
}

void STDMETHODCALLTYPE InGameVideoPlayer::OnMediaPlayerEvent(MFP_EVENT_HEADER* eventHeader)
{
    if (eventHeader != nullptr && FAILED(eventHeader->hrEvent))
    {
        OutputDebugStringA("[InGameVideoPlayer] media player event failed\n");
    }
}
