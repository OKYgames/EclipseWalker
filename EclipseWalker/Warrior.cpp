#include "Warrior.h"
#include <Windows.h>

Warrior::Warrior()
{
    // ==========================================
    // 1. 전사의 고유 스탯 설정 (근접 탱커/딜러)
    // ==========================================
    maxHp = 500.0f;
    hp = 500.0f;     // 피통이 아주 큼
    maxMp = 50.0f;
    mp = 50.0f;      // 마나는 적음

    mMoveSpeed = 3.4f;           // 좁은 맵 기준으로 묵직하지만 답답하지 않게 조정
    mDashDuration = 0.35f;       // 대쉬 유지 시간도 약간 길게 (육중한 구르기)
    mDashSpeedMultiplier = 2.5f; // 대신 구르는 속도는 살짝 느림

    // 태어날 때 1티어(수습 전사) 외형으로 초기화
    UpdateMeshForTier();
}

Warrior::~Warrior() {}

// ==========================================
// 2. 전사 스킬 구현
// ==========================================
void Warrior::Skill1()
{
    if (mp < 10.0f) return;
    mp -= 10.0f;

    OutputDebugStringA("[Warrior] 스킬 1: 방패 돌진 발동!\n");
}

void Warrior::Skill2()
{
    if (mp < 20.0f) return;
    mp -= 20.0f;
    OutputDebugStringA("[Warrior] 스킬 2: 대검 회전 베기 발동!\n");
}

// ==========================================
// 3. 전사 티어(승급)별 모델링 교체
// ==========================================
void Warrior::UpdateMeshForTier()
{
    // ※ 주의: 실제 렌더 아이템을 가져오는 코드는 사용자님의 
    // 리소스 매니저 구조(mGame->GetResources() 등)에 맞게 연결해 주셔야 합니다.

    if (mPlayerObject == nullptr) return;

    if (mCurrentTier == ClassTier::Tier1) {
        // 1티어: 낡은 가죽 갑옷과 녹슨 검
        OutputDebugStringA("[Warrior 외형 변경] 1티어: 수습 전사 장착\n");
        // mPlayerObject->Ritem = 1티어_렌더아이템;
    }
    else if (mCurrentTier == ClassTier::Tier2) {
        // 2티어: 단단한 강철 갑옷
        OutputDebugStringA("[Warrior 외형 변경] 2티어: 정규 기사 장착\n");
        // mPlayerObject->Ritem = 2티어_렌더아이템;
    }
    else if (mCurrentTier == ClassTier::Tier3) {
        // 3티어: 불타는 대검과 화려한 판금 갑옷
        OutputDebugStringA("[Warrior 외형 변경] 3티어: 소드마스터 장착!\n");
        // mPlayerObject->Ritem = 3티어_렌더아이템;
    }
}
