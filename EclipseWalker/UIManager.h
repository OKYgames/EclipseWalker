#pragma once
#include "GameObject.h"
#include "Material.h"
#include <vector>
#include <memory>

class EclipseWalkerGame;
struct RenderItem;

class UIManager
{
public:
    UIManager(EclipseWalkerGame* game);
    ~UIManager();

    // UI 시스템 초기화 및 생성
    void BuildInGameUI();

	// 플래시 이펙트 관련 함수들  
    void InitializeEffect(Material* flashMat, Material* bgMat, GameObject* flashObj, GameObject* screenBgObj);
    void TriggerFlashEffect();
    void UpdateEffect(float dt);

    // 매 프레임 체력/마나/랜턴 비율에 맞춰 UI 업데이트
    void Update(float currentHp, float maxHp, float currentMp, float maxMp, float currentLantern, float maxLantern);
    void UpdateBossHealthBar(float currentHp, float maxHp);
    void HideBossHealthBar();
    void SetChatBoxState(bool active, bool hasMessages);

    // UI 전용 객체 리스트 반환 (렌더링할 때 사용)
    const std::vector<std::unique_ptr<GameObject>>& GetUIObjects() const { return mUIObjects; }

 

private:
    EclipseWalkerGame* mGame;
    std::vector<std::unique_ptr<GameObject>> mUIObjects;

    GameObject* mHpBarFill = nullptr;
    GameObject* mMpBarFill = nullptr;
    GameObject* mHpBarDelay = nullptr;
    GameObject* mMpBarDelay = nullptr;
    GameObject* mHpBarGloss = nullptr;
    GameObject* mMpBarGloss = nullptr;
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
    Material* mBossHpBackMat = nullptr;
    Material* mBossHpDelayMat = nullptr;
    Material* mBossHpFillMat = nullptr;
    Material* mBossHpGlossMat = nullptr;
    Material* mLanternRingMat = nullptr;
    Material* mLanternGlowMat = nullptr;
    Material* mLanternIconMat = nullptr;

    Material* mFlashMat = nullptr;       // 일렁이는 노이즈 재질
    Material* mBgMat = nullptr;          // 화면 전체 보라색 배경 재질
    GameObject* mFlashObj = nullptr;     // 화면을 덮는 네모 도화지 객체
    GameObject* mScreenBgObj = nullptr;  // 배경 가림막
    GameObject* mChatLogBg = nullptr;
    GameObject* mChatInputBg = nullptr;
    Material* mChatLogMat = nullptr;
    Material* mChatInputMat = nullptr;

    bool mIsFlashActive = false;         // 이펙트 켜짐 여부
    float mCurrentTime = 0.0f;           // 이펙트 진행 시간
    float mFlashDuration = 1.55f;        // 랜턴 점등 + 가림막 유지 시간
    float mHpDelayRatio = 1.0f;
    float mMpDelayRatio = 1.0f;
    float mBossHpDelayRatio = 1.0f;
    int mBossHpVisibleLayer = 0;
    float mLanternDelayRatio = 0.0f;
    float mLanternGlowTime = 0.0f;
};
