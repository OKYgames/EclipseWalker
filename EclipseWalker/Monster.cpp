#include "Monster.h"

#include "AudioManager.h"
#include "NetworkManager.h"
#include "SkeletalAnimationComponent.h"

#include <algorithm>

namespace
{
    constexpr wchar_t kSkeletonAmbientSound[] = L"Sounds\\Skeleton\\Skeleton_Ambient.mp3";
    constexpr wchar_t kSkeletonAggroSound[] = L"Sounds\\Skeleton\\Skeleton_Aggro.mp3";
    constexpr wchar_t kSkeletonDamageSound[] = L"Sounds\\Skeleton\\Skeleton_Damage.mp3";
    constexpr wchar_t kSkeletonDeathSound[] = L"Sounds\\Skeleton\\Skeleton_Death.mp3";
    constexpr wchar_t kSkeletonArcherAttackSound[] = L"Sounds\\Skeleton\\SkeletonArcher_Attack.mp3";
    constexpr wchar_t kSkeletonKnightAttackSound[] = L"Sounds\\Skeleton\\SkeletonKnight_Attack.mp3";

    constexpr float kSkeletonAmbientVolume = 0.045f;
    constexpr float kSkeletonAggroVolume = 0.055f;
    constexpr float kSkeletonAttackVolume = 0.06f;
    constexpr float kSkeletonDamageVolume = 0.1f;
    constexpr float kSkeletonDeathVolume = 0.065f;
    constexpr float kPredictedHpHoldSeconds = 0.45f;
    constexpr float kSkeletonArcherReleaseFraction = 0.70f;
    constexpr float kImpArcherReleaseFraction = 0.56f;
    constexpr float kSkeletonArcherArrowHeight = 0.30f;
    constexpr float kImpArcherArrowHeight = 0.18f;
    constexpr float kSkeletonArcherArrowRightOffset = 0.10f;
    constexpr float kImpArcherArrowRightOffset = -0.05f;
    constexpr float kMonsterArrowExtraTravelDistance = 1.5f;
    constexpr float kMonsterArrowSpeed = 28.0f;
    constexpr float kDelayedDamageHitStopWaitSeconds = 0.45f;

    bool HasLineOfSightToTarget(const XMFLOAT3& from, const XMFLOAT3& to, MapSystem* mapSystem)
    {
        if (mapSystem == nullptr)
        {
            return true;
        }

        const XMVECTOR origin = XMVectorSet(from.x, from.y, from.z, 1.0f);
        const XMVECTOR target = XMVectorSet(to.x, to.y, to.z, 1.0f);
        const XMVECTOR toTarget = target - origin;
        const float distance = XMVectorGetX(XMVector3Length(toTarget));
        if (distance <= 0.05f)
        {
            return true;
        }

        float wallHitDistance = 0.0f;
        return !mapSystem->CastWallRay(
            origin,
            XMVectorScale(toTarget, 1.0f / distance),
            distance - 0.05f,
            wallHitDistance);
    }
}

Monster::Monster(MonsterType type) : m_type(type)
{
    m_hp = 100.0f;
    m_moveSpeed = 3.0f;
    m_detectRange = 16.0f; 
    m_attackRange = 2.0f;
    m_state = MonsterState::IDLE;

    switch (m_type) {
    case MonsterType::SPECTRAL_IMP:
        m_moveSpeed = 6.0f; // ?꾪봽??議곌툑 ??鍮좊Ⅴ寃?
        m_detectRange = 5.0f;
        m_attackRange = 1.6f;
        m_attackCooldown = 1.0f;
        break;
    case MonsterType::SPECTRAL_ARCHER:
        m_hp = 110.0f;
        m_moveSpeed = 3.4f;
        m_detectRange = 10.0f;
        m_attackRange = 10.0f;
        m_attackCooldown = 2.0f;
        break;
    case MonsterType::SPECTRAL_BRAWLER:
        m_hp = 150.0f;
        m_moveSpeed = 4.4f;
        m_detectRange = 5.0f;
        m_attackRange = 1.8f;
        m_attackCooldown = 2.0f;
        break;
    case MonsterType::REAL_SKELETON_SWORD:
        m_moveSpeed = 3.2f;
        m_detectRange = 5.0f;
        m_attackRange = 1.8f;
        m_attackCooldown = 2.0f;
        break;
    case MonsterType::REAL_SKELETON_ARCHER:
        m_moveSpeed = 2.6f;
        m_detectRange = 10.0f;
        m_attackRange = 10.5f;
        m_attackCooldown = 4.0f;
        break;
    case MonsterType::STAGE2_BOSS:
        m_hp = 1200.0f;
        m_moveSpeed = 2.0f;
        m_detectRange = 22.0f;
        m_attackRange = 4.0f;
        m_attackCooldown = 1.5f;
        break;
    }

    m_maxHp = m_hp;
}

