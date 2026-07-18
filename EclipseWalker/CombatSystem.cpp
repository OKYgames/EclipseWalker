#include "CombatSystem.h"

#include "Archer.h"
#include "AudioManager.h"
#include "Camera.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "MapSystem.h"
#include "Material.h"
#include "Mage.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SkillEffectManager.h"
#include "Stage2BossController.h"
#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <sstream>
#include <utility>

using namespace DirectX;

namespace
{
    constexpr int kDebugHitboxSegmentCount = 64;
    constexpr float kDebugHitboxBottomOffset = 0.035f;
    constexpr float kDebugHitboxBasicHalfHeight = 0.42f;
    constexpr float kDebugHitboxSkillHalfHeight = 0.55f;
    constexpr float kMaxTargetSelectDistance = 10.0f;
    // 공격 판정 지연
    constexpr float kBasicAttack1HitDelay = 0.40f;
    constexpr float kBasicAttack2HitDelay = 0.54f;
    constexpr float kDefaultSkill1HitDelay = 1.0f; // Q
    constexpr float kDefaultSkill2HitDelay = 0.42f;
    constexpr float kWarriorSwordStrikeSpawnDelay = 2.1f; // E 검 소환 시간
    constexpr float kWarriorSwordStrikeImpactDelay = 1.35f; // E 검 판정 시간

    // attack1
    constexpr float kWarriorBasicAttack1HitStopDuration = 0.12f;
    constexpr float kWarriorBasicAttack1HitStopTimeScale = 0.01f;

    constexpr float kWarriorBasicAttack1VictimHitStopDelay = 0.05f;

    constexpr float kWarriorBasicAttack1VictimHitStopDuration = 0.1f;
    constexpr float kWarriorBasicAttack1VictimHitStopTimeScale = 0.01f;

    constexpr float kWarriorBasicAttack1VictimKnockbackDistance = 0.2f;
    constexpr float kWarriorBasicAttack1VictimKnockbackDuration = 0.06f;

    // attack2
    constexpr float kWarriorBasicAttack2HitStopDuration = 0.12f;
    constexpr float kWarriorBasicAttack2HitStopTimeScale = 0.01f;

    constexpr float kWarriorBasicAttack2VictimHitStopDelay = 0.05f;

    constexpr float kWarriorBasicAttack2VictimHitStopDuration = 0.1f;
    constexpr float kWarriorBasicAttack2VictimHitStopTimeScale = 0.01f;

    constexpr float kWarriorBasicAttack2VictimKnockbackDistance = 0.2f;
    constexpr float kWarriorBasicAttack2VictimKnockbackDuration = 0.06f;

    constexpr float kWarriorSkill1VictimKnockbackDistance = 0.55f;
    constexpr float kWarriorSkill1VictimKnockbackDuration = 0.12f;

    constexpr int kArcherArrowRainHitCount = 3;
    constexpr float kArcherArrowRainHitInterval = 0.18f;
    // 궁수 기본공격은 실제 화살 이동 구간을 따라 판정하므로
    // 약간 넉넉한 폭과 전방 허용 각도를 줘야 체감상 누락이 줄어든다.
    constexpr float kArcherArrowCollisionRadius = 0.72f;
    constexpr float kArcherArrowCollisionMinRange = 0.50f;
    constexpr float kArcherArrowCollisionConeDot = 0.91f;
    constexpr wchar_t kWarriorSkill1ImpactSound[] = L"Sounds\\Warrior\\Warrior_EarthquakeSlam_Impact.mp3";
    constexpr wchar_t kWarriorSkill2ImpactSound[] = L"Sounds\\Warrior\\Warrior_GreatswordSummon_SwordFall.mp3";
    constexpr wchar_t kMageMeteorImpactSound[] = L"Sounds\\Mage\\Mage_Meteor_Impact.mp3";
    constexpr float kWarriorImpactVolume = 0.13f;
    constexpr float kMageMeteorImpactVolume = 0.12f;

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

    XMFLOAT3 GetVictimKnockbackDirection(const Player* sourcePlayer, const XMFLOAT3& victimPosition, float fallbackRotY)
    {
        XMVECTOR direction = XMVectorSet(
            std::sin(fallbackRotY),
            0.0f,
            std::cos(fallbackRotY),
            0.0f);

        if (sourcePlayer != nullptr)
        {
            const XMFLOAT3 sourcePos = sourcePlayer->GetPosition();
            direction = XMVectorSet(
                victimPosition.x - sourcePos.x,
                0.0f,
                victimPosition.z - sourcePos.z,
                0.0f);
            if (XMVectorGetX(XMVector3LengthSq(direction)) <= 0.0001f)
            {
                direction = XMVectorSet(
                    std::sin(fallbackRotY),
                    0.0f,
                    std::cos(fallbackRotY),
                    0.0f);
            }
        }

        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3Normalize(direction));
        return result;
    }

    bool IsMonsterSelectable(Monster* monster)
    {
        return monster != nullptr &&
            monster->Ritem != nullptr &&
            monster->Ritem->Visible &&
            monster->GetState() != MonsterState::DIE &&
            monster->GetState() != MonsterState::DYING;
    }

    int SplitIntegerDamage(int totalDamage, int impactCount, int impactIndex)
    {
        if (impactCount <= 0)
        {
            return totalDamage;
        }

        const int baseDamage = totalDamage / impactCount;
        const int remainder = totalDamage % impactCount;
        return baseDamage + (impactIndex < remainder ? 1 : 0);
    }

    bool IsMonsterWithinSelectableDistance(const Player* player, const Monster* monster)
    {
        if (player == nullptr || monster == nullptr)
        {
            return false;
        }

        const XMFLOAT3 playerPos = player->GetPosition();
        const XMFLOAT3 monsterPos = monster->GetPosition();
        const float dx = monsterPos.x - playerPos.x;
        const float dz = monsterPos.z - playerPos.z;
        const float distanceSq = dx * dx + dz * dz;
        return distanceSq <= (kMaxTargetSelectDistance * kMaxTargetSelectDistance);
    }

    XMFLOAT3 GetMonsterGroundPosition(const Monster* monster)
    {
        if (monster == nullptr)
        {
            return { 0.0f, 0.0f, 0.0f };
        }

        XMFLOAT3 groundPosition = monster->GetPosition();
        groundPosition.y -= monster->GetColliderHalfHeight();
        groundPosition.y += 0.02f;
        return groundPosition;
    }

    float GetProjectedHalfExtent2D(const XMFLOAT3& extents, float axisX, float axisZ)
    {
        return std::abs(axisX) * extents.x + std::abs(axisZ) * extents.z;
    }
}

CombatSystem::CombatSystem(EclipseWalkerGame* game)
    : mGame(game)
{
}

void CombatSystem::Reset()
{
    ClearSelectedMonster();
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
    mDamageTextCallback = nullptr;
    mBlockedHitCallback = nullptr;
    mTargetSelectionOverridePicker = nullptr;
    mTargetSelectionOverrideValidityCallback = nullptr;
    mTargetSelectionEnabled = true;
    HideDebugHitbox();
}

