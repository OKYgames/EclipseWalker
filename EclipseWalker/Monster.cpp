#include "Monster.h"

Monster::Monster(MonsterType type) : m_type(type)
{
    m_hp = 100.0f;
    m_moveSpeed = 3.0f;
    m_detectRange = 50.0f; 
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
    DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&pos);
    DirectX::XMVECTOR vPlayerPos = DirectX::XMLoadFloat3(&playerPos);

    // 1. ?뚮젅?댁뼱 諛⑺뼢?쇰줈 X, Z ?대룞
    DirectX::XMVECTOR vDir = DirectX::XMVector3Normalize(vPlayerPos - vPos);
    vPos += vDir * m_moveSpeed * dt;

    // ?뚯쟾 怨꾩궛
    DirectX::XMFLOAT3 dir;
    DirectX::XMStoreFloat3(&dir, vDir);
    float angle = atan2f(dir.x, dir.z);
    SetRotation(0.0f, angle, 0.0f);

    DirectX::XMStoreFloat3(&pos, vPos);

    // 吏???믪씠 留??곕룞
    if (mapSystem != nullptr)
    {
        float groundY = mapSystem->GetFloorHeight(pos.x, pos.z, pos.y, 2.0f);

        if (groundY > -9000.0f)
        {
            pos.y = groundY + m_collider.Extents.y;
        }
    }

    // 理쒖쥌 ?꾩튂 ?곸슜
    SetPosition(pos.x, pos.y, pos.z);
}

void Monster::OnDamaged(float damage)
{
    m_hp -= damage;
    if (m_hp <= 0.0f) {
        m_hp = 0.0f;
        m_state = MonsterState::DIE;
        // ?꾩슂 ???ш린??Ritem 鍮꾪솢?깊솕 ??泥섎━
    }
}

void Monster::ApplyServerHit(int remainHp, bool isDead)
{
    m_hp = remainHp > 0 ? static_cast<float>(remainHp) : 0.0f;
    if (isDead || m_hp <= 0.0f)
    {
        m_state = MonsterState::DIE;
    }
}