Monster::~Monster() {}

void Monster::Initialize(RenderItem* ritem, DirectX::XMFLOAT3 startPos)
{
    this->Ritem = ritem; 
    SetPosition(startPos.x, startPos.y, startPos.z);

    GameObject::Update();

    m_collider.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    if (m_type == MonsterType::SPECTRAL_IMP)
        m_collider.Extents = XMFLOAT3(0.3f, 0.5f, 0.3f); 
    else if (m_type == MonsterType::SPECTRAL_ARCHER || m_type == MonsterType::SPECTRAL_BRAWLER)
        m_collider.Extents = XMFLOAT3(0.38f, 0.6f, 0.38f);
    else if (m_type == MonsterType::STAGE2_BOSS)
        m_collider.Extents = XMFLOAT3(1.25f, 2.1f, 1.25f);
    else
        m_collider.Extents = XMFLOAT3(0.5f, 1.0f, 0.5f); 

    m_hurtboxExtents = m_collider.Extents;
    switch (m_type)
    {
    case MonsterType::SPECTRAL_IMP:
        m_hurtboxExtents = XMFLOAT3(0.46f, 0.72f, 0.46f);
        break;

    case MonsterType::REAL_SKELETON_ARCHER:
    case MonsterType::SPECTRAL_ARCHER:
        m_hurtboxExtents = (m_type == MonsterType::SPECTRAL_ARCHER)
            ? XMFLOAT3(0.58f, 0.90f, 0.58f)
            : XMFLOAT3(0.82f, 1.28f, 0.82f);
        break;

    case MonsterType::REAL_SKELETON_SWORD:
        m_hurtboxExtents = XMFLOAT3(0.88f, 1.34f, 0.88f);
        break;

    case MonsterType::SPECTRAL_BRAWLER:
        m_hurtboxExtents = XMFLOAT3(0.64f, 0.96f, 0.64f);
        break;

    case MonsterType::STAGE2_BOSS:
        m_hurtboxExtents = XMFLOAT3(2.45f, 2.65f, 2.45f);
        break;

    default:
        break;
    }
}

void Monster::Update(const GameTimer& gt, Player* pPlayer, MapSystem* mapSystem)
{
    // ?뚮젅?댁뼱媛 二쎌뿀嫄곕굹 ?놁쑝硫?由ы꽩
    if (m_state == MonsterState::DIE || pPlayer == nullptr) return;

    if (UpdateAnimationState(gt.DeltaTime()))
    {
        GameObject::Update();
        return;
    }

    DirectX::XMFLOAT3 playerPos = pPlayer->GetPosition();

    // AI 諛??대룞 濡쒖쭅
    ProcessAI(playerPos, mapSystem);
    ApplyMovement(gt.DeltaTime(), playerPos, mapSystem);

    // 怨듦꺽 濡쒖쭅 
    if (m_state == MonsterState::ATTACK)
    {
        m_attackTimer -= gt.DeltaTime();
        if (m_attackTimer <= 0.0f)
        {
            PlayAttackSound();
            if (!NetworkManager::Get()->IsConnected())
            {
                // ?뚮젅?댁뼱?먭쾶 10 ?곕?吏 ?곸슜
                pPlayer->OnDamaged(10.0f);
            }

            // 荑⑦???由ъ뀑 
            m_attackTimer = m_attackCooldown;
        }
    }
    else
    {
        m_attackTimer = 0.0f;
    }

    GameObject::Update();
}