void CombatSystem::Update(const GameTimer& gt, Player* player, const std::vector<Monster*>& monsters, MapSystem* mapSystem)
{
    UpdateDebugHitbox(gt.DeltaTime());
    ValidateSelectedMonster(monsters);

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

    player->RestoreMP(GetManaRegenPerSecond(player->GetClassType()) * gt.DeltaTime());
    UpdateCooldowns(gt.DeltaTime());
    UpdatePendingAttacks(gt.DeltaTime(), monsters);
    if (auto* archer = dynamic_cast<Archer*>(player))
    {
        archer->UpdateArrows(
            gt.DeltaTime(),
            [this, player, mapSystem, &monsters](
                const XMFLOAT3& previousPosition,
                const XMFLOAT3& currentPosition,
                float rotY)
            {
                return ResolveArrowCollision(player, previousPosition, currentPosition, rotY, mapSystem, monsters);
            });
    }

    const bool hasFocus = (mGame != nullptr && GetForegroundWindow() == mGame->GetMainWindowHandle());
    if (gIsChatInputActive || !hasFocus)
    {
        mLeftMousePressed = false;
        mQKeyPressed = false;
        mEKeyPressed = false;
        mDebugHitboxTogglePressed = false;
        return;
    }

    const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!mTargetSelectionEnabled)
    {
        ClearSelectedMonster();
        mLeftMousePressed = leftDown;
    }
    else if (gIsLanternUiInputActive)
    {
        mLeftMousePressed = leftDown;
    }
    else if (leftDown && !mLeftMousePressed)
    {
        Monster* previouslySelectedMonster = mSelectedMonster;
        const bool hadSelectedOverride = mHasSelectedTargetOverride;
        TargetSelectionOverride clickedOverride;
        if (Monster* clickedMonster = PickMonsterUnderCursor(monsters, &clickedOverride))
        {
            if (clickedMonster != previouslySelectedMonster || hadSelectedOverride)
            {
                SetSelectedMonster(clickedMonster);
            }
            else
            {
                TryBasicAttack(player, monsters);
            }
        }
        else if (clickedOverride.HighlightRenderItem != nullptr)
        {
            const bool overrideChanged =
                !mHasSelectedTargetOverride ||
                mSelectedTargetRenderItem != clickedOverride.HighlightRenderItem ||
                mSelectedTargetOverrideMonsterId != clickedOverride.MonsterId;
            if (overrideChanged || mSelectedMonster != nullptr)
            {
                SetSelectedTargetOverride(clickedOverride);
            }
            else
            {
                TryBasicAttack(player, monsters);
            }
        }
        else
        {
            ClearSelectedMonster();
        }
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

float CombatSystem::GetSkillCooldownRemaining(int skillIndex) const
{
    return skillIndex == 2 ? mSkill2Cooldown : mSkill1Cooldown;
}

float CombatSystem::GetSkillCooldownDuration(PlayerClass playerClass, int skillIndex) const
{
    switch (playerClass)
    {
    case PlayerClass::Warrior:
        return skillIndex == 1 ? 6.0f : 10.0f;
    case PlayerClass::Mage:
        return skillIndex == 1 ? 6.0f : 12.0f;
    case PlayerClass::Archer:
        return skillIndex == 1 ? 8.0f : 10.0f;
    case PlayerClass::None:
    default:
        return 0.0f;
    }
}

void CombatSystem::ValidateSelectedMonster(const std::vector<Monster*>& monsters)
{
    if (mHasSelectedTargetOverride)
    {
        if (mTargetSelectionOverrideValidityCallback != nullptr &&
            !mTargetSelectionOverrideValidityCallback())
        {
            ClearSelectedMonster();
        }
        return;
    }

    if (mSelectedMonster == nullptr)
    {
        return;
    }

    const bool stillExists = std::find(monsters.begin(), monsters.end(), mSelectedMonster) != monsters.end();
    Player* player = (mGame != nullptr) ? mGame->GetPlayer() : nullptr;
    if (!stillExists || !IsMonsterSelectable(mSelectedMonster) || !IsMonsterWithinSelectableDistance(player, mSelectedMonster))
    {
        ClearSelectedMonster();
    }
}

bool CombatSystem::HasSelectedTarget() const
{
    return mHasSelectedTargetOverride
        ? (mSelectedTargetRenderItem != nullptr)
        : IsMonsterSelectable(mSelectedMonster);
}

bool CombatSystem::HasSelectedTargetOverride() const
{
    return mHasSelectedTargetOverride;
}

XMFLOAT3 CombatSystem::GetSelectedTargetPosition() const
{
    if (mHasSelectedTargetOverride)
    {
        return mSelectedTargetOverridePosition;
    }

    return mSelectedMonster != nullptr ? mSelectedMonster->GetPosition() : XMFLOAT3{};
}

XMFLOAT3 CombatSystem::GetSelectedTargetGroundPosition() const
{
    XMFLOAT3 targetPosition = GetSelectedTargetPosition();
    const float halfHeight = mHasSelectedTargetOverride
        ? mSelectedTargetOverrideHalfHeight
        : (mSelectedMonster != nullptr ? mSelectedMonster->GetColliderHalfHeight() : 0.0f);
    targetPosition.y -= halfHeight;
    targetPosition.y += 0.02f;
    return targetPosition;
}

int CombatSystem::GetSelectedTargetMonsterId() const
{
    if (mHasSelectedTargetOverride)
    {
        return mSelectedTargetOverrideMonsterId;
    }

    return mSelectedMonster != nullptr ? mSelectedMonster->GetNetworkId() : -1;
}

Monster* CombatSystem::FindFallbackSkillTarget(Player* player, const std::vector<Monster*>& monsters) const
{
    if (player == nullptr)
    {
        return nullptr;
    }

    const XMFLOAT3 playerPos = player->GetPosition();
    const float forwardX = std::sin(player->GetFacingRotY());
    const float forwardZ = std::cos(player->GetFacingRotY());
    Monster* bestFrontTarget = nullptr;
    Monster* bestAnyTarget = nullptr;
    float bestFrontScore = FLT_MAX;
    float bestAnyDistanceSq = FLT_MAX;

    for (Monster* monster : monsters)
    {
        if (!IsMonsterSelectable(monster) || !IsMonsterWithinSelectableDistance(player, monster))
        {
            continue;
        }

        const XMFLOAT3 monsterPos = monster->GetPosition();
        const float dx = monsterPos.x - playerPos.x;
        const float dz = monsterPos.z - playerPos.z;
        const float distanceSq = dx * dx + dz * dz;
        if (distanceSq < bestAnyDistanceSq)
        {
            bestAnyDistanceSq = distanceSq;
            bestAnyTarget = monster;
        }

        const float distance = std::sqrt(distanceSq);
        const float dirX = distance > 0.001f ? dx / distance : forwardX;
        const float dirZ = distance > 0.001f ? dz / distance : forwardZ;
        const float dot = dirX * forwardX + dirZ * forwardZ;
        if (dot < -0.15f)
        {
            continue;
        }

        const float score = distanceSq - dot * 2.0f;
        if (score < bestFrontScore)
        {
            bestFrontScore = score;
            bestFrontTarget = monster;
        }
    }

    return bestFrontTarget != nullptr ? bestFrontTarget : bestAnyTarget;
}

Monster* CombatSystem::PickMonsterUnderCursor(const std::vector<Monster*>& monsters, TargetSelectionOverride* outOverrideTarget) const
{
    if (mGame == nullptr || mGame->GetCamera() == nullptr)
    {
        return nullptr;
    }

    Player* player = mGame->GetPlayer();
    if (player == nullptr)
    {
        return nullptr;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
    {
        return nullptr;
    }

    RECT clientRect{};
    if (!GetClientRect(mGame->GetMainWindowHandle(), &clientRect))
    {
        return nullptr;
    }

    const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (clientWidth <= 0.0f || clientHeight <= 0.0f)
    {
        return nullptr;
    }

    mGame->GetCamera()->UpdateViewMatrix();

    XMFLOAT4X4 proj4x4;
    XMStoreFloat4x4(&proj4x4, mGame->GetCamera()->GetProj());
    const float projX = proj4x4._11;
    const float projY = proj4x4._22;
    if (std::fabs(projX) <= 0.0001f || std::fabs(projY) <= 0.0001f)
    {
        return nullptr;
    }

    const float viewX = ((2.0f * static_cast<float>(cursor.x)) / clientWidth - 1.0f) / projX;
    const float viewY = ((-2.0f * static_cast<float>(cursor.y)) / clientHeight + 1.0f) / projY;

    const XMVECTOR rayDirView = XMVector3Normalize(XMVectorSet(viewX, viewY, 1.0f, 0.0f));
    const XMMATRIX invView = XMMatrixInverse(nullptr, mGame->GetCamera()->GetView());
    const XMVECTOR rayOrigin = mGame->GetCamera()->GetPosition();
    const XMVECTOR rayDir = XMVector3Normalize(XMVector3TransformNormal(rayDirView, invView));

    Monster* pickedMonster = nullptr;
    float closestHitDistance = FLT_MAX;

    for (Monster* monster : monsters)
    {
        if (!IsMonsterSelectable(monster) || !IsMonsterWithinSelectableDistance(player, monster))
        {
            continue;
        }

        DirectX::BoundingBox box;
        box.Center = monster->GetPosition();
        box.Extents = monster->GetColliderExtents();
        box.Extents.x *= 1.25f;
        box.Extents.y *= 1.15f;
        box.Extents.z *= 1.25f;

        float hitDistance = 0.0f;
        if (!box.Intersects(rayOrigin, rayDir, hitDistance))
        {
            continue;
        }

        if (hitDistance < closestHitDistance)
        {
            closestHitDistance = hitDistance;
            pickedMonster = monster;
        }
    }

    if (outOverrideTarget != nullptr)
    {
        *outOverrideTarget = {};
        if (mTargetSelectionOverridePicker != nullptr)
        {
            XMFLOAT3 rayOriginValue;
            XMFLOAT3 rayDirValue;
            XMStoreFloat3(&rayOriginValue, rayOrigin);
            XMStoreFloat3(&rayDirValue, rayDir);

            TargetSelectionOverride overrideTarget;
            if (mTargetSelectionOverridePicker(rayOriginValue, rayDirValue, player, overrideTarget) &&
                overrideTarget.HighlightRenderItem != nullptr &&
                (pickedMonster == nullptr || overrideTarget.HitDistance < closestHitDistance))
            {
                *outOverrideTarget = overrideTarget;
                return nullptr;
            }
        }
    }

    return pickedMonster;
}

void CombatSystem::SetSelectedMonster(Monster* monster)
{
    ClearSelectedMonster();

    if (!IsMonsterSelectable(monster) || mGame == nullptr || mGame->GetResources() == nullptr)
    {
        return;
    }

    auto* resources = mGame->GetResources();
    auto* renderItem = monster->Ritem;
    if (renderItem == nullptr)
    {
        return;
    }

    auto* baseMaterial = renderItem->Mat;
    if (baseMaterial == nullptr)
    {
        return;
    }

    std::ostringstream nameBuilder;
    nameBuilder << "MonsterTargetHighlightMat_" << renderItem->ObjCBIndex;
    const std::string highlightName = nameBuilder.str();

    Material* highlightMaterial = resources->GetMaterial(highlightName);
    if (highlightMaterial == nullptr)
    {
        auto newMaterial = std::make_unique<Material>(*baseMaterial);
        newMaterial->Name = highlightName;
        newMaterial->MatCBIndex = static_cast<int>(resources->mMaterials.size());
        newMaterial->NumFramesDirty = gNumFrameResources;
        auto insertResult = resources->mMaterials.emplace(highlightName, std::move(newMaterial));
        highlightMaterial = insertResult.first->second.get();
    }

    highlightMaterial->DiffuseMapName = baseMaterial->DiffuseMapName;
    highlightMaterial->NormalMapName = baseMaterial->NormalMapName;
    highlightMaterial->EmissiveMapName = baseMaterial->EmissiveMapName;
    highlightMaterial->MetallicMapName = baseMaterial->MetallicMapName;
    highlightMaterial->DiffuseAlbedo = baseMaterial->DiffuseAlbedo;
    highlightMaterial->FresnelR0 = baseMaterial->FresnelR0;
    highlightMaterial->Roughness = baseMaterial->Roughness;
    highlightMaterial->IsTransparent = baseMaterial->IsTransparent;
    highlightMaterial->IsToon = 0;
    highlightMaterial->OutlineThickness = 1.00f;
    highlightMaterial->OutlineColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    highlightMaterial->NumFramesDirty = gNumFrameResources;

    mSelectedMonster = monster;
    mHasSelectedTargetOverride = false;
    mSelectedTargetOverridePosition = {};
    mSelectedTargetOverrideHalfHeight = 0.0f;
    mSelectedTargetOverrideMonsterId = -1;
    mSelectedTargetRenderItem = renderItem;
    mSelectedMonsterBaseMaterial = baseMaterial;
    mSelectedMonsterBaseColorMultiplier = renderItem->ColorMultiplier;
    renderItem->Mat = highlightMaterial;
    renderItem->NumFramesDirty = gNumFrameResources;
}

void CombatSystem::SetSelectedTargetOverride(const TargetSelectionOverride& overrideTarget)
{
    ClearSelectedMonster();

    if (overrideTarget.HighlightRenderItem == nullptr ||
        mGame == nullptr ||
        mGame->GetResources() == nullptr)
    {
        return;
    }

    auto* resources = mGame->GetResources();
    auto* renderItem = overrideTarget.HighlightRenderItem;
    auto* baseMaterial = renderItem->Mat;
    if (baseMaterial == nullptr)
    {
        return;
    }

    std::ostringstream nameBuilder;
    nameBuilder << "MonsterTargetHighlightMat_" << renderItem->ObjCBIndex;
    const std::string highlightName = nameBuilder.str();

    Material* highlightMaterial = resources->GetMaterial(highlightName);
    if (highlightMaterial == nullptr)
    {
        auto newMaterial = std::make_unique<Material>(*baseMaterial);
        newMaterial->Name = highlightName;
        newMaterial->MatCBIndex = static_cast<int>(resources->mMaterials.size());
        newMaterial->NumFramesDirty = gNumFrameResources;
        auto insertResult = resources->mMaterials.emplace(highlightName, std::move(newMaterial));
        highlightMaterial = insertResult.first->second.get();
    }

    highlightMaterial->DiffuseMapName = baseMaterial->DiffuseMapName;
    highlightMaterial->NormalMapName = baseMaterial->NormalMapName;
    highlightMaterial->EmissiveMapName = baseMaterial->EmissiveMapName;
    highlightMaterial->MetallicMapName = baseMaterial->MetallicMapName;
    highlightMaterial->DiffuseAlbedo = baseMaterial->DiffuseAlbedo;
    highlightMaterial->FresnelR0 = baseMaterial->FresnelR0;
    highlightMaterial->Roughness = baseMaterial->Roughness;
    highlightMaterial->IsTransparent = baseMaterial->IsTransparent;
    highlightMaterial->IsToon = 0;
    highlightMaterial->OutlineThickness = 1.00f;
    highlightMaterial->OutlineColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    highlightMaterial->NumFramesDirty = gNumFrameResources;

    mSelectedMonster = nullptr;
    mHasSelectedTargetOverride = true;
    mSelectedTargetOverridePosition = overrideTarget.Position;
    mSelectedTargetOverrideHalfHeight = overrideTarget.HalfHeight;
    mSelectedTargetOverrideMonsterId = overrideTarget.MonsterId;
    mSelectedTargetRenderItem = renderItem;
    mSelectedMonsterBaseMaterial = baseMaterial;
    mSelectedMonsterBaseColorMultiplier = renderItem->ColorMultiplier;
    renderItem->Mat = highlightMaterial;
    renderItem->NumFramesDirty = gNumFrameResources;
}

void CombatSystem::ClearSelectedMonster()
{
    if (mSelectedTargetRenderItem != nullptr &&
        mSelectedMonsterBaseMaterial != nullptr)
    {
        mSelectedTargetRenderItem->Mat = mSelectedMonsterBaseMaterial;
        mSelectedTargetRenderItem->ColorMultiplier = mSelectedMonsterBaseColorMultiplier;
        mSelectedTargetRenderItem->NumFramesDirty = gNumFrameResources;
    }

    mSelectedMonster = nullptr;
    mHasSelectedTargetOverride = false;
    mSelectedTargetOverridePosition = {};
    mSelectedTargetOverrideHalfHeight = 0.0f;
    mSelectedTargetOverrideMonsterId = -1;
    mSelectedTargetRenderItem = nullptr;
    mSelectedMonsterBaseMaterial = nullptr;
    mSelectedMonsterBaseColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
}

void CombatSystem::SetDamageTextCallback(std::function<void(const XMFLOAT3&, float)> callback)
{
    mDamageTextCallback = std::move(callback);
}

void CombatSystem::SetBlockedHitCallback(std::function<bool(Monster*, const XMFLOAT3&)> callback)
{
    mBlockedHitCallback = std::move(callback);
}

void CombatSystem::SetSkillEffectManager(SkillEffectManager* skillEffectManager)
{
    mSkillEffectManager = skillEffectManager;
}

void CombatSystem::ClearSelectedTarget()
{
    ClearSelectedMonster();
}

void CombatSystem::SetTargetSelectionEnabled(bool enabled)
{
    mTargetSelectionEnabled = enabled;
    if (!mTargetSelectionEnabled)
    {
        ClearSelectedMonster();
    }
}

void CombatSystem::SetTargetSelectionOverridePicker(
    std::function<bool(const XMFLOAT3&, const XMFLOAT3&, const Player*, TargetSelectionOverride&)> callback)
{
    mTargetSelectionOverridePicker = std::move(callback);
}

void CombatSystem::SetTargetSelectionOverrideValidityCallback(std::function<bool()> callback)
{
    mTargetSelectionOverrideValidityCallback = std::move(callback);
}

void CombatSystem::ApplyMonsterKillReward(Player* player, int experienceReward)
{
    if (player == nullptr || experienceReward <= 0)
    {
        return;
    }

    const bool leveledUp = player->AddExperience(experienceReward);
    player->ForceSendNetworkState();

    if (!leveledUp || mGame == nullptr || player != mGame->GetPlayer())
    {
        return;
    }

    if (mSkillEffectManager != nullptr)
    {
        mSkillEffectManager->TriggerLevelUpEffect(
            player->GetPosition(),
            player->GetFacingRotY(),
            player->GetClassType(),
            player->GetLevel());
    }

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->TriggerLevelUpFlashEffect(
            player->GetClassType(),
            player->GetLevel());
    }

    if (mGame != nullptr)
    {
        mGame->SetSelectedPlayerTier(player->GetCurrentTier());
    }
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

    if (HasSelectedTarget())
    {
        player->FaceTowards(GetSelectedTargetPosition());
    }
    else
    {
        player->FaceCameraForward();
    }
    player->ForceSendNetworkState();

    if (!player->PlayRandomBasicAttack())
    {
        return;
    }

    const AttackProfile profile = ApplyPlayerStatDamage(
        GetProfile(player->GetClassType(), player->GetLevel(), 0),
        player);
    const float basicAttackSpeedMultiplier = (std::max)(player->GetBasicAttackSpeedMultiplier(), 1.0f);
    if (auto* archer = dynamic_cast<Archer*>(player))
    {
        float arrowTravelDistance = (std::max)(profile.range * 2.5f, 6.0f);
        if (HasSelectedTarget())
        {
            const XMFLOAT3 playerPos = player->GetPosition();
            const XMFLOAT3 monsterPos = GetSelectedTargetPosition();
            const float dx = monsterPos.x - playerPos.x;
            const float dz = monsterPos.z - playerPos.z;
            arrowTravelDistance = std::sqrt(dx * dx + dz * dz);
        }

        constexpr float kArcherBasicArrowFireDelay = 1.1f;
        archer->FireBasicArrow(mGame, player->GetPosition(), player->GetFacingRotY(), arrowTravelDistance);
        if (mSkillEffectManager != nullptr && archer->HasAttackSpeedBuff())
        {
            mSkillEffectManager->OnArcherHasteBasicShot(
                player->GetPosition(),
                player->GetFacingRotY(),
                arrowTravelDistance,
                kArcherBasicArrowFireDelay / basicAttackSpeedMultiplier,
                archer->GetSkillEffectIntensityMultiplier());
        }

        SendServerAttackCast(
            player,
            0,
            arrowTravelDistance,
            kArcherBasicArrowFireDelay / basicAttackSpeedMultiplier);
        mBasicCooldown = 0.28f / basicAttackSpeedMultiplier;
        return;
    }

    if (player->GetClassType() == PlayerClass::Mage)
    {
        float orbTravelDistance = (std::max)(profile.range, 4.6f);
        if (HasSelectedTarget())
        {
            const XMFLOAT3 playerPos = player->GetPosition();
            const XMFLOAT3 monsterPos = GetSelectedTargetPosition();
            const float dx = monsterPos.x - playerPos.x;
            const float dz = monsterPos.z - playerPos.z;
            orbTravelDistance = std::sqrt(dx * dx + dz * dz);
        }

        const float orbStartDelay = MageAnimationTiming::DelayFromProgress(
            player->GetAttackAnimationRemaining(),
            MageAnimationTiming::kBasicAttackEffectProgress);
        if (mSkillEffectManager != nullptr)
        {
            mSkillEffectManager->SpawnMageBasicOrb(
                player->GetPosition(),
                player->GetFacingRotY(),
                orbTravelDistance,
                orbStartDelay);
        }

        QueueAttack(player, 0, 0, profile);
        SendServerAttackCast(player, 0, orbTravelDistance, orbStartDelay);
        mBasicCooldown = 0.28f / basicAttackSpeedMultiplier;
        return;
    }

    QueueAttack(player, 0, 0, profile);
    SendServerAttackCast(player, 0);
    mBasicCooldown = 0.28f / basicAttackSpeedMultiplier;
}

bool CombatSystem::ResolveArrowCollision(
    Player* player,
    const XMFLOAT3& previousPosition,
    const XMFLOAT3& currentPosition,
    float rotY,
    MapSystem* mapSystem,
    const std::vector<Monster*>& monsters)
{
    if (player == nullptr)
    {
        return false;
    }

    const float dx = currentPosition.x - previousPosition.x;
    const float dz = currentPosition.z - previousPosition.z;
    const float sweptDistance = std::sqrt(dx * dx + dz * dz);

    XMFLOAT3 attackForward =
    {
        std::sin(rotY),
        0.0f,
        std::cos(rotY)
    };
    float resolvedRotY = rotY;
    if (sweptDistance > 0.0001f)
    {
        attackForward = { dx / sweptDistance, 0.0f, dz / sweptDistance };
        resolvedRotY = std::atan2(attackForward.x, attackForward.z);
    }

    bool wallHit = false;
    float wallHitDistance = sweptDistance;
    if (mapSystem != nullptr && sweptDistance > 0.0001f)
    {
        const XMVECTOR rayOrigin = XMLoadFloat3(&previousPosition);
        const XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&currentPosition) - rayOrigin);
        float hitDistance = 0.0f;
        if (mapSystem->CastWallRay(rayOrigin, rayDir, sweptDistance + 0.05f, hitDistance))
        {
            wallHit = true;
            wallHitDistance = std::clamp(hitDistance, 0.0f, sweptDistance);
        }
    }

    AttackProfile profile = ApplyPlayerStatDamage(
        GetProfile(PlayerClass::Archer, player->GetLevel(), 0),
        player);
    profile.range = wallHit
        ? (std::max)(wallHitDistance, 0.05f)
        : (std::max)(sweptDistance + kArcherArrowCollisionMinRange, kArcherArrowCollisionMinRange);
    profile.radius = kArcherArrowCollisionRadius;
    profile.coneDot = kArcherArrowCollisionConeDot;
    profile.hitAll = false;

    PendingAttack attack;
    attack.Profile = profile;
    attack.Origin = previousPosition;
    attack.RotY = resolvedRotY;
    attack.SkillType = 0;
    attack.AttackKind = 0;
    attack.BasicAttackVariant = player->GetLastBasicAttackVariant();
    attack.ClassType = PlayerClass::Archer;
    attack.SourcePlayer = player;
    attack.TargetMonster = nullptr;

    if (mDebugHitboxEnabled)
    {
        ShowDebugHitbox(attack.Origin, attack.RotY, attack.Profile, attack.AttackKind);
    }

    Monster* firstHitMonster = nullptr;
    const int hitCount = ApplyAttack(attack, attackForward, monsters, profile, &firstHitMonster);
    if (hitCount <= 0)
    {
        if (wallHit)
        {
            OutputDebugStringA("[Archer] Arrow blocked by wall\n");
            return true;
        }

        return false;
    }

    attack.TargetMonsterId = firstHitMonster != nullptr ? firstHitMonster->GetNetworkId() : -1;
    SendServerAttack(attack);
    OutputDebugStringA("[Archer] Arrow collision hit\n");
    return true;
}

