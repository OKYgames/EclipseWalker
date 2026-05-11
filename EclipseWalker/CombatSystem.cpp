#include "CombatSystem.h"

#include "Camera.h"
#include "EclipseWalkerGame.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "Scene.h"
#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace DirectX;

namespace
{
    XMFLOAT3 Normalize2D(const XMVECTOR& vectorValue)
    {
        XMVECTOR flat = XMVectorSetY(vectorValue, 0.0f);
        const float lengthSq = XMVectorGetX(XMVector3LengthSq(flat));
        if (lengthSq <= 0.0001f)
        {
            return { 0.0f, 0.0f, 1.0f };
        }

        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3Normalize(flat));
        return result;
    }
}

CombatSystem::CombatSystem(EclipseWalkerGame* game)
    : mGame(game)
{
}

void CombatSystem::Reset()
{
    mLeftMousePressed = false;
    mQKeyPressed = false;
    mEKeyPressed = false;
    mBasicCooldown = 0.0f;
    mSkill1Cooldown = 0.0f;
    mSkill2Cooldown = 0.0f;
}

void CombatSystem::Update(const GameTimer& gt, Player* player, const std::vector<Monster*>& monsters)
{
    if (player == nullptr)
    {
        return;
    }

    UpdateCooldowns(gt.DeltaTime());

    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    if (gIsChatInputActive || !hasFocus)
    {
        mLeftMousePressed = false;
        mQKeyPressed = false;
        mEKeyPressed = false;
        return;
    }

    const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (leftDown && !mLeftMousePressed)
    {
        TryBasicAttack(player, monsters);
    }
    mLeftMousePressed = leftDown;

    const bool qDown = (GetAsyncKeyState('Q') & 0x8000) != 0;
    if (qDown && !mQKeyPressed)
    {
        TrySkillAttack(player, monsters, 1);
    }
    mQKeyPressed = qDown;

    const bool eDown = (GetAsyncKeyState('E') & 0x8000) != 0;
    if (eDown && !mEKeyPressed)
    {
        TrySkillAttack(player, monsters, 2);
    }
    mEKeyPressed = eDown;
}

void CombatSystem::UpdateCooldowns(float dt)
{
    mBasicCooldown -= dt;
    mSkill1Cooldown -= dt;
    mSkill2Cooldown -= dt;

    if (mBasicCooldown < 0.0f) mBasicCooldown = 0.0f;
    if (mSkill1Cooldown < 0.0f) mSkill1Cooldown = 0.0f;
    if (mSkill2Cooldown < 0.0f) mSkill2Cooldown = 0.0f;
}

void CombatSystem::TryBasicAttack(Player* player, const std::vector<Monster*>& monsters)
{
    if (mBasicCooldown > 0.0f)
    {
        return;
    }

    player->FaceCameraForward();
    if (!player->PlayRandomBasicAttack())
    {
        return;
    }

    const AttackProfile profile = GetProfile(player->GetClassType(), 0);
    ApplyAttack(player, monsters, profile);
    SendServerAttack(player, 0);

    mBasicCooldown = 0.28f;
}

void CombatSystem::TrySkillAttack(Player* player, const std::vector<Monster*>& monsters, int skillIndex)
{
    float& cooldown = (skillIndex == 1) ? mSkill1Cooldown : mSkill2Cooldown;
    if (cooldown > 0.0f)
    {
        return;
    }

    player->FaceCameraForward();
    if (!player->PlaySkillAttack(skillIndex))
    {
        return;
    }

    if (skillIndex == 1)
    {
        player->Skill1();
    }
    else
    {
        player->Skill2();
    }

    const AttackProfile profile = GetProfile(player->GetClassType(), skillIndex);
    ApplyAttack(player, monsters, profile);
    SendServerAttack(player, skillIndex);

    cooldown = (skillIndex == 1) ? 1.0f : 1.6f;
}

CombatSystem::AttackProfile CombatSystem::GetProfile(PlayerClass playerClass, int attackKind) const
{
    switch (playerClass)
    {
    case PlayerClass::Warrior:
        if (attackKind == 2) return { 4.2f, 2.4f, 45.0f, 0.10f, true };
        if (attackKind == 1) return { 3.8f, 1.8f, 32.0f, 0.35f, true };
        return { 2.8f, 1.4f, 18.0f, 0.45f, true };

    case PlayerClass::Mage:
        if (attackKind == 2) return { 14.0f, 1.8f, 42.0f, 0.35f, true };
        if (attackKind == 1) return { 12.0f, 1.3f, 28.0f, 0.45f, true };
        return { 10.0f, 1.0f, 16.0f, 0.55f, false };

    case PlayerClass::Archer:
        if (attackKind == 2) return { 18.0f, 1.2f, 38.0f, 0.50f, false };
        if (attackKind == 1) return { 15.0f, 1.0f, 26.0f, 0.60f, false };
        return { 12.0f, 0.7f, 17.0f, 0.70f, false };

    case PlayerClass::None:
    default:
        return { 2.5f, 1.0f, 10.0f, 0.40f, false };
    }
}

void CombatSystem::SendServerAttack(Player* player, int skillType) const
{
    const XMFLOAT3 playerPos = player->GetPosition();
    const float rotY = player->GetFacingRotY();
    NetworkManager::Get()->SendPlayerAttack(skillType, playerPos.x, playerPos.y, playerPos.z, rotY);
}

int CombatSystem::ApplyAttack(Player* player, const std::vector<Monster*>& monsters, const AttackProfile& profile)
{
    const XMFLOAT3 playerPos = player->GetPosition();
    const XMFLOAT3 forward = Normalize2D(mGame->GetCamera()->GetLook());

    Monster* closestMonster = nullptr;
    float closestDistanceSq = FLT_MAX;
    int hitCount = 0;

    for (Monster* monster : monsters)
    {
        if (monster == nullptr || monster->GetState() == MonsterState::DIE)
        {
            continue;
        }

        const XMFLOAT3 monsterPos = monster->GetPosition();
        const float dx = monsterPos.x - playerPos.x;
        const float dz = monsterPos.z - playerPos.z;
        const float distanceSq = (dx * dx) + (dz * dz);
        if (distanceSq > (profile.range * profile.range))
        {
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        const float dirX = (distance > 0.001f) ? (dx / distance) : 0.0f;
        const float dirZ = (distance > 0.001f) ? (dz / distance) : 1.0f;
        const float dot = (dirX * forward.x) + (dirZ * forward.z);
        if (dot < profile.coneDot)
        {
            continue;
        }

        const float projected = (dx * forward.x) + (dz * forward.z);
        const float sideX = dx - (forward.x * projected);
        const float sideZ = dz - (forward.z * projected);
        const float sideDistanceSq = (sideX * sideX) + (sideZ * sideZ);
        if (sideDistanceSq > (profile.radius * profile.radius))
        {
            continue;
        }

        if (profile.hitAll)
        {
            ++hitCount;
        }
        else if (distanceSq < closestDistanceSq)
        {
            closestDistanceSq = distanceSq;
            closestMonster = monster;
        }
    }

    if (!profile.hitAll && closestMonster != nullptr)
    {
        hitCount = 1;
    }

    if (hitCount > 0)
    {
        OutputDebugStringA("[CombatSystem] 몬스터 타격 성공\n");
    }

    return hitCount;
}