bool Monster::UpdateAnimationState(float dt)
{
    UpdateDelayedDamageHitStop(dt);
    UpdateActiveKnockback(dt);

    if (m_predictedHpTimer > 0.0f)
    {
        m_predictedHpTimer -= dt;
        if (m_predictedHpTimer <= 0.0f)
        {
            m_predictedHpTimer = 0.0f;
            m_predictedHp = -1.0f;
        }
    }

    if (m_state == MonsterState::DAMAGED)
    {
        m_damageStateTimer -= dt;
        if (m_damageStateTimer <= 0.0f)
        {
            m_damageStateTimer = 0.0f;
            if (m_serverAttackQueued)
            {
                m_serverAttackQueued = false;
                StartServerAttackAnimation();
            }
            else
            {
                m_state = MonsterState::IDLE;
                PlayIdleAnimation();
            }
        }

        return true;
    }

    if (m_state == MonsterState::DYING)
    {
        m_deathStateTimer -= dt;
        if (m_deathStateTimer <= 0.0f)
        {
            m_state = MonsterState::DIE;
            m_deathStateTimer = 0.0f;
        }

        return true;
    }

    if (m_serverAttackAnimationLocked)
    {
        m_serverAttackAnimationTimer -= dt;
        if (m_serverAttackAnimationTimer <= 0.0f)
        {
            FinishServerAttackAnimation();
        }

        return true;
    }

    return false;
}

void Monster::UpdateLocomotionAnimation(bool isMoving)
{
    if (!IsSkeletonType() ||
        m_serverAttackAnimationLocked ||
        m_state == MonsterState::DAMAGED ||
        m_state == MonsterState::DYING ||
        m_state == MonsterState::DIE)
    {
        return;
    }

    const MonsterState nextState = isMoving ? MonsterState::TRACE : MonsterState::IDLE;
    if (m_state == nextState)
    {
        return;
    }

    m_state = nextState;
    if (isMoving)
    {
        PlayWalkAnimation();
    }
    else
    {
        PlayIdleAnimation();
    }
}

void Monster::ProcessAI(DirectX::XMFLOAT3 playerPos, MapSystem* mapSystem)
{
    XMFLOAT3 pos = GetPosition();
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vPlayerPos = XMLoadFloat3(&playerPos);

    float dist = XMVectorGetX(XMVector3Length(vPlayerPos - vPos));
    const bool hasLineOfSight =
        (m_type == MonsterType::STAGE2_BOSS) ||
        HasLineOfSightToTarget(pos, playerPos, mapSystem);

    // ?곹깭 ?꾪솚 濡쒖쭅
    if (hasLineOfSight && dist <= m_attackRange) {
        m_state = MonsterState::ATTACK;
    }
    else if (hasLineOfSight && dist <= m_detectRange) {
        m_state = MonsterState::TRACE;
    }
    else {
        m_state = MonsterState::IDLE;
    }
}