void CombatSystem::TrySkillAttack(Player* player, const std::vector<Monster*>& monsters, int skillIndex)
{
    float& cooldown = (skillIndex == 1) ? mSkill1Cooldown : mSkill2Cooldown;
    if (cooldown > 0.0f)
    {
        return;
    }

    if (player == nullptr || !player->IsSkillUnlocked(skillIndex))
    {
        return;
    }

    const float manaCost = GetSkillManaCost(player->GetClassType(), skillIndex);
    if (!player->HasMP(manaCost))
    {
        return;
    }

    const AttackProfile profile = ApplyPlayerStatDamage(
        GetProfile(player->GetClassType(), player->GetLevel(), skillIndex),
        player);
    const bool isMageHealingLight =
        player->GetClassType() == PlayerClass::Mage &&
        skillIndex == 1;
    if (!isMageHealingLight && !HasSelectedTarget())
    {
        return;
    }

    if (!isMageHealingLight && HasSelectedTarget())
    {
        player->FaceTowards(GetSelectedTargetPosition());
    }
    else if (!isMageHealingLight)
    {
        player->FaceCameraForward();
    }
    player->ForceSendNetworkState();

    if (!player->CanPlaySkillAttack(skillIndex))
    {
        return;
    }

    const bool skillActivated = (skillIndex == 1) ? player->Skill1() : player->Skill2();
    if (!skillActivated)
    {
        return;
    }

    if (!isMageHealingLight && HasSelectedTarget())
    {
        player->SetPendingSkillTargetPosition(GetSelectedTargetPosition());
    }

    if (!player->PlaySkillAttack(skillIndex))
    {
        return;
    }

    if (!player->TrySpendMP(manaCost))
    {
        return;
    }

    const float attackAnimationDuration = player->GetAttackAnimationRemaining();
    const bool isMageMeteor =
        player->GetClassType() == PlayerClass::Mage &&
        skillIndex == 2;
    const bool isArcherArrowRain =
        player->GetClassType() == PlayerClass::Archer &&
        skillIndex == 2;
    const float archerArrowFallStartDelay = isArcherArrowRain
        ? ArcherAnimationTiming::DelayFromProgress(
            attackAnimationDuration,
            ArcherAnimationTiming::kSkillEArrowFallStartProgress)
        : 0.0f;
    const float skillEffectDelay =
        player->GetClassType() == PlayerClass::Mage && skillIndex == 1
        ? MageAnimationTiming::DelayFromProgress(
            attackAnimationDuration,
            MageAnimationTiming::kSkillQEffectProgress)
        : (isMageMeteor
            ? MageAnimationTiming::DelayFromProgress(
                attackAnimationDuration,
                MageAnimationTiming::kSkillEMeteorImpactProgress)
            : (isArcherArrowRain
                ? archerArrowFallStartDelay
                : 0.0f));

    const bool isArcherWindImbuement =
        player->GetClassType() == PlayerClass::Archer &&
        skillIndex == 1;
    if (!isArcherWindImbuement && !isMageHealingLight)
    {
        QueueAttack(player, skillIndex, skillIndex, profile);
    }
    const int castTargetMonsterId = HasSelectedTarget() ? GetSelectedTargetMonsterId() : -1;
    SendServerAttackCast(player, skillIndex, 0.0f, skillEffectDelay, castTargetMonsterId);

    if (mSkillEffectManager != nullptr)
    {
        const float previewImpactDelay =
            (player->GetClassType() == PlayerClass::Warrior && skillIndex == 2)
            ? kWarriorSwordStrikeImpactDelay
            : ((player->GetClassType() == PlayerClass::Mage && skillIndex == 2)
                ? skillEffectDelay
                : ((player->GetClassType() == PlayerClass::Archer && skillIndex == 2)
                ? skillEffectDelay
                : GetHitDelay(skillIndex, 1)));

        if (player->GetClassType() == PlayerClass::Warrior &&
            skillIndex == 2)
        {
            XMFLOAT3 targetPosition;
            if (HasSelectedTarget())
            {
                targetPosition = GetSelectedTargetGroundPosition();
            }
            else
            {
                const XMFLOAT3 playerPosition = player->GetPosition();
                const float previewDistance = (std::max)((std::max)(profile.range, profile.radius), 0.75f);
                targetPosition =
                {
                    playerPosition.x + std::sin(player->GetFacingRotY()) * previewDistance,
                    playerPosition.y - Player::DefaultColliderHalfHeight + 0.02f,
                    playerPosition.z + std::cos(player->GetFacingRotY()) * previewDistance
                };
            }

            mSkillEffectManager->PreviewWarriorSwordStrike(
                targetPosition,
                player->GetFacingRotY(),
                (std::max)(profile.range, profile.radius),
                previewImpactDelay,
                kWarriorSwordStrikeSpawnDelay);
        }
        else if (player->GetClassType() == PlayerClass::Archer &&
            skillIndex == 2 &&
            HasSelectedTarget())
        {
            XMFLOAT3 targetPosition = GetSelectedTargetGroundPosition();

            mSkillEffectManager->PreviewArcherArrowRain(
                targetPosition,
                profile.radius,
                archerArrowFallStartDelay,
                ArcherAnimationTiming::kSkillEArrowFallDurationSeconds);
        }
        else if (player->GetClassType() == PlayerClass::Mage &&
            skillIndex == 2 &&
            HasSelectedTarget())
        {
            XMFLOAT3 targetPosition = GetSelectedTargetGroundPosition();

            mSkillEffectManager->PreviewMageMeteor(
                targetPosition,
                profile.radius,
                previewImpactDelay);
        }
        else
        {
            mSkillEffectManager->OnSkillCast(
                player->GetClassType(),
                skillIndex,
                player->GetPosition(),
                player->GetFacingRotY(),
                player->GetAttackAnimationRemaining(),
                skillEffectDelay);
        }
    }

    cooldown = GetSkillCooldownDuration(player->GetClassType(), skillIndex);
}

