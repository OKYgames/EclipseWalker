#include "Monster.h"

Monster::Monster(MonsterType type) : m_type(type)
{
    // 종류별 능력치 차별화
    switch (m_type) {
    case MonsterType::REAL_IMP: m_moveSpeed = 5.0f; break;
    case MonsterType::REAL_SKELETON_SWORD: m_attackRange = 2.5f; break;
    case MonsterType::SPECTRAL_BRAWLER: m_hp = 150.0f; m_moveSpeed = 4.0f; break;
    }
}

Monster::~Monster() {}

void Monster::Update(const GameTimer& gt, DirectX::XMFLOAT3 playerPos)
{
    if (m_state == MonsterState::DIE) return;

    ProcessAI(playerPos);
    ApplyMovement(gt.DeltaTime(), playerPos);

    // 부모 클래스의 World 행렬 업데이트 호출
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

void Monster::ApplyMovement(float dt, DirectX::XMFLOAT3 playerPos)
{
    if (m_state != MonsterState::TRACE) return;

    XMFLOAT3 pos = GetPosition();
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vPlayerPos = XMLoadFloat3(&playerPos);

    // 플레이어 방향으로 이동
    XMVECTOR vDir = XMVector3Normalize(vPlayerPos - vPos);
    vPos += vDir * m_moveSpeed * dt;

    // 회전 설정 (플레이어를 바라보게 함)
    XMFLOAT3 dir;
    XMStoreFloat3(&dir, vDir);
    float angle = atan2f(dir.x, dir.z);
    SetRotation(0.0f, angle, 0.0f);

    XMStoreFloat3(&pos, vPos);
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