void Monster::ApplyMovement(float dt, DirectX::XMFLOAT3 playerPos, MapSystem* mapSystem)
{
    if (m_state != MonsterState::TRACE) return;

    DirectX::XMFLOAT3 pos = GetPosition();
    DirectX::XMFLOAT3 dir =
    {
        playerPos.x - pos.x,
        0.0f,
        playerPos.z - pos.z
    };

    const float lenSq = dir.x * dir.x + dir.z * dir.z;
    if (lenSq <= 0.0001f)
    {
        return;
    }

    const float invLen = 1.0f / sqrtf(lenSq);
    dir.x *= invLen;
    dir.z *= invLen;

    float moveX = dir.x * m_moveSpeed * dt;
    float moveZ = dir.z * m_moveSpeed * dt;

    if (mapSystem != nullptr)
    {
        const float feetPos = pos.y - m_collider.Extents.y;
        if (mapSystem->CheckWall(pos.x, pos.z, feetPos, dir.x, 0.0f)) moveX = 0.0f;
        if (mapSystem->CheckWall(pos.x, pos.z, feetPos, 0.0f, dir.z)) moveZ = 0.0f;
    }

    if (moveX == 0.0f && moveZ == 0.0f)
    {
        return;
    }

    DirectX::XMFLOAT3 nextPos = pos;
    nextPos.x += moveX;
    nextPos.z += moveZ;

    if (mapSystem != nullptr)
    {
        const float halfHeight = m_collider.Extents.y;
        const float nextFeetPos = nextPos.y - halfHeight;
        const float rayStartY = nextFeetPos + 1.0f;
        const float floorY = mapSystem->GetFloorHeight(nextPos.x, nextPos.z, rayStartY, 1000.0f);

        if (floorY < -8000.0f)
        {
            return;
        }

        if (nextFeetPos < floorY || (nextFeetPos - floorY) <= 0.5f)
        {
            nextPos.y = floorY + halfHeight;
        }
        else
        {
            return;
        }
    }

    float angle = atan2f(dir.x, dir.z);
    SetRotation(0.0f, angle, 0.0f);
    SetPosition(nextPos.x, nextPos.y, nextPos.z);
}

void Monster::OnDamaged(float damage)
{
    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        return;
    }

    m_hp -= damage;
    if (m_hp <= 0.0f) {
        m_hp = 0.0f;
        EnterDeathState();
        return;
    }

    EnterDamageState();
}

void Monster::ApplyServerHit(int remainHp, bool isDead, bool playHitReaction)
{
    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        return;
    }

    m_hp = remainHp > 0 ? static_cast<float>(remainHp) : 0.0f;
    m_predictedHp = -1.0f;
    m_predictedHpTimer = 0.0f;

    if (isDead || m_hp <= 0.0f)
    {
        EnterDeathState();
        return;
    }

    if (!playHitReaction)
    {
        return;
    }

    EnterDamageState();
}

void Monster::ApplyPredictedDamage(float damage)
{
    if (damage <= 0.0f ||
        m_state == MonsterState::DIE ||
        m_state == MonsterState::DYING ||
        GetDisplayHP() <= 0.0f)
    {
        return;
    }

    m_predictedHp = (std::max)(1.0f, GetDisplayHP() - damage);
    m_predictedHpTimer = kPredictedHpHoldSeconds;
    EnterDamageState();
}