float CombatSystem::GetSkillManaCost(PlayerClass playerClass, int skillIndex) const
{
    if (skillIndex != 1 && skillIndex != 2)
    {
        return 0.0f;
    }

    switch (playerClass)
    {
    case PlayerClass::Warrior:
        return skillIndex == 1 ? 15.0f : 25.0f;
    case PlayerClass::Mage:
        return skillIndex == 1 ? 40.0f : 65.0f;
    case PlayerClass::Archer:
        return skillIndex == 1 ? 20.0f : 35.0f;
    case PlayerClass::None:
    default:
        return 0.0f;
    }
}

float CombatSystem::GetManaRegenPerSecond(PlayerClass playerClass) const
{
    switch (playerClass)
    {
    case PlayerClass::Warrior:
        return 8.0f;
    case PlayerClass::Mage:
        return 20.0f;
    case PlayerClass::Archer:
        return 12.0f;
    case PlayerClass::None:
    default:
        return 0.0f;
    }
}

CombatSystem::AttackProfile CombatSystem::GetProfile(PlayerClass playerClass, int playerLevel, int attackKind) const
{
    constexpr float kAttackForwardScale = 0.20f;
    constexpr float kAttackSideScale = 0.5f;
    (void)playerLevel;
    float basicDamage = 10.0f;

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
        if (attackKind == 2)
        {
            const AttackProfile baseProfile = ScaleRange({ 4.2f, 2.4f, 45.0f, 0.10f, true });
            const float hitRadius = (std::max)(baseProfile.range, baseProfile.radius);
            return { hitRadius, hitRadius, baseProfile.damage, -1.0f, true };
        }
        if (attackKind == 1) return { 2.4f, 2.4f, 35.0f, -1.0f, true };
        return ScaleRange({ 2.3f, 0.95f, basicDamage, 0.55f, true });

    case PlayerClass::Mage:
        if (attackKind == 2) return { 2.85f, 2.85f, 60.0f, -1.0f, true };
        if (attackKind == 1) return ScaleRange({ 12.0f, 1.3f, 0.0f, 0.45f, true });
        return { 8.0f, 0.75f, basicDamage, 0.96f, false };

    case PlayerClass::Archer:
        if (attackKind == 2) return { 2.35f, 2.35f, 51.0f, -1.0f, true };
        if (attackKind == 1) return ScaleRange({ 15.0f, 1.0f, 25.0f, 0.60f, false });
        return ScaleRange({ 12.0f, 0.7f, basicDamage, 0.70f, false });

    case PlayerClass::None:
    default:
        return ScaleRange({ 2.5f, 1.0f, basicDamage, 0.40f, false });
    }
}

