#pragma once
#include "GameObject.h"
#include "GameTimer.h"
#include "MapSystem.h"
#include "Player.h"


// 몬스터의 현재 상태
enum class MonsterState { IDLE, TRACE, ATTACK, DIE };

// 몬스터 종류 (현실/이면 세계 구분)
enum class MonsterType {
    REAL_IMP, REAL_SKELETON_ARCHER, REAL_SKELETON_SWORD, // 현실
    SPECTRAL_BRAWLER, SPECTRAL_ARCHER, SPECTRAL_IMP     // 이면
};

class Monster : public GameObject
{
public:
    Monster(MonsterType type);
    virtual ~Monster();

    void Initialize(RenderItem* ritem, DirectX::XMFLOAT3 startPos);
    void Update(const GameTimer& gt, Player* pPlayer, MapSystem* mapSystem);

    // 상태 제어
    void OnDamaged(float damage);
    MonsterState GetState() const { return m_state; }
    MonsterType GetType() const { return m_type; }

protected:
    // AI 로직
    void ProcessAI(DirectX::XMFLOAT3 playerPos);
    void ApplyMovement(float dt, DirectX::XMFLOAT3 playerPos, MapSystem* mapSystem);

protected:
    MonsterType m_type;
    MonsterState m_state = MonsterState::IDLE;

    // 능력치
    float m_hp = 100.0f;
    float m_moveSpeed = 3.0f;
    float m_detectRange = 15.0f; // 플레이어 감지 거리
    float m_attackRange = 2.0f;  // 공격 사거리
    float m_attackCooldown = 1.5f; 
    float m_attackTimer = 0.0f;

    DirectX::BoundingBox m_collider;
};