void Monster::ApplyServerState(int serverState, int remainHp, bool isDead, int attackSequence)
{
    const float serverHp = remainHp > 0 ? static_cast<float>(remainHp) : 0.0f;
    const bool shouldDie = isDead || serverState == 3 || remainHp <= 0;
    m_deferredServerState = serverState;
    const bool attackSequenceChanged = attackSequence != m_lastServerAttackSequence;
    m_lastServerAttackSequence = attackSequence;
    const bool shouldPlayAttackSound =
        attackSequenceChanged ||
        (serverState == 2 && m_state != MonsterState::ATTACK);

    if (shouldDie)
    {
        m_hp = 0.0f;
        m_predictedHp = -1.0f;
        m_predictedHpTimer = 0.0f;
        if (m_state != MonsterState::DIE && m_state != MonsterState::DYING)
        {
            EnterDeathState();
        }
        return;
    }

    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        m_hp = serverHp;
        m_predictedHp = -1.0f;
        m_predictedHpTimer = 0.0f;
        m_damageStateTimer = 0.0f;
        m_deathStateTimer = 0.0f;

        if (serverState == 1)
        {
            ForceAnimationState(MonsterState::TRACE);
        }
        else if (serverState == 2)
        {
            ForceAnimationState(MonsterState::ATTACK);
        }
        else
        {
            ForceAnimationState(MonsterState::IDLE);
        }
        return;
    }

    m_hp = serverHp;

    // Keep the hit reaction until it finishes; the next snapshot restores
    // the server's locomotion state.
    if (m_state == MonsterState::DAMAGED)
    {
        if (attackSequenceChanged)
        {
            m_serverAttackQueued = true;
        }
        return;
    }

    if (attackSequenceChanged)
    {
        if (shouldPlayAttackSound)
        {
            PlayAttackSound();
        }

        if (m_serverAttackAnimationLocked)
        {
            m_serverAttackQueued = true;
        }
        else
        {
            StartServerAttackAnimation();
        }
        return;
    }

    if (m_serverAttackAnimationLocked)
    {
        return;
    }

    if (serverState == 2 && m_state != MonsterState::ATTACK)
    {
        if (shouldPlayAttackSound)
        {
            PlayAttackSound();
        }
        StartServerAttackAnimation();
        return;
    }

    MonsterState nextState = MonsterState::IDLE;
    if (serverState == 1)
    {
        nextState = MonsterState::TRACE;
    }
    else if (serverState == 2)
    {
        nextState = MonsterState::ATTACK;
    }

    if (m_state == nextState)
    {
        return;
    }

    m_state = nextState;
    m_damageStateTimer = 0.0f;
    m_deathStateTimer = 0.0f;
    if (nextState == MonsterState::TRACE)
    {
        PlayWalkAnimation();
    }
    else if (nextState == MonsterState::ATTACK)
    {
        PlayIdleAnimation();
    }
    else
    {
        PlayIdleAnimation();
    }
}

float Monster::GetDisplayHP() const
{
    float displayHp = m_hp;
    if (m_predictedHpTimer > 0.0f && m_predictedHp >= 0.0f)
    {
        displayHp = (std::min)(displayHp, m_predictedHp);
    }

    if (displayHp < 0.0f)
    {
        return 0.0f;
    }

    if (displayHp > m_maxHp)
    {
        return m_maxHp;
    }

    return displayHp;
}

int Monster::GetExperienceReward() const
{
    switch (m_type)
    {
    case MonsterType::SPECTRAL_IMP:
        return 10;
    case MonsterType::REAL_SKELETON_ARCHER:
    case MonsterType::SPECTRAL_ARCHER:
        return 15;
    case MonsterType::REAL_SKELETON_SWORD:
    case MonsterType::SPECTRAL_BRAWLER:
        return 20;
    case MonsterType::STAGE2_BOSS:
        return 0;
    default:
        return 0;
    }
}

bool Monster::ConsumeArrowRequest(MonsterArrowRequest& request)
{
    if (!m_arrowRequestPending)
    {
        return false;
    }

    request = m_arrowRequest;
    m_arrowRequestPending = false;
    return true;
}

void Monster::RequestDelayedDamageHitStop(
    float delaySeconds,
    float durationSeconds,
    float timeScale,
    XMFLOAT3 knockbackDirection,
    float knockbackDistance,
    float knockbackDuration)
{
    if (durationSeconds <= 0.0f)
    {
        ClearDelayedDamageHitStop();
        return;
    }

    m_delayedDamageHitStopPending = true;
    m_delayedDamageHitStopDelay = (std::max)(0.0f, delaySeconds);
    m_delayedDamageHitStopDuration = (std::max)(0.0f, durationSeconds);
    m_delayedDamageHitStopTimeScale = (std::min)(1.0f, (std::max)(0.01f, timeScale));
    m_delayedDamageHitStopWaitTimer = kDelayedDamageHitStopWaitSeconds;
    m_delayedDamageKnockbackDirection = knockbackDirection;
    m_delayedDamageKnockbackDistance = (std::max)(0.0f, knockbackDistance);
    m_delayedDamageKnockbackDuration = (std::max)(0.0f, knockbackDuration);
}

