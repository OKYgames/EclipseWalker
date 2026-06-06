#include "CombatSystem.h"

#include "Camera.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include "Scene.h"
#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <utility>

using namespace DirectX;

namespace
{
    constexpr int kDebugHitboxSegmentCount = 64;
    constexpr float kDebugHitboxBottomOffset = 0.035f;
    constexpr float kDebugHitboxBasicHalfHeight = 0.42f;
    constexpr float kDebugHitboxSkillHalfHeight = 0.55f;
    // 공격 판정 지연
    constexpr float kBasicAttack1HitDelay = 0.49f;
    constexpr float kBasicAttack2HitDelay = 0.57f;
    constexpr float kSkill1HitDelay = 0.38f;
    constexpr float kSkill2HitDelay = 0.42f;

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
    mDebugHitboxTimer = 0.0f;
    mDebugHitboxEnabled = false;
    mDebugHitboxTogglePressed = false;
    mPendingAttacks.clear();
    HideDebugHitbox();
}

void CombatSystem::Update(const GameTimer& gt, Player* player, const std::vector<Monster*>& monsters)
{
    UpdateDebugHitbox(gt.DeltaTime());

    if (player == nullptr)
    {
        return;
    }

    if (player->IsDead())
    {
        mPendingAttacks.clear();
        mLeftMousePressed = false;
        mQKeyPressed = false;
        mEKeyPressed = false;
        return;
    }

    UpdateCooldowns(gt.DeltaTime());
    UpdatePendingAttacks(gt.DeltaTime(), monsters);

    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    if (gIsChatInputActive || !hasFocus)
    {
        mLeftMousePressed = false;
        mQKeyPressed = false;
        mEKeyPressed = false;
        mDebugHitboxTogglePressed = false;
        return;
    }

    HandleDebugHitboxToggle();

    const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (gIsLanternUiInputActive)
    {
        mLeftMousePressed = leftDown;
    }
    else if (leftDown && !mLeftMousePressed)
    {
        TryBasicAttack(player, monsters);
        mLeftMousePressed = leftDown;
    }
    else
    {
        mLeftMousePressed = leftDown;
    }

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

void CombatSystem::SetDamageTextCallback(std::function<void(const XMFLOAT3&, float)> callback)
{
    mDamageTextCallback = std::move(callback);
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
    QueueAttack(player, 0, 0, profile);

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
    QueueAttack(player, skillIndex, skillIndex, profile);

    cooldown = (skillIndex == 1) ? 1.0f : 1.6f;
}

CombatSystem::AttackProfile CombatSystem::GetProfile(PlayerClass playerClass, int attackKind) const
{
    constexpr float kAttackForwardScale = 0.20f;
    constexpr float kAttackSideScale = 0.5f;

    auto ScaleRange = [=](AttackProfile profile)
    {
        profile.range *= kAttackForwardScale;
        profile.radius *= kAttackSideScale;
        profile.hitAll = true;
        return profile;
    };

    switch (playerClass)
    {
    case PlayerClass::Warrior:
        if (attackKind == 2) return ScaleRange({ 4.2f, 2.4f, 45.0f, 0.10f, true });
        if (attackKind == 1) return ScaleRange({ 3.8f, 1.8f, 32.0f, 0.35f, true });
        return ScaleRange({ 2.3f, 0.95f, 18.0f, 0.55f, true });

    case PlayerClass::Mage:
        if (attackKind == 2) return ScaleRange({ 14.0f, 1.8f, 42.0f, 0.35f, true });
        if (attackKind == 1) return ScaleRange({ 12.0f, 1.3f, 28.0f, 0.45f, true });
        return ScaleRange({ 10.0f, 1.0f, 16.0f, 0.55f, false });

    case PlayerClass::Archer:
        if (attackKind == 2) return ScaleRange({ 18.0f, 1.2f, 38.0f, 0.50f, false });
        if (attackKind == 1) return ScaleRange({ 15.0f, 1.0f, 26.0f, 0.60f, false });
        return ScaleRange({ 12.0f, 0.7f, 17.0f, 0.70f, false });

    case PlayerClass::None:
    default:
        return ScaleRange({ 2.5f, 1.0f, 10.0f, 0.40f, false });
    }
}

float CombatSystem::GetHitDelay(int attackKind, int basicAttackVariant) const
{
    if (attackKind == 2)
    {
        return kSkill2HitDelay;
    }

    if (attackKind == 1)
    {
        return kSkill1HitDelay;
    }

    return basicAttackVariant == 2 ? kBasicAttack2HitDelay : kBasicAttack1HitDelay;
}

void CombatSystem::QueueAttack(Player* player, int skillType, int attackKind, const AttackProfile& profile)
{
    if (player == nullptr)
    {
        return;
    }

    PendingAttack attack;
    attack.Profile = profile;
    attack.Origin = player->GetPosition();
    attack.RotY = player->GetFacingRotY();
    attack.SkillType = skillType;
    attack.AttackKind = attackKind;
    attack.BasicAttackVariant = attackKind == 0 ? player->GetLastBasicAttackVariant() : 1;
    attack.Timer = GetHitDelay(attackKind, attack.BasicAttackVariant);
    mPendingAttacks.push_back(attack);

    OutputDebugStringA("[CombatSystem] Attack queued until swing hit frame\n");
}

void CombatSystem::UpdatePendingAttacks(float dt, const std::vector<Monster*>& monsters)
{
    for (size_t i = 0; i < mPendingAttacks.size();)
    {
        PendingAttack& attack = mPendingAttacks[i];
        attack.Timer -= dt;
        if (attack.Timer > 0.0f)
        {
            ++i;
            continue;
        }

        if (mDebugHitboxEnabled)
        {
            ShowDebugHitbox(attack.Origin, attack.RotY, attack.Profile, attack.AttackKind);
        }

        const XMFLOAT3 attackForward =
        {
            std::sin(attack.RotY),
            0.0f,
            std::cos(attack.RotY)
        };
        ApplyAttack(attack.Origin, attackForward, monsters, attack.Profile);
        SendServerAttack(attack);

        OutputDebugStringA("[CombatSystem] Attack hit frame executed\n");
        mPendingAttacks.erase(mPendingAttacks.begin() + i);
    }
}

void CombatSystem::SendServerAttack(const PendingAttack& attack) const
{
    NetworkManager::Get()->SendPlayerAttack(
        attack.SkillType,
        attack.Origin.x,
        attack.Origin.y,
        attack.Origin.z,
        attack.RotY,
        attack.Profile.range,
        attack.Profile.radius,
        attack.Profile.coneDot);
}

bool CombatSystem::EnsureDebugHitbox()
{
    if (static_cast<int>(mDebugHitboxSegments.size()) == kDebugHitboxSegmentCount)
    {
        return true;
    }

    if (mGame == nullptr || mGame->GetResources() == nullptr)
    {
        return false;
    }

    auto* resources = mGame->GetResources();
    auto geoIt = resources->mGeometries.find("boxGeo");
    if (geoIt == resources->mGeometries.end())
    {
        return false;
    }

    if (resources->GetMaterial("AttackRangeDebugMat") == nullptr)
    {
        resources->CreateMaterial(
            "AttackRangeDebugMat",
            static_cast<int>(resources->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 0.75f, 0.1f, 0.24f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            0.35f);
    }

    Material* debugMat = resources->GetMaterial("AttackRangeDebugMat");
    if (debugMat == nullptr)
    {
        return false;
    }
    debugMat->IsTransparent = 1;
    debugMat->NumFramesDirty = gNumFrameResources;

    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();
    const auto& drawArgs = geoIt->second->DrawArgs["box"];

    while (static_cast<int>(mDebugHitboxSegments.size()) < kDebugHitboxSegmentCount)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
        ritem->Mat = debugMat;
        ritem->Geo = geoIt->second.get();
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = drawArgs.IndexCount;
        ritem->StartIndexLocation = drawArgs.StartIndexLocation;
        ritem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        ritem->Visible = false;

        auto object = std::make_unique<GameObject>();
        object->Ritem = ritem.get();
        object->SetScale(0.0f, 0.0f, 0.0f);
        object->Update();

        mDebugHitboxSegments.push_back({ object.get(), ritem.get() });
        ritems.push_back(std::move(ritem));
        objects.push_back(std::move(object));
    }

    return true;
}

void CombatSystem::ShowDebugHitbox(
    const XMFLOAT3& attackOrigin,
    float attackRotY,
    const AttackProfile& profile,
    int attackKind)
{
    if (!EnsureDebugHitbox())
    {
        return;
    }

    const float bottomY = attackOrigin.y - Player::DefaultColliderHalfHeight + kDebugHitboxBottomOffset;
    const float volumeHalfHeight = (attackKind == 0) ? kDebugHitboxBasicHalfHeight : kDebugHitboxSkillHalfHeight;
    const float volumeCenterY = bottomY + volumeHalfHeight;
    const float baseRotationY = attackRotY;
    const float coneDot = (std::clamp)(profile.coneDot, -0.999f, 0.999f);
    const float halfAngle = std::acos(coneDot);
    const float angleStep = (halfAngle * 2.0f) / static_cast<float>(kDebugHitboxSegmentCount);

    Material* debugMat = mGame->GetResources()->GetMaterial("AttackRangeDebugMat");
    if (debugMat != nullptr)
    {
        if (attackKind == 1)
        {
            debugMat->DiffuseAlbedo = XMFLOAT4(0.10f, 0.85f, 1.0f, 0.16f);
        }
        else if (attackKind == 2)
        {
            debugMat->DiffuseAlbedo = XMFLOAT4(1.0f, 0.28f, 0.06f, 0.18f);
        }
        else
        {
            debugMat->DiffuseAlbedo = XMFLOAT4(1.0f, 0.78f, 0.12f, 0.14f);
        }
        debugMat->NumFramesDirty = gNumFrameResources;
    }

    for (int i = 0; i < kDebugHitboxSegmentCount; ++i)
    {
        const float localAngle = -halfAngle + angleStep * (static_cast<float>(i) + 0.5f);
        const float sinAngle = std::sin(localAngle);
        const float sideLimitedDistance =
            std::fabs(sinAngle) > 0.0001f ? (profile.radius / std::fabs(sinAngle)) : profile.range;
        const float segmentLength = (std::min)(profile.range, sideLimitedDistance);
        const float halfLength = segmentLength * 0.5f;
        const float visualHalfWidth = (std::max)(0.018f, segmentLength * std::tan(angleStep * 0.5f) * 1.05f);

        const float worldAngle = baseRotationY + localAngle;
        const float dirX = std::sin(worldAngle);
        const float dirZ = std::cos(worldAngle);
        auto& segment = mDebugHitboxSegments[i];
        if (segment.Object == nullptr || segment.Ritem == nullptr)
        {
            continue;
        }

        segment.Object->SetScale(visualHalfWidth, volumeHalfHeight, halfLength);
        segment.Object->SetPosition(
            attackOrigin.x + dirX * halfLength,
            volumeCenterY,
            attackOrigin.z + dirZ * halfLength);
        segment.Object->SetRotation(0.0f, worldAngle, 0.0f);
        segment.Object->Update();

        segment.Ritem->Visible = true;
        segment.Ritem->NumFramesDirty = gNumFrameResources;
    }

    mDebugHitboxTimer = 1.0f;
}

void CombatSystem::UpdateDebugHitbox(float dt)
{
    if (mDebugHitboxTimer <= 0.0f)
    {
        return;
    }

    mDebugHitboxTimer -= dt;
    if (mDebugHitboxTimer <= 0.0f)
    {
        for (const auto& segment : mDebugHitboxSegments)
        {
            if (segment.Ritem != nullptr)
            {
                segment.Ritem->Visible = false;
                segment.Ritem->NumFramesDirty = gNumFrameResources;
            }
        }
    }
}

int CombatSystem::ApplyAttack(
    const XMFLOAT3& attackOrigin,
    const XMFLOAT3& attackForward,
    const std::vector<Monster*>& monsters,
    const AttackProfile& profile)
{
    Monster* closestMonster = nullptr;
    float closestDistanceSq = FLT_MAX;
    std::vector<Monster*> hitMonsters;

    for (Monster* monster : monsters)
    {
        if (monster == nullptr)
        {
            continue;
        }

        const MonsterState monsterState = monster->GetState();
        if (monsterState == MonsterState::DIE || monsterState == MonsterState::DYING)
        {
            continue;
        }

        const XMFLOAT3 monsterPos = monster->GetPosition();
        const float dx = monsterPos.x - attackOrigin.x;
        const float dz = monsterPos.z - attackOrigin.z;
        const float distanceSq = (dx * dx) + (dz * dz);
        if (distanceSq > (profile.range * profile.range))
        {
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        const float dirX = (distance > 0.001f) ? (dx / distance) : 0.0f;
        const float dirZ = (distance > 0.001f) ? (dz / distance) : 1.0f;
        const float dot = (dirX * attackForward.x) + (dirZ * attackForward.z);
        if (dot < profile.coneDot)
        {
            continue;
        }

        const float projected = (dx * attackForward.x) + (dz * attackForward.z);
        const float sideX = dx - (attackForward.x * projected);
        const float sideZ = dz - (attackForward.z * projected);
        const float sideDistanceSq = (sideX * sideX) + (sideZ * sideZ);
        if (sideDistanceSq > (profile.radius * profile.radius))
        {
            continue;
        }

        if (profile.hitAll)
        {
            hitMonsters.push_back(monster);
        }
        else if (distanceSq < closestDistanceSq)
        {
            closestDistanceSq = distanceSq;
            closestMonster = monster;
        }
    }

    if (!profile.hitAll && closestMonster != nullptr)
    {
        hitMonsters.push_back(closestMonster);
    }

    for (Monster* monster : hitMonsters)
    {
        const XMFLOAT3 monsterPos = monster->GetPosition();
        XMFLOAT3 textPosition =
        {
            monsterPos.x,
            monsterPos.y + monster->GetColliderHalfHeight() * 0.45f,
            monsterPos.z
        };

        monster->OnDamaged(profile.damage);
        if (mDamageTextCallback)
        {
            mDamageTextCallback(textPosition, profile.damage);
        }
    }

    if (!hitMonsters.empty())
    {
        OutputDebugStringA("[CombatSystem] 몬스터 타격 성공\n");
    }

    return static_cast<int>(hitMonsters.size());
}

void CombatSystem::HandleDebugHitboxToggle()
{
    const bool toggleDown = (GetAsyncKeyState('1') & 0x8000) != 0;
    if (toggleDown && !mDebugHitboxTogglePressed)
    {
        mDebugHitboxEnabled = !mDebugHitboxEnabled;
        if (!mDebugHitboxEnabled)
        {
            HideDebugHitbox();
        }

        OutputDebugStringA(mDebugHitboxEnabled
            ? "[CombatSystem] Hitbox debug enabled\n"
            : "[CombatSystem] Hitbox debug disabled\n");
    }

    mDebugHitboxTogglePressed = toggleDown;
}

void CombatSystem::HideDebugHitbox()
{
    mDebugHitboxTimer = 0.0f;

    for (const auto& segment : mDebugHitboxSegments)
    {
        if (segment.Ritem != nullptr)
        {
            segment.Ritem->Visible = false;
            segment.Ritem->NumFramesDirty = gNumFrameResources;
        }
    }
}
