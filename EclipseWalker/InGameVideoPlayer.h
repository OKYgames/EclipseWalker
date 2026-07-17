#pragma once

#include <Windows.h>
#include <mfplay.h>
#include <wrl.h>
#include <string>

class InGameVideoPlayer final : public IMFPMediaPlayerCallback
{
public:
    InGameVideoPlayer();
    ~InGameVideoPlayer();

    bool Initialize(HWND parentWindow);
    bool Play(const std::wstring& videoPath, float videoDurationSeconds, float preBlackSeconds = 0.0f, float postBlackSeconds = 0.0f);
    void Stop();
    void Update(float dt);
    void ResizeToParent();
    bool IsPlaying() const { return mPlaying; }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* eventHeader) override;

private:
    enum class PlaybackPhase
    {
        None,
        FadeToBlack,
        PrepareVideo,
        FadeToVideo,
        Video,
        FadeToBlackAfterVideo,
        PostHold,
        FadeToGame
    };

    bool PrepareVideo();
    bool StartPreparedVideo();
    void StopVideoOnly();
    void SetFadeAlpha(BYTE alpha);
    void ShowFadeWindow(bool show);
    void BeginPhase(PlaybackPhase phase, float durationSeconds);

    HWND mParentWindow = nullptr;
    HWND mVideoWindow = nullptr;
    HWND mFadeWindow = nullptr;
    Microsoft::WRL::ComPtr<IMFPMediaPlayer> mPlayer;
    std::wstring mPendingVideoPath;
    long mRefCount = 1;
    float mRemainingSeconds = 0.0f;
    float mPhaseDurationSeconds = 0.0f;
    float mVideoDurationSeconds = 0.0f;
    float mPreBlackSeconds = 0.0f;
    float mPostBlackSeconds = 0.0f;
    bool mMfStarted = false;
    bool mPlaying = false;
    bool mPrepared = false;
    bool mVideoStarted = false;
    PlaybackPhase mPhase = PlaybackPhase::None;
};