void Monster::RequestDamageKnockback(
    XMFLOAT3 knockbackDirection,
    float knockbackDistance,
    float knockbackDuration)
{
    StartDamageKnockback(
        knockbackDirection,
        (std::max)(0.0f, knockbackDistance),
        (std::max)(0.0f, knockbackDuration));
}

void Monster::UpdateDelayedDamageHitStop(float dt)
{
    if (!m_delayedDamageHitStopPending)
    {
        return;
    }

    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        ClearDelayedDamageHitStop();
        return;
    }

    if (m_state != MonsterState::DAMAGED)
    {
        m_delayedDamageHitStopWaitTimer -= dt;
        if (m_delayedDamageHitStopWaitTimer <= 0.0f)
        {
            ClearDelayedDamageHitStop();
        }
        return;
    }

    m_delayedDamageHitStopDelay -= dt;
    if (m_delayedDamageHitStopDelay > 0.0f)
    {
        return;
    }

    if (auto* animation = GetSkeletalAnimation())
    {
        animation->RequestHitStop(
            m_delayedDamageHitStopDuration,
            m_delayedDamageHitStopTimeScale);
    }

    PlayDamageSound();

    StartDamageKnockback(
        m_delayedDamageKnockbackDirection,
        m_delayedDamageKnockbackDistance,
        m_delayedDamageKnockbackDuration);

    ClearDelayedDamageHitStop();
}

void Monster::ClearDelayedDamageHitStop()
{
    m_delayedDamageHitStopPending = false;
    m_delayedDamageHitStopDelay = 0.0f;
    m_delayedDamageHitStopDuration = 0.0f;
    m_delayedDamageHitStopTimeScale = 1.0f;
    m_delayedDamageHitStopWaitTimer = 0.0f;
    m_delayedDamageKnockbackDirection = { 0.0f, 0.0f, 0.0f };
    m_delayedDamageKnockbackDistance = 0.0f;
    m_delayedDamageKnockbackDuration = 0.0f;
}

void Monster::StartDamageKnockback(const XMFLOAT3& direction, float distance, float duration)
{
    if (distance <= 0.0f)
    {
        ClearKnockback();
        return;
    }

    XMVECTOR knockbackDirection = XMVectorSet(direction.x, 0.0f, direction.z, 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(knockbackDirection)) <= 0.0001f)
    {
        ClearKnockback();
        return;
    }

    knockbackDirection = XMVector3Normalize(knockbackDirection);
    XMStoreFloat3(&m_knockbackDirection, knockbackDirection);

    if (duration <= 0.0f)
    {
        const XMFLOAT3 position = GetPosition();
        SetPosition(
            position.x + m_knockbackDirection.x * distance,
            position.y,
            position.z + m_knockbackDirection.z * distance);
        ClearKnockback();
        return;
    }

    m_knockbackActive = true;
    m_knockbackRemainingDistance = distance;
    m_knockbackRemainingTime = duration;
}

void Monster::UpdateActiveKnockback(float dt)
{
    if (!m_knockbackActive)
    {
        return;
    }

    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        ClearKnockback();
        return;
    }

    if (m_knockbackRemainingDistance <= 0.0f || m_knockbackRemainingTime <= 0.0f)
    {
        ClearKnockback();
        return;
    }

    const float moveDistance = (std::min)(
        m_knockbackRemainingDistance,
        m_knockbackRemainingDistance * (dt / m_knockbackRemainingTime));
    const XMFLOAT3 position = GetPosition();
    SetPosition(
        position.x + m_knockbackDirection.x * moveDistance,
        position.y,
        position.z + m_knockbackDirection.z * moveDistance);

    m_knockbackRemainingDistance -= moveDistance;
    m_knockbackRemainingTime -= dt;
    if (m_knockbackRemainingDistance <= 0.0f || m_knockbackRemainingTime <= 0.0f)
    {
        ClearKnockback();
    }
}