CombatSystem::AttackProfile CombatSystem::ApplyPlayerStatDamage(AttackProfile profile, const Player* player) const
{
    if (player == nullptr || profile.damage <= 0.0f)
    {
        return profile;
    }

    const float statDamage =
        player->GetClassType() == PlayerClass::Mage
        ? player->GetMagicPower()
        : player->GetAttackPower();
    profile.damage += (std::max)(0.0f, statDamage);
    return profile;
}

float CombatSystem::GetHitDelay(int attackKind, int basicAttackVariant) const
{
    if (attackKind == 2)
    {
        return kDefaultSkill2HitDelay;
    }

    if (attackKind == 1)
    {
        return kDefaultSkill1HitDelay;
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
    attack.ClassType = player->GetClassType();
    attack.SourcePlayer = player;
    attack.TargetMonster = !mHasSelectedTargetOverride && IsMonsterSelectable(mSelectedMonster) ? mSelectedMonster : nullptr;
    attack.TargetMonsterId = HasSelectedTarget() ? GetSelectedTargetMonsterId() : -1;
    attack.Timer = GetHitDelay(attackKind, attack.BasicAttackVariant);
    if (attack.ClassType == PlayerClass::Warrior && attack.SkillType == 2)
    {
        attack.Timer = kWarriorSwordStrikeImpactDelay;
        if (HasSelectedTarget())
        {
            attack.Origin = GetSelectedTargetGroundPosition();
        }
        else
        {
            const float previewDistance = (std::max)((std::max)(profile.range, profile.radius), 0.75f);
            attack.Origin =
            {
                attack.Origin.x + std::sin(attack.RotY) * previewDistance,
                attack.Origin.y - Player::DefaultColliderHalfHeight + 0.02f,
                attack.Origin.z + std::cos(attack.RotY) * previewDistance
            };
        }
    }
    else if (attack.ClassType == PlayerClass::Mage && attack.SkillType == 2)
    {
        attack.Timer = MageAnimationTiming::DelayFromProgress(
            player->GetAttackAnimationRemaining(),
            MageAnimationTiming::kSkillEMeteorImpactProgress);
        if (HasSelectedTarget())
        {
            attack.Origin = GetSelectedTargetGroundPosition();
        }
    }
    else if (attack.ClassType == PlayerClass::Archer && attack.SkillType == 2)
    {
        attack.Timer = ArcherAnimationTiming::DelayFromProgress(
            player->GetAttackAnimationRemaining(),
            ArcherAnimationTiming::kSkillEHitProgress);
        if (HasSelectedTarget())
        {
            attack.Origin = GetSelectedTargetGroundPosition();
        }
    }

    XMFLOAT3 overrideOrigin;
    float overrideDelay = 0.0f;
    if (player->ConsumeQueuedSkillAttackOverride(skillType, overrideOrigin, overrideDelay))
    {
        if (attack.ClassType == PlayerClass::Warrior && attack.SkillType == 2)
        {
            attack.Timer = kWarriorSwordStrikeImpactDelay;
        }
        else
        {
            attack.Origin = overrideOrigin;
            attack.Timer = overrideDelay;
        }
    }

    if (attack.ClassType == PlayerClass::Archer && attack.SkillType == 2)
    {
        const int totalDamage = (std::max)(1, static_cast<int>(std::lround(attack.Profile.damage)));
        for (int impactIndex = 0; impactIndex < kArcherArrowRainHitCount; ++impactIndex)
        {
            PendingAttack repeatedAttack = attack;
            repeatedAttack.ImpactIndex = impactIndex;
            repeatedAttack.ImpactCount = kArcherArrowRainHitCount;
            repeatedAttack.Timer = attack.Timer + kArcherArrowRainHitInterval * static_cast<float>(impactIndex);
            repeatedAttack.Profile.damage = static_cast<float>(
                SplitIntegerDamage(totalDamage, kArcherArrowRainHitCount, impactIndex));
            mPendingAttacks.push_back(repeatedAttack);
        }
    }
    else
    {
        mPendingAttacks.push_back(attack);
    }

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

        if (attack.ClassType == PlayerClass::Warrior &&
            attack.AttackKind == 1 &&
            attack.SourcePlayer != nullptr)
        {
            attack.Origin = attack.SourcePlayer->GetPosition();
            attack.RotY = attack.SourcePlayer->GetFacingRotY();
        }

        if (mDebugHitboxEnabled &&
            !(attack.ClassType == PlayerClass::Warrior && attack.SkillType == 0))
        {
            ShowDebugHitbox(attack.Origin, attack.RotY, attack.Profile, attack.AttackKind);
        }

        const XMFLOAT3 attackForward =
        {
            std::sin(attack.RotY),
            0.0f,
            std::cos(attack.RotY)
        };
        Monster* firstHitMonster = nullptr;
        if (attack.ClassType == PlayerClass::Warrior && attack.SkillType == 0)
        {
            ApplyWarriorWeaponAttack(attack, monsters, &firstHitMonster);
        }
        else
        {
            ApplyAttack(attack, attackForward, monsters, attack.Profile, &firstHitMonster);
        }

        const bool preserveTargetedAreaTarget =
            attack.SkillType == 2 &&
            (attack.ClassType == PlayerClass::Warrior ||
                attack.ClassType == PlayerClass::Mage ||
                attack.ClassType == PlayerClass::Archer);
        if (!preserveTargetedAreaTarget)
        {
            attack.TargetMonsterId = firstHitMonster != nullptr ? firstHitMonster->GetNetworkId() : -1;
        }
        SendServerAttack(attack);

        OutputDebugStringA("[CombatSystem] Attack hit frame executed\n");
        mPendingAttacks.erase(mPendingAttacks.begin() + i);
    }
}

