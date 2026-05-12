#include "Monster.h"

#include "SkeletalAnimationComponent.h"

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
        m_moveSpeed = 6.0f; // ?꾪봽??議곌툑 ??鍮좊Ⅴ寃?
        break;
    case MonsterType::REAL_SKELETON_SWORD:
        m_attackRange = 2.5f;
        break;
    case MonsterType::SPECTRAL_BRAWLER:
        m_hp = 150.0f;
        m_moveSpeed = 4.0f;
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
    if (m_type == MonsterType::REAL_IMP)
        m_collider.Extents = XMFLOAT3(0.3f, 0.5f, 0.3f); 
    else
        m_collider.Extents = XMFLOAT3(0.5f, 1.0f, 0.5f); 
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
    case MonsterState::IDLE:
    case MonsterState::TRACE:
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
