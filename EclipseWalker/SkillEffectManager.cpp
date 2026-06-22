#include "SkillEffectManager.h"

#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Material.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <memory>

using namespace DirectX;

namespace
{
    constexpr int kBurstPoolSize = 36;
    constexpr int kGroundPoolSize = 24;
    constexpr int kBeamPoolSize = 18;
    constexpr int kSummonedSwordPoolSize = 6;
    constexpr float kSummonedSwordSpawnHeight = 6.40f;
    constexpr float kSummonedSwordLifeTime = 1.45f;
    constexpr float kSummonedSwordMotionDuration = 0.24f;
    constexpr float kSummonedSwordFadeStartTime = 1.05f;
    constexpr float kSummonedSwordEmbedDepth = 0.04f;
    constexpr float kSummonedSwordFlipYRadians = XM_PI;
    constexpr float kSummonedSwordVisualScale = 2.45f;
    constexpr float kSummonedSwordWidthScaleMultiplier = 1.45f;
    constexpr float kSummonedSwordPostImpactLife = 0.95f;

    XMFLOAT3 ForwardFromYaw(float rotY)
    {
        return { std::sin(rotY), 0.0f, std::cos(rotY) };
    }

    XMFLOAT3 AddScaled(const XMFLOAT3& origin, const XMFLOAT3& direction, float scale)
    {
        return {
            origin.x + direction.x * scale,
            origin.y + direction.y * scale,
            origin.z + direction.z * scale
        };
    }

    XMFLOAT3 Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    XMFLOAT4 Lerp4(const XMFLOAT4& a, const XMFLOAT4& b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        };
    }

    XMFLOAT4 GetSkillColor(PlayerClass playerClass, int skillIndex)
    {
        switch (playerClass)
        {
        case PlayerClass::Warrior:
            return (skillIndex == 2)
                ? XMFLOAT4(1.0f, 0.24f, 0.08f, 0.74f)
                : XMFLOAT4(1.0f, 0.54f, 0.16f, 0.66f);

        case PlayerClass::Mage:
            return (skillIndex == 2)
                ? XMFLOAT4(0.66f, 0.72f, 1.0f, 0.72f)
                : XMFLOAT4(0.40f, 1.0f, 0.92f, 0.62f);

        case PlayerClass::Archer:
            return (skillIndex == 2)
                ? XMFLOAT4(0.82f, 1.0f, 0.54f, 0.68f)
                : XMFLOAT4(0.48f, 1.0f, 0.64f, 0.60f);

        case PlayerClass::None:
        default:
            return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.55f);
        }
    }

    XMFLOAT4 FadeColor(const XMFLOAT4& color, float alphaScale)
    {
        return { color.x, color.y, color.z, color.w * alphaScale };
    }

    float EaseOutQuart(float t)
    {
        t = (std::clamp)(t, 0.0f, 1.0f);
        const float inv = 1.0f - t;
        return 1.0f - inv * inv * inv * inv;
    }

    XMMATRIX RotationFromTo(FXMVECTOR from, FXMVECTOR to)
    {
        const XMVECTOR fromNorm = XMVector3Normalize(from);
        const XMVECTOR toNorm = XMVector3Normalize(to);
        float dot = XMVectorGetX(XMVector3Dot(fromNorm, toNorm));
        dot = (std::clamp)(dot, -1.0f, 1.0f);

        if (dot > 0.9999f)
        {
            return XMMatrixIdentity();
        }

        if (dot < -0.9999f)
        {
            XMVECTOR axis = XMVector3Cross(fromNorm, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
            if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f)
            {
                axis = XMVector3Cross(fromNorm, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
            }

            axis = XMVector3Normalize(axis);
            return XMMatrixRotationAxis(axis, XM_PI);
        }

        const XMVECTOR axis = XMVector3Normalize(XMVector3Cross(fromNorm, toNorm));
        return XMMatrixRotationAxis(axis, std::acos(dot));
    }

    void BuildSummonedSwordPlacement(
        const SubmeshGeometry& submesh,
        XMFLOAT3& outTipAxisLocal,
        XMFLOAT3& outAnchorLocal,
        XMFLOAT3& outScaleMultiplier)
    {
        const XMFLOAT3 center = submesh.Bounds.Center;
        const XMFLOAT3 extents = submesh.Bounds.Extents;
        const float axisExtents[3] = { extents.x, extents.y, extents.z };

        int dominantAxis = 0;
        if (axisExtents[1] > axisExtents[dominantAxis])
        {
            dominantAxis = 1;
        }
        if (axisExtents[2] > axisExtents[dominantAxis])
        {
            dominantAxis = 2;
        }

        const float mins[3] =
        {
            center.x - extents.x,
            center.y - extents.y,
            center.z - extents.z
        };
        const float maxs[3] =
        {
            center.x + extents.x,
            center.y + extents.y,
            center.z + extents.z
        };

        const float tipCoord = mins[dominantAxis];
        const float tipSign = tipCoord >= 0.0f ? 1.0f : -1.0f;

        outTipAxisLocal = { 0.0f, 0.0f, 0.0f };
        outAnchorLocal = center;
        outScaleMultiplier = { 1.0f, 1.0f, 1.0f };

        int widthAxis = -1;
        float maxCrossExtent = -1.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (axis == dominantAxis)
            {
                continue;
            }

            if (axisExtents[axis] > maxCrossExtent)
            {
                maxCrossExtent = axisExtents[axis];
                widthAxis = axis;
            }
        }

        switch (widthAxis)
        {
        case 0:
            outScaleMultiplier.x = kSummonedSwordWidthScaleMultiplier;
            break;

        case 1:
            outScaleMultiplier.y = kSummonedSwordWidthScaleMultiplier;
            break;

        case 2:
            outScaleMultiplier.z = kSummonedSwordWidthScaleMultiplier;
            break;

        default:
            break;
        }

        switch (dominantAxis)
        {
        case 0:
            outTipAxisLocal.x = tipSign;
            outAnchorLocal.x = tipCoord;
            break;

        case 1:
            outTipAxisLocal.y = tipSign;
            outAnchorLocal.y = tipCoord;
            break;

        default:
            outTipAxisLocal.z = tipSign;
            outAnchorLocal.z = tipCoord;
            break;
        }
    }

    XMMATRIX BuildSummonedSwordRotation(const XMFLOAT3& tipAxisLocal, float rotY)
    {
        const XMVECTOR localTipAxis = XMLoadFloat3(&tipAxisLocal);
        const XMMATRIX flipAroundY = XMMatrixRotationY(kSummonedSwordFlipYRadians);
        const XMVECTOR flippedTipAxis = XMVector3TransformNormal(localTipAxis, flipAroundY);
        const XMMATRIX alignToDown = RotationFromTo(flippedTipAxis, XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
        return flipAroundY * alignToDown * XMMatrixRotationY(rotY);
    }
}

