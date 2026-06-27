#pragma once
#include "GameObject.h"
#include "Material.h"
#include "Player.h"
#include <DescriptorHeap.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <string>
#include <vector>
#include <memory>

class EclipseWalkerGame;
struct RenderItem;

class UIManager
{
public:
    struct StageClearEntry
    {
        std::wstring Name;
        int Damage = 0;
    };

    UIManager(EclipseWalkerGame* game);
    ~UIManager();

    // UI 시스템 초기화 및 생성
    void BuildInGameUI();

	// 플래시 이펙트 관련 함수들  
    void InitializeEffect(Material* flashMat, Material* bgMat, GameObject* flashObj, GameObject* screenBgObj);
    void TriggerFlashEffect();
    void TriggerLevelUpFlashEffect(PlayerClass playerClass, int newLevel);
    void UpdateEffect(float dt);

    // 매 프레임 체력/마나/랜턴 비율에 맞춰 UI 업데이트
    void Update(float currentHp, float maxHp, float currentMp, float maxMp, float currentLantern, float maxLantern, float currentDashCooldown, float maxDashCooldown, float currentExpRatio);
    void SetSkillCooldowns(float currentSkill1Cooldown, float maxSkill1Cooldown, float currentSkill2Cooldown, float maxSkill2Cooldown);
    void UpdateBossHealthBar(float currentHp, float maxHp);
    void HideBossHealthBar();
    void ShowMirrorCrackWarning(float progress);
    void HideMirrorCrackWarning();
    void SetChatBoxState(bool active, bool hasMessages);
    void SetRespawnScreenState(bool active, float countdownRemaining, bool buttonEnabled);
    void SetStageClearScreenState(bool active, float clearTimeSeconds, const std::vector<StageClearEntry>& entries);
    bool IsRespawnScreenActive() const { return mRespawnScreenActive; }
    bool IsStageClearScreenActive() const { return mStageClearScreenActive; }
    bool IsRespawnButtonHovered() const;
    void DrawCooldownOverlay();

    // UI 전용 객체 리스트 반환 (렌더링할 때 사용)
    const std::vector<std::unique_ptr<GameObject>>& GetUIObjects() const { return mUIObjects; }

 

private:
    struct CooldownWidget
    {
        GameObject* Back = nullptr;
        GameObject* Fill = nullptr;
        GameObject* Frame = nullptr;
        GameObject* Icon = nullptr;
        RenderItem* FillRitem = nullptr;
        RenderItem* IconRitem = nullptr;
        Material* BackMat = nullptr;
        Material* FillMat = nullptr;
        Material* IconWarriorMat = nullptr;
        Material* IconMageMat = nullptr;
        Material* IconArcherMat = nullptr;
        float CenterX = 0.0f;
        float CenterY = 0.0f;
        float CooldownRemaining = 0.0f;
        float CooldownDuration = 0.0f;
        float CooldownRatio = 0.0f;
    };

    EclipseWalkerGame* mGame;
    std::vector<std::unique_ptr<GameObject>> mUIObjects;

    GameObject* mHpBarFill = nullptr;
    GameObject* mMpBarFill = nullptr;
    GameObject* mExpBarBack = nullptr;
    GameObject* mExpBarFill = nullptr;
    GameObject* mHpBarDelay = nullptr;
    GameObject* mMpBarDelay = nullptr;
    GameObject* mHpBarGloss = nullptr;
    GameObject* mMpBarGloss = nullptr;
    GameObject* mHpMpFrame = nullptr;
    GameObject* mHpMpGloss = nullptr;
    GameObject* mBossHpFrame = nullptr;
    GameObject* mBossHpBack = nullptr;
    GameObject* mBossHpDelay = nullptr;
    GameObject* mBossHpFill = nullptr;
    GameObject* mBossHpGloss = nullptr;
    GameObject* mBossHpLeftCap = nullptr;
    GameObject* mBossHpRightCap = nullptr;
    RenderItem* mLanternRingFillRitem = nullptr;
    GameObject* mLanternOrbGlow = nullptr;
    GameObject* mLanternOrbCore = nullptr;
    RenderItem* mClassEmblemRitem = nullptr;
    RenderItem* mSkillIcon1Ritem = nullptr;
    RenderItem* mSkillIcon2Ritem = nullptr;
    Material* mBossHpBackMat = nullptr;
    Material* mBossHpDelayMat = nullptr;
    Material* mBossHpFillMat = nullptr;
    Material* mBossHpGlossMat = nullptr;
    Material* mLanternRingMat = nullptr;
    Material* mLanternGlowMat = nullptr;
    Material* mLanternIconMat = nullptr;
    Material* mMirrorCrackMat = nullptr;
    Material* mClassEmblemWarriorMat = nullptr;
    Material* mClassEmblemMageMat = nullptr;
    Material* mClassEmblemArcherMat = nullptr;
    Material* mSkillIcon1WarriorMat = nullptr;
    Material* mSkillIcon2WarriorMat = nullptr;
    Material* mSkillIcon1MageMat = nullptr;
    Material* mSkillIcon2MageMat = nullptr;
    Material* mSkillIcon1ArcherMat = nullptr;
    Material* mSkillIcon2ArcherMat = nullptr;
    GameObject* mMirrorCrackObj = nullptr;