void CombatSystem::SendServerAttackCast(const Player* player, int skillType, float visualRange, float visualDelay, int targetMonsterId) const
{
    if (player == nullptr)
    {
        return;
    }

    const XMFLOAT3 position = player->GetPosition();
    NetworkManager::Get()->SendPlayerAttackCast(
        skillType,
        static_cast<int>(player->GetClassType()),
        player->GetLevel(),
        targetMonsterId,
        position.x,
        position.y,
        position.z,
        player->GetFacingRotY(),
        visualRange,
        visualDelay);
}

void CombatSystem::SendServerAttack(const PendingAttack& attack) const
{
    const int playerLevel = attack.SourcePlayer != nullptr ? attack.SourcePlayer->GetLevel() : Player::MinProgressionLevel;
    const PlayerAttackOrientedHitbox* orientedHitboxPayload = nullptr;
    PlayerAttackOrientedHitbox orientedHitbox = {};
    if (attack.ClassType == PlayerClass::Warrior && attack.SkillType == 0)
    {
        BoundingOrientedBox weaponHitbox;
        if (TryGetWarriorWeaponHitbox(weaponHitbox))
        {
            orientedHitbox.centerX = weaponHitbox.Center.x;
            orientedHitbox.centerY = weaponHitbox.Center.y;
            orientedHitbox.centerZ = weaponHitbox.Center.z;
            orientedHitbox.extentX = weaponHitbox.Extents.x;
            orientedHitbox.extentY = weaponHitbox.Extents.y;
            orientedHitbox.extentZ = weaponHitbox.Extents.z;
            orientedHitbox.orientationX = weaponHitbox.Orientation.x;
            orientedHitbox.orientationY = weaponHitbox.Orientation.y;
            orientedHitbox.orientationZ = weaponHitbox.Orientation.z;
            orientedHitbox.orientationW = weaponHitbox.Orientation.w;
            orientedHitboxPayload = &orientedHitbox;
        }
    }

    NetworkManager::Get()->SendPlayerAttack(
        attack.SkillType,
        static_cast<int>(attack.ClassType),
        playerLevel,
        attack.TargetMonsterId,
        attack.Origin.x,
        attack.Origin.y,
        attack.Origin.z,
        attack.RotY,
        attack.Profile.range,
        attack.Profile.radius,
        attack.Profile.coneDot,
        orientedHitboxPayload);
}