void Monster::ClearKnockback()
{
    m_knockbackActive = false;
    m_knockbackDirection = { 0.0f, 0.0f, 0.0f };
    m_knockbackRemainingDistance = 0.0f;
    m_knockbackRemainingTime = 0.0f;
}

bool Monster::IsSkeletonType() const
{
    return m_type == MonsterType::REAL_SKELETON_SWORD ||
        m_type == MonsterType::REAL_SKELETON_ARCHER;
}

void Monster::PlayAmbientSound() const
{
    if (!IsSkeletonType())
    {
        return;
    }

    AudioManager::Get().PlayEffect(kSkeletonAmbientSound, kSkeletonAmbientVolume);
}

void Monster::PlayAggroSound() const
{
    if (!IsSkeletonType())
    {
        return;
    }

    AudioManager::Get().PlayEffect(kSkeletonAggroSound, kSkeletonAggroVolume);
}

void Monster::PlayAttackSound() const
{
    if (m_type == MonsterType::REAL_SKELETON_ARCHER)
    {
        AudioManager::Get().PlayEffect(kSkeletonArcherAttackSound, kSkeletonAttackVolume);
    }
    else if (m_type == MonsterType::REAL_SKELETON_SWORD)
    {
        AudioManager::Get().PlayEffect(kSkeletonKnightAttackSound, kSkeletonAttackVolume);
    }
}

void Monster::PlayDamageSound() const
{
    if (!IsSkeletonType())
    {
        return;
    }

    AudioManager::Get().PlayEffect(kSkeletonDamageSound, kSkeletonDamageVolume);
}

void Monster::PlayDeathSound() const
{
    if (!IsSkeletonType())
    {
        return;
    }

    AudioManager::Get().PlayEffect(kSkeletonDeathSound, kSkeletonDeathVolume);
}

void Monster::ForceAnimationState(MonsterState state)
{
    switch (state)
    {
    case MonsterState::DAMAGED:
        if (m_hp <= 0.0f)
        {
            m_hp = m_maxHp;
        }
        m_deathStateTimer = 0.0f;
        EnterDamageState();
        break;
    case MonsterState::DYING:
    case MonsterState::DIE:
        m_hp = 0.0f;
        EnterDeathState();
        break;
    case MonsterState::TRACE:
        if (m_hp <= 0.0f)
        {
            m_hp = m_maxHp;
        }
        m_state = MonsterState::TRACE;
        m_damageStateTimer = 0.0f;
        m_deathStateTimer = 0.0f;
        PlayWalkAnimation();
        break;
    case MonsterState::ATTACK:
        if (m_hp <= 0.0f)
        {
            m_hp = m_maxHp;
        }
        m_state = MonsterState::ATTACK;
        m_damageStateTimer = 0.0f;
        m_deathStateTimer = 0.0f;
        PlayAttackAnimation();
        break;
    case MonsterState::IDLE:
    default:
        if (m_hp <= 0.0f)
        {
            m_hp = m_maxHp;
        }
        m_state = MonsterState::IDLE;
        m_damageStateTimer = 0.0f;
        m_deathStateTimer = 0.0f;
        PlayIdleAnimation();
        break;
    }
}

void Monster::PlayIdleAnimation(float blendDuration)
{
    if (auto* animation = GetSkeletalAnimation())
    {
        animation->Play("SkeletonIdle", blendDuration);
    }
}

void Monster::PlayWalkAnimation(float blendDuration)
{
    if (auto* animation = GetSkeletalAnimation())
    {
        animation->Play("SkeletonWalk", blendDuration);
    }
}

void Monster::PlayAttackAnimation()
{
    if (auto* animation = GetSkeletalAnimation())
    {
        if (!animation->Play("SkeletonAttack", 0.05f, 1.0f, false))
        {
            animation->Play("SkeletonIdle", 0.05f);
        }
    }
}