void SkillEffectManager::Initialize(EclipseWalkerGame* game, const TrackOwnedCallback& trackOwned)
{
    mGame = game;
    mTrackOwned = trackOwned;

    EnsureResources();
    EnsurePool();
    Reset();
}

void SkillEffectManager::Reset()
{
    for (auto& effect : mEffects)
    {
        DeactivateEffect(effect);
    }
}

void SkillEffectManager::Update(float dt)
{
    for (auto& effect : mEffects)
    {
        if (!effect.Active || effect.Object == nullptr || effect.Ritem == nullptr)
        {
            continue;
        }

        effect.Age += dt;
        if (effect.Age >= effect.LifeTime)
        {
            DeactivateEffect(effect);
            continue;
        }

        const float t = (std::clamp)(effect.Age / (std::max)(effect.LifeTime, 0.0001f), 0.0f, 1.0f);
        const float eased = 1.0f - (1.0f - t) * (1.0f - t);
        const XMFLOAT3 currentScale = Lerp3(effect.StartScale, effect.EndScale, eased);
        XMFLOAT4 currentColor = Lerp4(effect.StartColor, effect.EndColor, t);
        XMFLOAT3 currentPosition =
        {
            effect.BasePosition.x + effect.Velocity.x * effect.Age,
            effect.BasePosition.y + effect.Velocity.y * effect.Age,
            effect.BasePosition.z + effect.Velocity.z * effect.Age
        };

        if (effect.Style == EffectStyle::SummonedSword)
        {
            if (effect.Age < effect.StartDelay)
            {
                effect.Ritem->Visible = false;
                effect.Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
                effect.Ritem->NumFramesDirty = gNumFrameResources;
                continue;
            }

            const float swordAge = effect.Age - effect.StartDelay;
            const float fallT = effect.MotionDuration > 0.0f
                ? (std::clamp)(swordAge / effect.MotionDuration, 0.0f, 1.0f)
                : 1.0f;
            currentPosition = Lerp3(effect.BasePosition, effect.TargetPosition, EaseOutQuart(fallT));

            if (effect.Age >= effect.FadeStartTime)
            {
                const float fadeDuration = (std::max)(effect.LifeTime - effect.FadeStartTime, 0.0001f);
                const float fadeT = (std::clamp)((effect.Age - effect.FadeStartTime) / fadeDuration, 0.0f, 1.0f);
                currentColor = effect.StartColor;
                currentColor.w = 1.0f - fadeT;
            }
            else
            {
                currentColor = effect.StartColor;
            }

            const XMMATRIX anchorOffset = XMMatrixTranslation(
                -effect.AnchorLocalPoint.x,
                -effect.AnchorLocalPoint.y,
                -effect.AnchorLocalPoint.z);
            const XMMATRIX scaleMatrix = XMMatrixScaling(currentScale.x, currentScale.y, currentScale.z);
            const XMMATRIX rotationMatrix = XMLoadFloat4x4(&effect.RotationMatrix);
            const XMMATRIX translationMatrix = XMMatrixTranslation(
                currentPosition.x,
                currentPosition.y,
                currentPosition.z);

            effect.Object->SetWorldTransform(anchorOffset * scaleMatrix * rotationMatrix * translationMatrix);
            effect.Ritem->ColorMultiplier = currentColor;
            effect.Ritem->Visible = currentColor.w > 0.001f;
            effect.Ritem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        effect.Object->SetPosition(currentPosition.x, currentPosition.y, currentPosition.z);
        effect.Object->SetScale(currentScale.x, currentScale.y, currentScale.z);
        if (!effect.Object->mIsBillboard)
        {
            effect.Object->SetRotation(effect.RotX, effect.RotY, effect.RotZ);
        }
        effect.Object->Update();

        effect.Ritem->ColorMultiplier = currentColor;
        effect.Ritem->Visible = currentColor.w > 0.001f;
        effect.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::OnSkillCast(PlayerClass playerClass, int skillIndex, const XMFLOAT3& origin, float rotY)
{
    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT4 skillColor = GetSkillColor(playerClass, skillIndex);
    const XMFLOAT4 fadeColor = FadeColor(skillColor, 0.0f);

    switch (playerClass)
    {
    case PlayerClass::Warrior:
        if (skillIndex == 1)
        {
            SpawnBurst({ origin.x, origin.y + 0.90f, origin.z }, 0.24f, 0.78f, 0.22f, 0.85f, skillColor, fadeColor);
        }
        break;

    case PlayerClass::Mage:
        if (skillIndex == 1)
        {
            SpawnGroundDecal({ origin.x, origin.y + 0.05f, origin.z }, rotY, 0.28f, 0.96f, 0.45f, skillColor, fadeColor);
            SpawnBurst({ origin.x, origin.y + 1.15f, origin.z }, 0.30f, 0.74f, 0.30f, 0.80f, skillColor, fadeColor);
        }
        else if (skillIndex == 2)
        {
            const XMFLOAT3 target = AddScaled(origin, forward, 2.0f);
            SpawnGroundDecal({ target.x, origin.y + 0.05f, target.z }, rotY, 0.40f, 1.60f, 0.58f, skillColor, fadeColor);
            SpawnBurst({ target.x, origin.y + 1.45f, target.z }, 0.42f, 1.18f, 0.34f, 1.20f, skillColor, fadeColor);
        }
        break;

    case PlayerClass::Archer:
        if (skillIndex == 1)
        {
            const XMFLOAT3 front = AddScaled(origin, forward, 1.15f);
            SpawnBurst({ front.x, origin.y + 1.05f, front.z }, 0.18f, 0.58f, 0.18f, 0.18f, skillColor, fadeColor);
            SpawnGroundDecal({ front.x, origin.y + 0.03f, front.z }, rotY, 0.18f, 0.52f, 0.18f, skillColor, fadeColor);
        }
        else if (skillIndex == 2)
        {
            for (float distance : { 1.6f, 2.5f, 3.4f })
            {
                const XMFLOAT3 point = AddScaled(origin, forward, distance);
                SpawnBurst({ point.x, origin.y + 1.10f, point.z }, 0.16f, 0.50f, 0.18f, 0.10f, skillColor, fadeColor);
            }

            const XMFLOAT3 farPoint = AddScaled(origin, forward, 3.2f);
            SpawnGroundDecal({ farPoint.x, origin.y + 0.03f, farPoint.z }, rotY, 0.26f, 0.86f, 0.24f, skillColor, fadeColor);
        }
        break;

    case PlayerClass::None:
    default:
        break;
    }
}

void SkillEffectManager::OnSkillImpact(PlayerClass playerClass, int skillIndex, const XMFLOAT3& hitPosition)
{
    const XMFLOAT4 skillColor = GetSkillColor(playerClass, skillIndex);
    const XMFLOAT4 fadeColor = FadeColor(skillColor, 0.0f);

    switch (playerClass)
    {
    case PlayerClass::Warrior:
        if (skillIndex == 2)
        {
            return;
        }
        SpawnBurst(hitPosition, 0.30f, 0.94f, 0.24f, 0.55f, skillColor, fadeColor);
        break;

    case PlayerClass::Mage:
        SpawnBurst(hitPosition, 0.36f, 1.08f, 0.30f, 0.85f, skillColor, fadeColor);
        break;

    case PlayerClass::Archer:
        SpawnBurst(hitPosition, 0.22f, 0.68f, 0.20f, 0.25f, skillColor, fadeColor);
        break;

    case PlayerClass::None:
    default:
        SpawnBurst(hitPosition, 0.20f, 0.54f, 0.18f, 0.20f, skillColor, fadeColor);
        break;
    }
}

void SkillEffectManager::PreviewWarriorSwordStrike(
    const XMFLOAT3& targetPosition,
    float rotY,
    float effectRadius,
    float impactDelay,
    float swordSpawnDelay)
{
    const float clampedDelay = (std::max)(impactDelay, 0.12f);
    const float clampedSwordSpawnDelay = (std::clamp)(
        swordSpawnDelay,
        0.0f,
        (std::max)(clampedDelay - 0.05f, 0.0f));
    const float swordMotionDuration = (std::max)(clampedDelay - clampedSwordSpawnDelay, 0.05f);
    const float telegraphScale = (std::max)(effectRadius * 1.55f, 1.55f);
    const float telegraphLife = clampedDelay + 0.08f;
    const XMFLOAT4 outerColor = { 0.20f, 0.58f, 1.0f, 0.98f };
    const XMFLOAT4 outerFade = { 0.12f, 0.42f, 1.0f, 0.70f };
    const XMFLOAT4 innerColor = { 0.82f, 0.94f, 1.0f, 0.92f };
    const XMFLOAT4 innerFade = { 0.44f, 0.72f, 1.0f, 0.56f };
    const XMFLOAT4 beamColor = { 0.34f, 0.72f, 1.0f, 0.60f };
    const XMFLOAT4 beamFade = { 0.12f, 0.34f, 1.0f, 0.10f };
    const XMFLOAT4 beamCoreColor = { 0.88f, 0.96f, 1.0f, 0.42f };
    const XMFLOAT4 beamCoreFade = { 0.50f, 0.78f, 1.0f, 0.04f };

    SpawnGroundDecal(
        { targetPosition.x, targetPosition.y + 0.03f, targetPosition.z },
        rotY,
        telegraphScale * 0.92f,
        telegraphScale * 1.02f,
        telegraphLife,
        outerColor,
        outerFade);
    SpawnGroundDecal(
        { targetPosition.x, targetPosition.y + 0.04f, targetPosition.z },
        rotY + XM_PIDIV4,
        telegraphScale * 0.62f,
        telegraphScale * 0.72f,
        telegraphLife,
        innerColor,
        innerFade);
    SpawnVerticalBeam(
        targetPosition,
        rotY,
        0.86f,
        4.30f,
        telegraphLife,
        beamColor,
        beamFade);
    SpawnVerticalBeam(
        targetPosition,
        rotY + XM_PIDIV2,
        0.86f,
        4.30f,
        telegraphLife,
        beamColor,
        beamFade);
    SpawnVerticalBeam(
        targetPosition,
        rotY + XM_PIDIV4,
        0.42f,
        4.65f,
        telegraphLife,
        beamCoreColor,
        beamCoreFade);
    SpawnVerticalBeam(
        targetPosition,
        rotY - XM_PIDIV4,
        0.42f,
        4.65f,
        telegraphLife,
        beamCoreColor,
        beamCoreFade);

    SpawnSummonedSword(
        targetPosition,
        rotY,
        kSummonedSwordVisualScale,
        kSummonedSwordSpawnHeight,
        clampedDelay + kSummonedSwordPostImpactLife,
        swordMotionDuration,
        clampedSwordSpawnDelay);
}

void SkillEffectManager::OnSkillResolved(PlayerClass playerClass, int skillIndex, const XMFLOAT3& impactCenter, float rotY, float effectRadius)
{
    if (playerClass == PlayerClass::Warrior && skillIndex == 1)
    {
        const float decalScale = (std::max)(effectRadius, 0.1f);
        const XMFLOAT4 crackColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        const XMFLOAT3 decalPosition =
        {
            impactCenter.x,
            impactCenter.y - Player::DefaultColliderHalfHeight + 0.03f,
            impactCenter.z
        };

        SpawnGroundDecal(
            decalPosition,
            rotY,
            decalScale,
            decalScale,
            0.55f,
            crackColor,
            crackColor,
            mEarthshatterDecalMaterial);
    }
    else if (playerClass == PlayerClass::Warrior && skillIndex == 2)
    {
        const float ringScale = (std::max)(effectRadius, 0.9f);
        const XMFLOAT4 ringColor = { 1.0f, 0.62f, 0.20f, 1.0f };
        const XMFLOAT4 ringFade = { 1.0f, 0.28f, 0.08f, 0.18f };

        SpawnGroundDecal(
            { impactCenter.x, impactCenter.y + 0.05f, impactCenter.z },
            rotY,
            ringScale * 0.52f,
            ringScale * 1.72f,
            0.30f,
            ringColor,
            ringFade);
    }
}

void SkillEffectManager::EnsureResources()
{
    auto* resources = (mGame != nullptr) ? mGame->GetResources() : nullptr;
    if (resources == nullptr)
    {
        return;
    }

    if (resources->GetMaterial("SkillFx_BurstMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_BurstMat",
            static_cast<int>(resources->mMaterials.size()),
            resources->GetTexture("Fire_1") != nullptr ? "Fire_1" : "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 0.92f),
            XMFLOAT3(0.04f, 0.04f, 0.04f),
            0.08f);
    }

    if (resources->GetMaterial("SkillFx_DecalMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_DecalMat",
            static_cast<int>(resources->mMaterials.size()),
            resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 0.82f),
            XMFLOAT3(0.04f, 0.04f, 0.04f),
            0.18f);
    }

    if (resources->GetMaterial("SkillFx_EarthshatterCrackMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_EarthshatterCrackMat",
            static_cast<int>(resources->mMaterials.size()),
            resources->GetTexture("Skill_Warrior_EarthquakeCrack") != nullptr ? "Skill_Warrior_EarthquakeCrack" :
            (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white"),
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 0.92f),
            XMFLOAT3(0.04f, 0.04f, 0.04f),
            0.36f);
    }

    if (resources->GetMaterial("SkillFx_BeamMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_BeamMat",
            static_cast<int>(resources->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 0.74f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            0.06f);
    }

    mBurstMaterial = resources->GetMaterial("SkillFx_BurstMat");
    if (mBurstMaterial != nullptr)
    {
        mBurstMaterial->IsTransparent = 1;
        mBurstMaterial->IsToon = 0;
        mBurstMaterial->OutlineThickness = 0.0f;
        mBurstMaterial->NumFramesDirty = gNumFrameResources;
    }

    mDecalMaterial = resources->GetMaterial("SkillFx_DecalMat");
    if (mDecalMaterial != nullptr)
    {
        mDecalMaterial->IsTransparent = 1;
        mDecalMaterial->IsToon = 0;
        mDecalMaterial->OutlineThickness = 0.0f;
        mDecalMaterial->NumFramesDirty = gNumFrameResources;
    }

    mEarthshatterDecalMaterial = resources->GetMaterial("SkillFx_EarthshatterCrackMat");
    if (mEarthshatterDecalMaterial != nullptr)
    {
        mEarthshatterDecalMaterial->IsTransparent = 1;
        mEarthshatterDecalMaterial->IsToon = 0;
        mEarthshatterDecalMaterial->OutlineThickness = 0.0f;
        mEarthshatterDecalMaterial->NumFramesDirty = gNumFrameResources;
    }

    mBeamMaterial = resources->GetMaterial("SkillFx_BeamMat");
    if (mBeamMaterial != nullptr)
    {
        mBeamMaterial->IsTransparent = 1;
        mBeamMaterial->IsToon = 0;
        mBeamMaterial->OutlineThickness = 0.0f;
        mBeamMaterial->NumFramesDirty = gNumFrameResources;
    }

    mSummonedSwordMaterial = resources->GetMaterial("PlayerSwordMat");
}

void SkillEffectManager::EnsurePool()
{
    if (!mEffects.empty() || mGame == nullptr || mGame->GetResources() == nullptr)
    {
        return;
    }

    auto* resources = mGame->GetResources();
    auto geoIt = resources->mGeometries.find("quadGeo");
    if (geoIt == resources->mGeometries.end() || geoIt->second == nullptr)
    {
        OutputDebugStringA("[SkillEffectManager] quadGeo missing, skipping pool creation\n");
        return;
    }

    const auto& drawArgs = geoIt->second->DrawArgs["quad"];
    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    auto CreateEffect = [&](EffectStyle style)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Geo = geoIt->second.get();
        if (style == EffectStyle::BillboardBurst)
        {
            renderItem->Mat = mBurstMaterial;
        }
        else if (style == EffectStyle::VerticalBeam)
        {
            renderItem->Mat = mBeamMaterial;
        }
        else
        {
            renderItem->Mat = mDecalMaterial;
        }
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->IndexCount = drawArgs.IndexCount;
        renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
        renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        renderItem->Visible = false;
        renderItem->CastShadow = false;
        renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->mIsBillboard = style == EffectStyle::BillboardBurst;
        object->mIsAnimated = false;
        object->SetScale(0.0f, 0.0f, 1.0f);
        object->SetPosition(0.0f, -1000.0f, 0.0f);
        if (style == EffectStyle::GroundDecal)
        {
            object->SetRotation(XM_PIDIV2, 0.0f, 0.0f);
        }
        else
        {
            object->SetRotation(0.0f, 0.0f, 0.0f);
        }
        object->Update();

        EffectInstance instance;
        instance.Object = object.get();
        instance.Ritem = renderItem.get();
        instance.Style = style;
        mEffects.push_back(instance);

        if (mTrackOwned)
        {
            mTrackOwned(object.get(), renderItem.get());
        }

        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    };

    for (int i = 0; i < kBurstPoolSize; ++i)
    {
        CreateEffect(EffectStyle::BillboardBurst);
    }

    for (int i = 0; i < kGroundPoolSize; ++i)
    {
        CreateEffect(EffectStyle::GroundDecal);
    }

    for (int i = 0; i < kBeamPoolSize; ++i)
    {
        CreateEffect(EffectStyle::VerticalBeam);
    }
}

void SkillEffectManager::EnsureSummonedSwordPool()
{
    if (mGame == nullptr || mGame->GetResources() == nullptr)
    {
        return;
    }

    const bool alreadyCreated = std::any_of(
        mEffects.begin(),
        mEffects.end(),
        [](const EffectInstance& effect)
        {
            return effect.Style == EffectStyle::SummonedSword;
        });
    if (alreadyCreated)
    {
        return;
    }

    auto* swordObject = mGame->GetPlayerWeaponObject();
    auto* swordRitem = swordObject != nullptr ? swordObject->Ritem : nullptr;
    MeshGeometry* geometry = swordRitem != nullptr ? swordRitem->Geo : nullptr;
    Material* material = swordRitem != nullptr ? swordRitem->Mat : mSummonedSwordMaterial;
    if (geometry == nullptr)
    {
        auto* resources = mGame->GetResources();
        auto geoIt = resources->mGeometries.find("warriorLv3SwordGeo");
        if (geoIt != resources->mGeometries.end())
        {
            geometry = geoIt->second.get();
        }
    }

    if (geometry == nullptr || material == nullptr)
    {
        return;
    }

    auto submeshIt = geometry->DrawArgs.find("mesh");
    if (submeshIt == geometry->DrawArgs.end())
    {
        return;
    }

    BuildSummonedSwordPlacement(
        submeshIt->second,
        mSummonedSwordTipAxisLocal,
        mSummonedSwordAnchorLocal,
        mSummonedSwordScaleMultiplier);

    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    for (int i = 0; i < kSummonedSwordPoolSize; ++i)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Geo = geometry;
        renderItem->Mat = material;
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->IndexCount = submeshIt->second.IndexCount;
        renderItem->StartIndexLocation = submeshIt->second.StartIndexLocation;
        renderItem->BaseVertexLocation = submeshIt->second.BaseVertexLocation;
        renderItem->Visible = false;
        renderItem->CastShadow = false;
        renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->mIsBillboard = false;
        object->mIsAnimated = false;
        object->SetScale(0.0f, 0.0f, 0.0f);
        object->SetPosition(0.0f, -1000.0f, 0.0f);
        object->Update();

        EffectInstance instance;
        instance.Object = object.get();
        instance.Ritem = renderItem.get();
        instance.Style = EffectStyle::SummonedSword;
        mEffects.push_back(instance);

        if (mTrackOwned)
        {
            mTrackOwned(object.get(), renderItem.get());
        }

        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    }
}

SkillEffectManager::EffectInstance* SkillEffectManager::AcquireEffect(EffectStyle style)
{
    auto it = std::find_if(
        mEffects.begin(),
        mEffects.end(),
        [style](const EffectInstance& effect)
        {
            return effect.Style == style && !effect.Active;
        });

    if (it != mEffects.end())
    {
        return &(*it);
    }

    auto fallback = std::find_if(
        mEffects.begin(),
        mEffects.end(),
        [style](const EffectInstance& effect)
        {
            return effect.Style == style;
        });

    return (fallback != mEffects.end()) ? &(*fallback) : nullptr;
}

void SkillEffectManager::DeactivateEffect(EffectInstance& effect)
{
    effect.Active = false;
    effect.Age = 0.0f;
    effect.LifeTime = 0.0f;
    effect.Velocity = { 0.0f, 0.0f, 0.0f };
    effect.StartDelay = 0.0f;
    effect.MotionDuration = 0.0f;
    effect.FadeStartTime = 0.0f;

    if (effect.Object != nullptr)
    {
        effect.Object->mIsAnimated = false;
        effect.Object->ClearWorldTransformOverride();
    }

    if (effect.Ritem != nullptr)
    {
        effect.Ritem->Visible = false;
        effect.Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
        effect.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::SpawnBurst(
    const XMFLOAT3& position,
    float startScale,
    float endScale,
    float lifeTime,
    float riseSpeed,
    const XMFLOAT4& startColor,
    const XMFLOAT4& endColor)
{
    EffectInstance* effect = AcquireEffect(EffectStyle::BillboardBurst);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    effect->Style = EffectStyle::BillboardBurst;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = lifeTime;
    effect->BasePosition = position;
    effect->Velocity = { 0.0f, riseSpeed, 0.0f };
    effect->StartScale = { startScale, startScale, 1.0f };
    effect->EndScale = { endScale, endScale, 1.0f };
    effect->StartColor = startColor;
    effect->EndColor = endColor;
    effect->RotX = 0.0f;
    effect->RotY = 0.0f;
    effect->RotZ = 0.0f;

    effect->Object->mIsBillboard = true;
    effect->Object->mIsAnimated = true;
    effect->Object->mAnimTime = 0.0f;
    effect->Object->mFrameDuration = 0.06f;
    effect->Object->mCurrFrame = 0;
    effect->Object->mNumCols = 2;
    effect->Object->mNumRows = 2;
    effect->Object->SetPosition(position.x, position.y, position.z);
    effect->Object->SetScale(startScale, startScale, 1.0f);
    effect->Object->SetRotation(0.0f, 0.0f, 0.0f);
    effect->Object->Update();

    effect->Ritem->Mat = mBurstMaterial;
    effect->Ritem->Visible = true;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = startColor;
    effect->Ritem->NumFramesDirty = gNumFrameResources;
}

void SkillEffectManager::SpawnGroundDecal(
    const XMFLOAT3& position,
    float rotY,
    float startScale,
    float endScale,
    float lifeTime,
    const XMFLOAT4& startColor,
    const XMFLOAT4& endColor,
    Material* materialOverride)
{
    EffectInstance* effect = AcquireEffect(EffectStyle::GroundDecal);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    effect->Style = EffectStyle::GroundDecal;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = lifeTime;
    effect->BasePosition = position;
    effect->Velocity = { 0.0f, 0.0f, 0.0f };
    effect->StartScale = { startScale, startScale, 1.0f };
    effect->EndScale = { endScale, endScale, 1.0f };
    effect->StartColor = startColor;
    effect->EndColor = endColor;
    effect->RotX = XM_PIDIV2;
    effect->RotY = rotY;
    effect->RotZ = 0.0f;

    effect->Object->mIsBillboard = false;
    effect->Object->mIsAnimated = false;
    effect->Object->SetPosition(position.x, position.y, position.z);
    effect->Object->SetScale(startScale, startScale, 1.0f);
    effect->Object->SetRotation(effect->RotX, effect->RotY, effect->RotZ);
    effect->Object->Update();

    effect->Ritem->Mat = materialOverride != nullptr ? materialOverride : mDecalMaterial;
    effect->Ritem->Visible = true;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = startColor;
    effect->Ritem->NumFramesDirty = gNumFrameResources;
}

void SkillEffectManager::SpawnVerticalBeam(
    const XMFLOAT3& position,
    float rotY,
    float width,
    float height,
    float lifeTime,
    const XMFLOAT4& startColor,
    const XMFLOAT4& endColor)
{
    EffectInstance* effect = AcquireEffect(EffectStyle::VerticalBeam);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    const XMFLOAT3 beamCenter =
    {
        position.x,
        position.y + height * 0.50f,
        position.z
    };

    effect->Style = EffectStyle::VerticalBeam;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = lifeTime;
    effect->BasePosition = beamCenter;
    effect->Velocity = { 0.0f, 0.0f, 0.0f };
    effect->StartScale = { width * 0.92f, height, 1.0f };
    effect->EndScale = { width, height, 1.0f };
    effect->StartColor = startColor;
    effect->EndColor = endColor;
    effect->RotX = 0.0f;
    effect->RotY = rotY;
    effect->RotZ = 0.0f;

    effect->Object->mIsBillboard = false;
    effect->Object->mIsAnimated = false;
    effect->Object->SetPosition(beamCenter.x, beamCenter.y, beamCenter.z);
    effect->Object->SetScale(effect->StartScale.x, effect->StartScale.y, 1.0f);
    effect->Object->SetRotation(0.0f, rotY, 0.0f);
    effect->Object->Update();

    effect->Ritem->Mat = mBeamMaterial != nullptr ? mBeamMaterial : mDecalMaterial;
    effect->Ritem->Visible = true;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = startColor;
    effect->Ritem->NumFramesDirty = gNumFrameResources;
}

void SkillEffectManager::SpawnSummonedSword(
    const XMFLOAT3& targetPosition,
    float rotY,
    float uniformScale,
    float spawnHeight,
    float lifeTime,
    float motionDuration,
    float startDelay)
{
    EnsureSummonedSwordPool();

    EffectInstance* effect = AcquireEffect(EffectStyle::SummonedSword);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    const XMFLOAT3 startPosition =
    {
        targetPosition.x,
        targetPosition.y + spawnHeight,
        targetPosition.z
    };
    const XMFLOAT3 plantedPosition =
    {
        targetPosition.x,
        targetPosition.y - kSummonedSwordEmbedDepth,
        targetPosition.z
    };
    XMFLOAT4X4 rotationMatrix;
    XMStoreFloat4x4(&rotationMatrix, BuildSummonedSwordRotation(mSummonedSwordTipAxisLocal, rotY));

    effect->Style = EffectStyle::SummonedSword;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = (std::max)(lifeTime, 0.05f);
    effect->StartDelay = (std::max)(startDelay, 0.0f);
    effect->BasePosition = startPosition;
    effect->TargetPosition = plantedPosition;
    effect->Velocity = { 0.0f, 0.0f, 0.0f };
    effect->StartScale =
    {
        uniformScale * mSummonedSwordScaleMultiplier.x,
        uniformScale * mSummonedSwordScaleMultiplier.y,
        uniformScale * mSummonedSwordScaleMultiplier.z
    };
    effect->EndScale = effect->StartScale;
    effect->StartColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    effect->EndColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    effect->RotX = 0.0f;
    effect->RotY = 0.0f;
    effect->RotZ = 0.0f;
    effect->AnchorLocalPoint = mSummonedSwordAnchorLocal;
    effect->RotationMatrix = rotationMatrix;
    const float visibleLifeTime = (std::max)(effect->LifeTime - effect->StartDelay, 0.05f);
    effect->MotionDuration = (std::min)(
        visibleLifeTime,
        motionDuration > 0.0f ? motionDuration : kSummonedSwordMotionDuration);
    effect->FadeStartTime = (std::min)(
        effect->LifeTime,
        effect->StartDelay + (motionDuration > 0.0f
            ? (effect->MotionDuration + kSummonedSwordPostImpactLife * 0.62f)
            : kSummonedSwordFadeStartTime));

    effect->Object->mIsBillboard = false;
    effect->Object->mIsAnimated = false;
    effect->Object->ClearWorldTransformOverride();
    const XMMATRIX anchorOffset = XMMatrixTranslation(
        -effect->AnchorLocalPoint.x,
        -effect->AnchorLocalPoint.y,
        -effect->AnchorLocalPoint.z);
    const XMMATRIX scaleMatrix = XMMatrixScaling(
        effect->StartScale.x,
        effect->StartScale.y,
        effect->StartScale.z);
    const XMMATRIX rotationWorld = XMLoadFloat4x4(&effect->RotationMatrix);
    const XMMATRIX translationMatrix = XMMatrixTranslation(
        startPosition.x,
        startPosition.y,
        startPosition.z);
    effect->Object->SetWorldTransform(anchorOffset * scaleMatrix * rotationWorld * translationMatrix);

    effect->Ritem->Mat = effect->Ritem->Mat != nullptr ? effect->Ritem->Mat : mSummonedSwordMaterial;
    const bool visibleImmediately = effect->StartDelay <= 0.0001f;
    effect->Ritem->Visible = visibleImmediately;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, visibleImmediately ? 1.0f : 0.0f };
    effect->Ritem->NumFramesDirty = gNumFrameResources;
}
