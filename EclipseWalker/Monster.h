#pragma once
#include "GameObject.h"
#include "GameTimer.h"
#include "MapSystem.h"
#include "Player.h"


// 紐ъ뒪?곗쓽 ?꾩옱 ?곹깭
enum class MonsterState { IDLE, TRACE, ATTACK, DAMAGED, DYING, DIE };

// 紐ъ뒪??醫낅쪟 (?꾩떎/?대㈃ ?멸퀎 援щ텇)
enum class MonsterType {
    REAL_IMP, REAL_SKELETON_ARCHER, REAL_SKELETON_SWORD, // ?꾩떎
    SPECTRAL_BRAWLER, SPECTRAL_ARCHER, SPECTRAL_IMP,     // ?대㈃
    STAGE2_BOSS
};

struct MonsterArrowRequest
{
    float TravelDistance = 0.0f;
    float StartDelay = 0.0f;
    float StartHeight = 0.0f;
    float StartRightOffset = 0.1f;
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
    void ApplyPredictedDamage(float damage);
    void ApplyServerHit(int remainHp, bool isDead, bool playHitReaction = true);
    void RequestDelayedDamageHitStop(
        float delaySeconds,
        float durationSeconds,
        float timeScale,
        DirectX::XMFLOAT3 knockbackDirection,
        float knockbackDistance,
        float knockbackDuration);
    void ApplyServerState(int serverState, int remainHp, bool isDead, int attackSequence);
    void ForceAnimationState(MonsterState state);
    bool UpdateAnimationState(float dt);
    void UpdateLocomotionAnimation(bool isMoving);
    bool IsSkeletonType() const;
    void PlayAmbientSound() const;
    void PlayAggroSound() const;
    void PlayAttackSound() const;
    void PlayDamageSound() const;
    void PlayDeathSound() const;
    MonsterState GetState() const { return m_state; }
    MonsterType GetType() const { return m_type; }
    void SetNetworkId(int networkId) { m_networkId = networkId; }
    int GetNetworkId() const { return m_networkId; }
    float GetDisplayHP() const;
    float GetHP() const { return GetDisplayHP(); }
    float GetMaxHP() const { return m_maxHp; }
    int GetExperienceReward() const;
    bool ConsumeArrowRequest(MonsterArrowRequest& request);
    float GetHealthRatio() const
    {
        if (m_maxHp <= 0.0f) return 0.0f;
        const float ratio = GetDisplayHP() / m_maxHp;
        if (ratio < 0.0f) return 0.0f;
        if (ratio > 1.0f) return 1.0f;
        return ratio;
    }
    float GetColliderHalfHeight() const { return m_collider.Extents.y; }
    DirectX::XMFLOAT3 GetColliderExtents() const { return m_collider.Extents; }
    float GetHurtboxHalfHeight() const { return m_hurtboxExtents.y; }
    DirectX::XMFLOAT3 GetHurtboxExtents() const { return m_hurtboxExtents; }
    float GetGroundOffset() const { return m_collider.Extents.y; }

protected:
    // AI 濡쒖쭅
    void ProcessAI(DirectX::XMFLOAT3 playerPos, MapSystem* mapSystem);
    void ApplyMovement(float dt, DirectX::XMFLOAT3 playerPos, MapSystem* mapSystem);
    void PlayIdleAnimation(float blendDuration = 0.12f);
    void PlayWalkAnimation(float blendDuration = 0.12f);
    void PlayAttackAnimation();
    void StartServerAttackAnimation();
    void FinishServerAttackAnimation();
    void PlayDamageAnimation();
    void PlayDeathAnimation();
    void EnterDamageState();
    void EnterDeathState();
    void UpdateDelayedDamageHitStop(float dt);
    void ClearDelayedDamageHitStop();
    void StartDamageKnockback(const DirectX::XMFLOAT3& direction, float distance, float duration);
    void UpdateActiveKnockback(float dt);
    void ClearKnockback();

protected:
    MonsterType m_type;
    MonsterState m_state = MonsterState::IDLE;
    int m_networkId = -1;

    // ?λ젰移?
    float m_hp = 100.0f;
    float m_maxHp = 100.0f;
    float m_moveSpeed = 3.0f;
    float m_detectRange = 15.0f; // ?뚮젅?댁뼱 媛먯? 嫄곕━
    float m_attackRange = 2.0f;  // 怨듦꺽 ?ш굅由?
    float m_attackCooldown = 1.5f; 
    float m_attackTimer = 0.0f;
    float m_damageStateTimer = 0.0f;
    float m_deathStateTimer = 0.0f;
    float m_predictedHp = -1.0f;
    float m_predictedHpTimer = 0.0f;
    int m_lastServerAttackSequence = 0;
    int m_deferredServerState = 0;
    float m_serverAttackAnimationTimer = 0.0f;
    bool m_serverAttackAnimationLocked = false;
    bool m_serverAttackQueued = false;
    bool m_delayedDamageHitStopPending = false;
    float m_delayedDamageHitStopDelay = 0.0f;
    float m_delayedDamageHitStopDuration = 0.0f;
    float m_delayedDamageHitStopTimeScale = 1.0f;
    float m_delayedDamageHitStopWaitTimer = 0.0f;
    DirectX::XMFLOAT3 m_delayedDamageKnockbackDirection = { 0.0f, 0.0f, 0.0f };
    float m_delayedDamageKnockbackDistance = 0.0f;
    float m_delayedDamageKnockbackDuration = 0.0f;
    bool m_knockbackActive = false;
    DirectX::XMFLOAT3 m_knockbackDirection = { 0.0f, 0.0f, 0.0f };
    float m_knockbackRemainingDistance = 0.0f;
    float m_knockbackRemainingTime = 0.0f;
    bool m_arrowRequestPending = false;
    MonsterArrowRequest m_arrowRequest;

    DirectX::BoundingBox m_collider;
    DirectX::XMFLOAT3 m_hurtboxExtents = { 0.5f, 1.0f, 0.5f };
};