bool CombatSystem::TryGetWarriorWeaponHitbox(BoundingOrientedBox& outHitbox) const
{
    if (mGame == nullptr)
    {
        return false;
    }

    GameObject* weaponObject = mGame->GetPlayerWeaponObject();
    if (weaponObject == nullptr || weaponObject->Ritem == nullptr || !weaponObject->Ritem->Visible)
    {
        return false;
    }

    MeshGeometry* geometry = weaponObject->Ritem->Geo;
    if (geometry == nullptr)
    {
        return false;
    }

    const auto submeshIt = geometry->DrawArgs.find("mesh");
    if (submeshIt == geometry->DrawArgs.end())
    {
        return false;
    }

    BoundingOrientedBox localHitbox;
    BoundingOrientedBox::CreateFromBoundingBox(localHitbox, submeshIt->second.Bounds);

    // 무기 모델 축이 티어별로 달라도 가장 긴 축을 검 길이로 보고 조금 늘린다.
    // 나머지 두 축은 살짝만 줄여 두께가 과하게 넓어지지 않게 유지한다.
    const float extentX = localHitbox.Extents.x;
    const float extentY = localHitbox.Extents.y;
    const float extentZ = localHitbox.Extents.z;
    int longestAxis = 0;
    float longestExtent = extentX;
    if (extentY > longestExtent)
    {
        longestAxis = 1;
        longestExtent = extentY;
    }
    if (extentZ > longestExtent)
    {
        longestAxis = 2;
    }

    constexpr float kWarriorWeaponLengthScale = 1.18f;
    constexpr float kWarriorWeaponThicknessScale = 0.88f;
    switch (longestAxis)
    {
    case 0:
        localHitbox.Extents.x *= kWarriorWeaponLengthScale;
        localHitbox.Extents.y *= kWarriorWeaponThicknessScale;
        localHitbox.Extents.z *= kWarriorWeaponThicknessScale;
        break;
    case 1:
        localHitbox.Extents.x *= kWarriorWeaponThicknessScale;
        localHitbox.Extents.y *= kWarriorWeaponLengthScale;
        localHitbox.Extents.z *= kWarriorWeaponThicknessScale;
        break;
    case 2:
    default:
        localHitbox.Extents.x *= kWarriorWeaponThicknessScale;
        localHitbox.Extents.y *= kWarriorWeaponThicknessScale;
        localHitbox.Extents.z *= kWarriorWeaponLengthScale;
        break;
    }

    localHitbox.Transform(outHitbox, XMLoadFloat4x4(&weaponObject->World));
    return true;
}

int CombatSystem::ResolveHitMonsters(
    const PendingAttack& attack,
    const AttackProfile& profile,
    const XMFLOAT3& baseResolvedEffectCenter,
    const std::vector<Monster*>& hitMonsters,
    Monster** outFirstHitMonster)
{
    if (outFirstHitMonster != nullptr)
    {
        *outFirstHitMonster = hitMonsters.empty() ? nullptr : hitMonsters.front();
    }

    XMFLOAT3 resolvedEffectCenter = baseResolvedEffectCenter;
    if (attack.ClassType == PlayerClass::Warrior && attack.SkillType == 2 && !hitMonsters.empty())
    {
        Monster* effectTarget = attack.TargetMonster;
        if (!IsMonsterSelectable(effectTarget))
        {
            effectTarget = hitMonsters.front();
        }

        if (effectTarget != nullptr)
        {
            resolvedEffectCenter = effectTarget->GetPosition();
            resolvedEffectCenter.y -= effectTarget->GetColliderHalfHeight();
            resolvedEffectCenter.y += 0.02f;
        }
    }

    if (mSkillEffectManager != nullptr && attack.SkillType > 0)
    {
        mSkillEffectManager->OnSkillResolved(
            attack.ClassType,
            attack.SkillType,
            resolvedEffectCenter,
            attack.RotY,
            (std::max)(profile.range, profile.radius));
    }

    const bool shouldPlayLocalWarriorImpactSound =
        attack.ClassType == PlayerClass::Warrior &&
        attack.SkillType > 0 &&
        attack.SourcePlayer != nullptr &&
        mGame != nullptr &&
        attack.SourcePlayer == mGame->GetPlayer();
    if (shouldPlayLocalWarriorImpactSound)
    {
        if (attack.SkillType == 1)
        {
            AudioManager::Get().PlayEffect(kWarriorSkill1ImpactSound, kWarriorImpactVolume);
        }
        else if (attack.SkillType == 2)
        {
            AudioManager::Get().PlayEffect(kWarriorSkill2ImpactSound, kWarriorImpactVolume);
        }
    }

    const bool shouldPlayLocalMageImpactSound =
        attack.ClassType == PlayerClass::Mage &&
        attack.SkillType == 2 &&
        attack.SourcePlayer != nullptr &&
        mGame != nullptr &&
        attack.SourcePlayer == mGame->GetPlayer();
    if (shouldPlayLocalMageImpactSound)
    {
        AudioManager::Get().PlayEffect(kMageMeteorImpactSound, kMageMeteorImpactVolume);
    }

    const bool shouldRequestWarriorBasicHitStop =
        attack.ClassType == PlayerClass::Warrior &&
        attack.SkillType == 0 &&
        attack.SourcePlayer != nullptr;
    const bool shouldRequestWarriorSkill1Knockback =
        attack.ClassType == PlayerClass::Warrior &&
        attack.SkillType == 1 &&
        attack.SourcePlayer != nullptr;
    bool requestedAttackerHitStop = false;

    for (Monster* monster : hitMonsters)
    {
        const float damageMultiplier = attack.SourcePlayer != nullptr
            ? (std::max)(attack.SourcePlayer->GetOutgoingDamageMultiplier(), 0.0f)
            : 1.0f;
        const float appliedDamage = profile.damage * damageMultiplier;
        const XMFLOAT3 monsterPos = monster->GetPosition();
        XMFLOAT3 textPosition =
        {
            monsterPos.x,
            monsterPos.y + monster->GetColliderHalfHeight() * 0.45f,
            monsterPos.z
        };

        if (mBlockedHitCallback && mBlockedHitCallback(monster, textPosition))
        {
            continue;
        }

        monster->PlayDamageSound();

        if (shouldRequestWarriorBasicHitStop)
        {
            const bool isBasicAttack2 = attack.BasicAttackVariant == 2;
            const float attackerHitStopDuration = isBasicAttack2
                ? kWarriorBasicAttack2HitStopDuration
                : kWarriorBasicAttack1HitStopDuration;
            const float attackerHitStopTimeScale = isBasicAttack2
                ? kWarriorBasicAttack2HitStopTimeScale
                : kWarriorBasicAttack1HitStopTimeScale;
            const float victimHitStopDuration = isBasicAttack2
                ? kWarriorBasicAttack2VictimHitStopDuration
                : kWarriorBasicAttack1VictimHitStopDuration;
            const float victimHitStopTimeScale = isBasicAttack2
                ? kWarriorBasicAttack2VictimHitStopTimeScale
                : kWarriorBasicAttack1VictimHitStopTimeScale;
            const float victimHitStopDelay = isBasicAttack2
                ? kWarriorBasicAttack2VictimHitStopDelay
                : kWarriorBasicAttack1VictimHitStopDelay;
            const float victimKnockbackDistance = isBasicAttack2
                ? kWarriorBasicAttack2VictimKnockbackDistance
                : kWarriorBasicAttack1VictimKnockbackDistance;
            const float victimKnockbackDuration = isBasicAttack2
                ? kWarriorBasicAttack2VictimKnockbackDuration
                : kWarriorBasicAttack1VictimKnockbackDuration;

            if (!requestedAttackerHitStop)
            {
                attack.SourcePlayer->RequestAnimationHitStop(
                    attackerHitStopDuration,
                    attackerHitStopTimeScale);
                requestedAttackerHitStop = true;
            }

            XMFLOAT3 victimKnockbackDirection = { 0.0f, 0.0f, 0.0f };
            if (victimKnockbackDistance > 0.0f)
            {
                victimKnockbackDirection = GetVictimKnockbackDirection(
                    attack.SourcePlayer,
                    monsterPos,
                    attack.RotY);
            }

            monster->RequestDelayedDamageHitStop(
                victimHitStopDelay,
                victimHitStopDuration,
                victimHitStopTimeScale,
                victimKnockbackDirection,
                victimKnockbackDistance,
                victimKnockbackDuration);
        }
        else if (shouldRequestWarriorSkill1Knockback)
        {
            monster->RequestDamageKnockback(
                GetVictimKnockbackDirection(attack.SourcePlayer, monsterPos, attack.RotY),
                kWarriorSkill1VictimKnockbackDistance,
                kWarriorSkill1VictimKnockbackDuration);
        }

        if (mSkillEffectManager != nullptr && attack.SkillType > 0)
        {
            mSkillEffectManager->OnSkillImpact(attack.ClassType, attack.SkillType, textPosition);
        }

        const bool shouldApplyLocalDamage = !NetworkManager::Get()->IsConnected();
        if (shouldApplyLocalDamage)
        {
            const bool wasAlive =
                monster->GetState() != MonsterState::DIE &&
                monster->GetState() != MonsterState::DYING &&
                monster->GetHP() > 0.0f;
            monster->OnDamaged(appliedDamage);

            const bool isNowDead =
                monster->GetState() == MonsterState::DIE ||
                monster->GetState() == MonsterState::DYING;
            if (wasAlive &&
                isNowDead &&
                attack.SourcePlayer != nullptr)
            {
                ApplyMonsterKillReward(attack.SourcePlayer, monster->GetExperienceReward());
            }

            if (mDamageTextCallback)
            {
                mDamageTextCallback(textPosition, appliedDamage);
            }
        }
    }

    if (!hitMonsters.empty())
    {
        OutputDebugStringA("[CombatSystem] 몬스터 타격 성공\n");
    }

    return static_cast<int>(hitMonsters.size());
}