void Monster::StartServerAttackAnimation()
{
    m_state = MonsterState::ATTACK;
    m_damageStateTimer = 0.0f;
    m_deathStateTimer = 0.0f;
    m_serverAttackAnimationLocked = true;
    m_serverAttackQueued = false;

    float attackDuration = 1.0f;
    if (auto* animation = GetSkeletalAnimation())
    {
        const float clipDuration = animation->GetClipDurationSeconds("SkeletonAttack");
        if (clipDuration > 0.05f)
        {
            attackDuration = clipDuration;
        }
    }

    m_serverAttackAnimationTimer = attackDuration;

    if (m_type == MonsterType::REAL_SKELETON_ARCHER ||
        m_type == MonsterType::SPECTRAL_ARCHER)
    {
        const bool isImpArcher = m_type == MonsterType::SPECTRAL_ARCHER;
        m_arrowRequest.TravelDistance = m_attackRange + kMonsterArrowExtraTravelDistance;
        m_arrowRequest.StartDelay = attackDuration *
            (isImpArcher ? kImpArcherReleaseFraction : kSkeletonArcherReleaseFraction);
        m_arrowRequest.StartHeight =
            isImpArcher ? kImpArcherArrowHeight : kSkeletonArcherArrowHeight;
        m_arrowRequest.StartRightOffset = isImpArcher
            ? kImpArcherArrowRightOffset
            : kSkeletonArcherArrowRightOffset;
        m_arrowRequest.Speed = kMonsterArrowSpeed;
        m_arrowRequestPending = true;
    }

    PlayAttackAnimation();
}

void Monster::FinishServerAttackAnimation()
{
    m_serverAttackAnimationLocked = false;
    m_serverAttackAnimationTimer = 0.0f;

    if (m_serverAttackQueued)
    {
        m_serverAttackQueued = false;
        StartServerAttackAnimation();
        return;
    }

    if (m_deferredServerState == 1)
    {
        m_state = MonsterState::TRACE;
        PlayWalkAnimation();
    }
    else
    {
        m_state = m_deferredServerState == 2 ? MonsterState::ATTACK : MonsterState::IDLE;
        PlayIdleAnimation();
    }
}

void Monster::PlayDamageAnimation()
{
    if (auto* animation = GetSkeletalAnimation())
    {
        animation->Play("SkeletonDamage", 0.05f);
    }
}

void Monster::PlayDeathAnimation()
{
    if (auto* animation = GetSkeletalAnimation())
    {
        animation->Play("SkeletonDeath", 0.05f);
    }
}

void Monster::EnterDamageState()
{
    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        return;
    }

    m_state = MonsterState::DAMAGED;
    m_serverAttackAnimationLocked = false;
    m_serverAttackAnimationTimer = 0.0f;
    m_serverAttackQueued = false;
    m_damageStateTimer = 0.65f;
    m_deathStateTimer = 0.0f;
    m_attackTimer = 0.0f;
    PlayDamageAnimation();
}

void Monster::EnterDeathState()
{
    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        return;
    }

    m_state = MonsterState::DYING;
    m_serverAttackAnimationLocked = false;
    m_serverAttackAnimationTimer = 0.0f;
    m_serverAttackQueued = false;
    ClearDelayedDamageHitStop();
    ClearKnockback();
    m_arrowRequestPending = false;
    m_hp = 0.0f;
    m_predictedHp = -1.0f;
    m_predictedHpTimer = 0.0f;
    m_damageStateTimer = 0.0f;
    m_attackTimer = 0.0f;
    PlayDeathSound();
    PlayDeathAnimation();

    float deathDuration = 1.2f;
    if (auto* animation = GetSkeletalAnimation())
    {
        const float clipDuration = animation->GetClipDurationSeconds("SkeletonDeath");
        if (clipDuration > 0.05f)
        {
            deathDuration = clipDuration;
        }
    }
    m_deathStateTimer = deathDuration;
}
