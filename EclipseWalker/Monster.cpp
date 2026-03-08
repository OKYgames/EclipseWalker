#include "Monster.h"

Monster::Monster(MonsterType type) : m_type(type)
{
    m_hp = 100.0f;
    m_moveSpeed = 3.0f;
    m_detectRange = 50.0f; 
    m_attackRange = 2.0f;
    m_state = MonsterState::IDLE;

    // 종류별 능력치 차별화
    switch (m_type) {
    case MonsterType::REAL_IMP:
        m_moveSpeed = 6.0f; // 임프는 조금 더 빠르게
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

void Monster::Update(const GameTimer& gt, DirectX::XMFLOAT3 playerPos, MapSystem* mapSystem)
{
    if (m_state == MonsterState::DIE) return;

    ProcessAI(playerPos);

    // 이동 함수에 맵 시스템 전달
    ApplyMovement(gt.DeltaTime(), playerPos, mapSystem);

    GameObject::Update();
}

void Monster::ProcessAI(DirectX::XMFLOAT3 playerPos)
{
    XMFLOAT3 pos = GetPosition();
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vPlayerPos = XMLoadFloat3(&playerPos);

    float dist = XMVectorGetX(XMVector3Length(vPlayerPos - vPos));

    // 상태 전환 로직
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

    // 1. 플레이어 방향으로 X, Z 이동
    DirectX::XMVECTOR vDir = DirectX::XMVector3Normalize(vPlayerPos - vPos);
    vPos += vDir * m_moveSpeed * dt;

    // 회전 계산
    DirectX::XMFLOAT3 dir;
    DirectX::XMStoreFloat3(&dir, vDir);
    float angle = atan2f(dir.x, dir.z);
    SetRotation(0.0f, angle, 0.0f);

    DirectX::XMStoreFloat3(&pos, vPos);

    // 지형 높이 맵 연동
    if (mapSystem != nullptr)
    {
        float groundY = mapSystem->GetFloorHeight(pos.x, pos.z, pos.y, 2.0f);

        if (groundY > -9000.0f)
        {
            pos.y = groundY + m_collider.Extents.y;
        }
    }

    // 최종 위치 적용
    SetPosition(pos.x, pos.y, pos.z);
}

void Monster::OnDamaged(float damage)
{
    m_hp -= damage;
    if (m_hp <= 0.0f) {
        m_hp = 0.0f;
        m_state = MonsterState::DIE;
        // 필요 시 여기서 Ritem 비활성화 등 처리
    }
}