int CombatSystem::ApplyWarriorWeaponAttack(
    PendingAttack& attack,
    const std::vector<Monster*>& monsters,
    Monster** outFirstHitMonster)
{
    BoundingOrientedBox weaponHitbox;
    if (!TryGetWarriorWeaponHitbox(weaponHitbox))
    {
        const XMFLOAT3 attackForward =
        {
            std::sin(attack.RotY),
            0.0f,
            std::cos(attack.RotY)
        };
        return ApplyAttack(attack, attackForward, monsters, attack.Profile, outFirstHitMonster);
    }

    const XMFLOAT3 attackOrigin = attack.SourcePlayer != nullptr
        ? attack.SourcePlayer->GetPosition()
        : attack.Origin;
    Monster* preferredMonster = IsMonsterSelectable(attack.TargetMonster) ? attack.TargetMonster : nullptr;
    Monster* selectedHitMonster = nullptr;
    Monster* nearestHitMonster = nullptr;
    float nearestDistanceSq = FLT_MAX;
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

        BoundingBox hurtbox;
        hurtbox.Center = monster->GetPosition();
        hurtbox.Extents = monster->GetHurtboxExtents();
        if (!weaponHitbox.Intersects(hurtbox))
        {
            continue;
        }

        hitMonsters.push_back(monster);
        if (monster == preferredMonster)
        {
            selectedHitMonster = monster;
        }

        const XMFLOAT3 monsterPos = monster->GetPosition();
        const float dx = monsterPos.x - attackOrigin.x;
        const float dz = monsterPos.z - attackOrigin.z;
        const float distanceSq = dx * dx + dz * dz;
        if (distanceSq < nearestDistanceSq)
        {
            nearestDistanceSq = distanceSq;
            nearestHitMonster = monster;
        }
    }

    if (selectedHitMonster != nullptr && !hitMonsters.empty() && hitMonsters.front() != selectedHitMonster)
    {
        auto selectedIt = std::find(hitMonsters.begin(), hitMonsters.end(), selectedHitMonster);
        if (selectedIt != hitMonsters.end())
        {
            std::iter_swap(hitMonsters.begin(), selectedIt);
        }
    }
    else if (selectedHitMonster == nullptr &&
        nearestHitMonster != nullptr &&
        !hitMonsters.empty() &&
        hitMonsters.front() != nearestHitMonster)
    {
        auto nearestIt = std::find(hitMonsters.begin(), hitMonsters.end(), nearestHitMonster);
        if (nearestIt != hitMonsters.end())
        {
            std::iter_swap(hitMonsters.begin(), nearestIt);
        }
    }

    attack.Origin = weaponHitbox.Center;
    return ResolveHitMonsters(attack, attack.Profile, attack.Origin, hitMonsters, outFirstHitMonster);
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
    const PendingAttack& attack,
    const XMFLOAT3& attackForward,
    const std::vector<Monster*>& monsters,
    const AttackProfile& profile,
    Monster** outFirstHitMonster)
{
    if (outFirstHitMonster != nullptr)
    {
        *outFirstHitMonster = nullptr;
    }

    const XMFLOAT3& attackOrigin = attack.Origin;
    Monster* closestMonster = nullptr;
    float closestDistanceSq = FLT_MAX;
    std::vector<Monster*> hitMonsters;
    Monster* targetMonster = IsMonsterSelectable(attack.TargetMonster) ? attack.TargetMonster : nullptr;

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
        const XMFLOAT3 hurtboxExtents = monster->GetHurtboxExtents();
        const float dx = monsterPos.x - attackOrigin.x;
        const float dz = monsterPos.z - attackOrigin.z;
        const float distanceSq = (dx * dx) + (dz * dz);
        const float distance = std::sqrt(distanceSq);
        const float projected = (dx * attackForward.x) + (dz * attackForward.z);
        const XMFLOAT3 sideAxis = { -attackForward.z, 0.0f, attackForward.x };
        const float sideProjected = (dx * sideAxis.x) + (dz * sideAxis.z);
        const bool isRadialAttack = profile.coneDot <= -0.999f;

        if (isRadialAttack)
        {
            const float horizontalRadius = (std::max)(hurtboxExtents.x, hurtboxExtents.z);
            const float maxHitDistance = profile.range + horizontalRadius;
            if (distanceSq > (maxHitDistance * maxHitDistance))
            {
                continue;
            }
        }
        else
        {
            const float forwardHalfExtent =
                GetProjectedHalfExtent2D(hurtboxExtents, attackForward.x, attackForward.z);
            const float sideHalfExtent =
                GetProjectedHalfExtent2D(hurtboxExtents, sideAxis.x, sideAxis.z);

            if ((projected + forwardHalfExtent) < 0.0f)
            {
                continue;
            }

            if ((projected - forwardHalfExtent) > profile.range)
            {
                continue;
            }

            const float sideDistance = (std::max)(0.0f, std::abs(sideProjected) - sideHalfExtent);
            if ((sideDistance * sideDistance) > (profile.radius * profile.radius))
            {
                continue;
            }

            const float dirX = (distance > 0.001f) ? (dx / distance) : 0.0f;
            const float dirZ = (distance > 0.001f) ? (dz / distance) : 1.0f;
            const float dot = (dirX * attackForward.x) + (dirZ * attackForward.z);
            const float baseHalfAngle = std::acos((std::clamp)(profile.coneDot, -1.0f, 1.0f));
            const float horizontalRadius = (std::max)(hurtboxExtents.x, hurtboxExtents.z);
            const float safeDistance = (std::max)(distance, horizontalRadius + 0.001f);
            const float angularPadding =
                std::asin((std::clamp)(horizontalRadius / safeDistance, 0.0f, 1.0f));
            const float effectiveConeDot =
                std::cos((std::min)(XM_PI, baseHalfAngle + angularPadding));

            if (dot < effectiveConeDot)
            {
                continue;
            }
        }

        if (profile.hitAll)
        {
            hitMonsters.push_back(monster);
        }
        else if (targetMonster != nullptr && monster == targetMonster)
        {
            closestMonster = monster;
            closestDistanceSq = -1.0f;
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

    return ResolveHitMonsters(attack, profile, attackOrigin, hitMonsters, outFirstHitMonster);
}

void CombatSystem::HandleDebugHitboxToggle()
{
    mDebugHitboxTogglePressed = false;
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
