#pragma once
#include "GameObject.h"
#include "GameTimer.h"
#include "MapSystem.h"
#include "Player.h"


// 紐ъ뒪?곗쓽 ?꾩옱 ?곹깭
enum class MonsterState { IDLE, TRACE, ATTACK, DIE };

// 紐ъ뒪??醫낅쪟 (?꾩떎/?대㈃ ?멸퀎 援щ텇)
enum class MonsterType {
    REAL_IMP, REAL_SKELETON_ARCHER, REAL_SKELETON_SWORD, // ?꾩떎
    SPECTRAL_BRAWLER, SPECTRAL_ARCHER, SPECTRAL_IMP     // ?대㈃
};

class Monster : public GameObject
{
public:
    Monster(MonsterType type);
    virtual ~Monster();

    void Initialize(RenderItem* ritem, DirectX::XMFLOAT3 startPos);
    void Update(const GameTimer& gt, Player* pPlayer, MapSystem* mapSystem);

    // ?곹깭 ?쒖뼱
    void OnDamaged(float damage);
    MonsterState GetState() const { return m_state; }
    MonsterType GetType() const { return m_type; }
    float GetHP() const { return m_hp; }
    float GetMaxHP() const { return m_maxHp; }
    float GetHealthRatio() const
    {
        if (m_maxHp <= 0.0f) return 0.0f;
        const float ratio = m_hp / m_maxHp;
        if (ratio < 0.0f) return 0.0f;
        if (ratio > 1.0f) return 1.0f;
        return ratio;
    }
    float GetColliderHalfHeight() const { return m_collider.Extents.y; }

protected:
    // AI 濡쒖쭅
    void ProcessAI(DirectX::XMFLOAT3 playerPos);
    void ApplyMovement(float dt, DirectX::XMFLOAT3 playerPos, MapSystem* mapSystem);

protected:
    MonsterType m_type;
    MonsterState m_state = MonsterState::IDLE;

    // ?λ젰移?
    float m_hp = 100.0f;
    float m_maxHp = 100.0f;
    float m_moveSpeed = 3.0f;
    float m_detectRange = 15.0f; // ?뚮젅?댁뼱 媛먯? 嫄곕━
    float m_attackRange = 2.0f;  // 怨듦꺽 ?ш굅由?
    float m_attackCooldown = 1.5f; 
    float m_attackTimer = 0.0f;

    DirectX::BoundingBox m_collider;
};
