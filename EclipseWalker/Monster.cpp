#include "Monster.h"

#include "AudioManager.h"
#include "SkeletalAnimationComponent.h"

namespace
{
    constexpr wchar_t kSkeletonAmbientSound[] = L"Sounds\\Skeleton\\Skeleton_Ambient.mp3";
    constexpr wchar_t kSkeletonAggroSound[] = L"Sounds\\Skeleton\\Skeleton_Aggro.mp3";
    constexpr wchar_t kSkeletonDeathSound[] = L"Sounds\\Skeleton\\Skeleton_Death.mp3";
    constexpr wchar_t kSkeletonArcherAttackSound[] = L"Sounds\\Skeleton\\SkeletonArcher_Attack.mp3";
    constexpr wchar_t kSkeletonKnightAttackSound[] = L"Sounds\\Skeleton\\SkeletonKnight_Attack.mp3";

    constexpr float kSkeletonAmbientVolume = 0.18f;
    constexpr float kSkeletonAggroVolume = 0.22f;
    constexpr float kSkeletonAttackVolume = 0.24f;
    constexpr float kSkeletonDeathVolume = 0.26f;
}

Monster::Monster(MonsterType type) : m_type(type)
{
    m_hp = 100.0f;
    m_moveSpeed = 3.0f;
    m_detectRange = 16.0f; 
    m_attackRange = 2.0f;
    m_state = MonsterState::IDLE;

    // 醫낅쪟蹂??λ젰移?李⑤퀎??
    switch (m_type) {
    case MonsterType::REAL_IMP:
    case MonsterType::SPECTRAL_IMP:
        m_moveSpeed = 6.0f; // ?꾪봽??議곌툑 ??鍮좊Ⅴ寃?
        break;
    case MonsterType::SPECTRAL_ARCHER:
        m_hp = 110.0f;
        m_moveSpeed = 3.4f;
        m_detectRange = 18.0f;
        m_attackRange = 8.0f;
        m_attackCooldown = 1.8f;
        break;
    case MonsterType::SPECTRAL_BRAWLER:
        m_hp = 150.0f;
        m_moveSpeed = 4.4f;
        m_detectRange = 17.0f;
        m_attackRange = 2.3f;
        m_attackCooldown = 1.35f;
        break;
    case MonsterType::REAL_SKELETON_SWORD:
        m_moveSpeed = 3.2f;
        m_detectRange = 17.5f;
        m_attackRange = 2.5f;
        m_attackCooldown = 1.45f;
        break;
    case MonsterType::REAL_SKELETON_ARCHER:
        m_moveSpeed = 2.6f;
        m_detectRange = 18.5f;
        m_attackRange = 8.5f;
        m_attackCooldown = 1.9f;
        break;
    case MonsterType::STAGE2_BOSS:
        m_hp = 1200.0f;
        m_moveSpeed = 2.0f;
        m_detectRange = 22.0f;
        m_attackRange = 4.0f;
        m_attackCooldown = 2.4f;
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
    if (m_type == MonsterType::REAL_IMP || m_type == MonsterType::SPECTRAL_IMP)
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
    case MonsterType::REAL_IMP:
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
    ProcessAI(playerPos);
    ApplyMovement(gt.DeltaTime(), playerPos, mapSystem);

    // 怨듦꺽 濡쒖쭅 
    if (m_state == MonsterState::ATTACK)
    {
        m_attackTimer -= gt.DeltaTime();
        if (m_attackTimer <= 0.0f)
        {
            // ?뚮젅?댁뼱?먭쾶 10 ?곕?吏 ?곸슜
            pPlayer->OnDamaged(10.0f);

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
    if (m_state == MonsterState::DAMAGED)
    {
        m_damageStateTimer -= dt;
        if (m_damageStateTimer <= 0.0f)
        {
            m_state = MonsterState::IDLE;
            m_damageStateTimer = 0.0f;
            PlayIdleAnimation();
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

    return false;
}

void Monster::UpdateLocomotionAnimation(bool isMoving)
{
    if (!IsSkeletonType() ||
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

void Monster::ProcessAI(DirectX::XMFLOAT3 playerPos)
{
    XMFLOAT3 pos = GetPosition();
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vPlayerPos = XMLoadFloat3(&playerPos);

    float dist = XMVectorGetX(XMVector3Length(vPlayerPos - vPos));

    // ?곹깭 ?꾪솚 濡쒖쭅
    if (dist <= m_attackRange) {
        m_state = MonsterState::ATTACK;
    }
    else if (dist <= m_detectRange) {
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

void Monster::ApplyServerHit(int remainHp, bool isDead)
{
    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        return;
    }

    m_hp = remainHp > 0 ? static_cast<float>(remainHp) : 0.0f;
    if (isDead || m_hp <= 0.0f)
    {
        EnterDeathState();
        return;
    }

    EnterDamageState();
}

void Monster::ApplyPredictedDamage(float damage)
{
    if (damage <= 0.0f ||
        m_state == MonsterState::DIE ||
        m_state == MonsterState::DYING ||
        m_hp <= 0.0f)
    {
        return;
    }

    m_hp = (std::max)(1.0f, m_hp - damage);
    EnterDamageState();
}

void Monster::ApplyServerState(int serverState, int remainHp, bool isDead)
{
    const bool shouldDie = isDead || serverState == 3 || remainHp <= 0;
    m_hp = remainHp > 0 ? static_cast<float>(remainHp) : 0.0f;

    if (shouldDie)
    {
        if (m_state != MonsterState::DIE && m_state != MonsterState::DYING)
        {
            EnterDeathState();
        }
        return;
    }

    // Normal monsters do not respawn in the current stage, so a dead actor
    // must not be revived by an outdated visual snapshot.
    if (m_state == MonsterState::DIE || m_state == MonsterState::DYING)
    {
        return;
    }

    // Keep the hit reaction until it finishes; the next snapshot restores
    // the server's locomotion state.
    if (m_state == MonsterState::DAMAGED)
    {
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
    else
    {
        PlayIdleAnimation();
    }
}

int Monster::GetExperienceReward() const
{
    switch (m_type)
    {
    case MonsterType::REAL_IMP:
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
    case MonsterState::IDLE:
    case MonsterState::ATTACK:
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
    m_hp = 0.0f;
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