    Material* mFlashMat = nullptr;       // 일렁이는 노이즈 재질
    Material* mBgMat = nullptr;          // 화면 전체 보라색 배경 재질
    GameObject* mFlashObj = nullptr;     // 화면을 덮는 네모 도화지 객체
    GameObject* mScreenBgObj = nullptr;  // 배경 가림막
    GameObject* mChatLogBg = nullptr;
    GameObject* mChatInputBg = nullptr;
    GameObject* mRespawnOverlayBg = nullptr;
    GameObject* mRespawnButtonBg = nullptr;
    GameObject* mRespawnButtonFrame = nullptr;
    GameObject* mStageClearOverlayBg = nullptr;
    GameObject* mStageClearPanelBg = nullptr;
    GameObject* mStageClearPanelFrame = nullptr;
    GameObject* mStageClearBannerBg = nullptr;
    Material* mChatLogMat = nullptr;
    Material* mChatInputMat = nullptr;
    Material* mRespawnOverlayMat = nullptr;
    Material* mRespawnButtonMat = nullptr;
    Material* mRespawnButtonFrameMat = nullptr;
    Material* mStageClearOverlayMat = nullptr;
    Material* mStageClearPanelMat = nullptr;
    Material* mStageClearPanelFrameMat = nullptr;
    Material* mStageClearBannerMat = nullptr;
    CooldownWidget mSkill1CooldownWidget;
    CooldownWidget mSkill2CooldownWidget;
    CooldownWidget mDashCooldownWidget;
    std::unique_ptr<DirectX::DescriptorHeap> mCooldownTextHeap;
    std::unique_ptr<DirectX::SpriteBatch> mCooldownTextBatch;
    std::unique_ptr<DirectX::SpriteFont> mCooldownTextFont;

    bool mIsFlashActive = false;         // 이펙트 켜짐 여부
    float mCurrentTime = 0.0f;           // 이펙트 진행 시간
    float mFlashDuration = 1.55f;        // 랜턴 점등 + 가림막 유지 시간
    bool mUseShortFlashProfile = false;
    float mFlashPeakAlpha = 1.0f;
    float mBgPeakAlpha = 1.0f;
    DirectX::XMFLOAT4 mFlashBaseColor = { 1.0f, 0.95f, 0.82f, 0.0f };
    DirectX::XMFLOAT4 mBgBaseColor = { 0.95f, 0.90f, 0.72f, 0.0f };
    float mHpDelayRatio = 1.0f;
    float mMpDelayRatio = 1.0f;
    float mBossHpDelayRatio = 1.0f;
    int mBossHpVisibleLayer = 0;
    float mLanternDelayRatio = 0.0f;
    float mLanternGlowTime = 0.0f;
    bool mMirrorCrackWarningActive = false;
    float mMirrorCrackWarningProgress = 0.0f;
    float mMirrorCrackWarningTime = 0.0f;
    float mDebugHudDrainTime = 0.0f;
    bool mRespawnScreenActive = false;
    bool mRespawnButtonEnabled = false;
    float mRespawnCountdownRemaining = 0.0f;
    bool mStageClearScreenActive = false;
    float mStageClearTimeSeconds = 0.0f;
    std::vector<StageClearEntry> mStageClearEntries;

    void UpdateCooldownWidget(CooldownWidget& widget);
    void UpdateSkillIconMaterials();
    void DrawCooldownWidgetText(const CooldownWidget& widget);
    void DrawRespawnOverlayText();
    void DrawStageClearOverlayText();
};
