#pragma once
#include "GameObject.h"
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

    // 매 프레임 체력/마나 비율에 맞춰 UI 업데이트
    void Update(float currentHp, float maxHp, float currentMp, float maxMp);

    // UI 전용 객체 리스트 반환 (렌더링할 때 사용)
    const std::vector<std::unique_ptr<GameObject>>& GetUIObjects() const { return mUIObjects; }

private:
    EclipseWalkerGame* mGame;

    std::vector<std::unique_ptr<GameObject>> mUIObjects;

    GameObject* mHpBarFill = nullptr;
    GameObject* mMpBarFill = nullptr;
};