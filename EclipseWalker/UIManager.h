#pragma once
#include "GameObject.h"
#include "Material.h"
#include <vector>
#include <memory>

class EclipseWalkerGame;

class UIManager
{
public:
    UIManager(EclipseWalkerGame* game);
    ~UIManager();

    // UI 시스템 초기화 및 생성
    void BuildInGameUI();

	// 플래시 이펙트 관련 함수들  
    void InitializeEffect(Material* flashMat, Material* bgMat, GameObject* flashObj);
    void TriggerFlashEffect();
    void UpdateEffect(float dt);

    // 매 프레임 체력/마나 비율에 맞춰 UI 업데이트
    void Update(float currentHp, float maxHp, float currentMp, float maxMp);

    // UI 전용 객체 리스트 반환 (렌더링할 때 사용)
    const std::vector<std::unique_ptr<GameObject>>& GetUIObjects() const { return mUIObjects; }

 

private:
    EclipseWalkerGame* mGame;
    std::vector<std::unique_ptr<GameObject>> mUIObjects;

    GameObject* mHpBarFill = nullptr;
    GameObject* mMpBarFill = nullptr;

    Material* mFlashMat = nullptr;       // 일렁이는 노이즈 재질
    Material* mBgMat = nullptr;          // 화면 전체 보라색 배경 재질
    GameObject* mFlashObj = nullptr;     // 화면을 덮는 네모 도화지 객체

    bool mIsFlashActive = false;         // 이펙트 켜짐 여부
    float mCurrentTime = 0.0f;           // 이펙트 진행 시간
    float mFlashDuration = 3.0f;         // 이펙트 유지 시간 (3초)
};