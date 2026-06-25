#include "SkillEffectManager.h"

#include "Archer.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Material.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace DirectX;

namespace
{
    constexpr int kGroundPoolSize = 24;
    constexpr int kBeamPoolSize = 18;
    constexpr int kArcherWindRibbonPoolSize = 64;
    constexpr int kArcherArrowRainPoolSize = 20;
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
    constexpr float kArcherBuffGroundOffset = 0.010f;
    constexpr int kArcherBuffColumnPanelCount = 8;
    constexpr float kArcherArrowRainPostImpactLife = 0.14f;
    constexpr float kArcherBasicArrowStartForwardOffset = 0.8f;
    constexpr float kArcherBasicArrowStartHeight = 0.7f;
    constexpr float kArcherBasicArrowStartRightOffset = 0.1f;
    constexpr float kArcherBasicArrowSpeed = 20.0f;
    constexpr float kArcherBasicArrowMinDistance = 3.0f;
    constexpr float kArcherBasicArrowMaxDistance = 30.0f;

    XMFLOAT3 ForwardFromYaw(float rotY)
    {
        return { std::sin(rotY), 0.0f, std::cos(rotY) };
    }

    XMFLOAT3 RightFromYaw(float rotY)
    {
        return { std::cos(rotY), 0.0f, -std::sin(rotY) };
    }

    XMFLOAT3 AddScaled(const XMFLOAT3& origin, const XMFLOAT3& direction, float scale)
    {
        return {
            origin.x + direction.x * scale,
            origin.y + direction.y * scale,
            origin.z + direction.z * scale
        };
    }

    XMFLOAT3 ArcherBuffGroundPosition(const XMFLOAT3& origin)
    {
        return {
            origin.x,
            origin.y - Player::DefaultColliderHalfHeight + kArcherBuffGroundOffset,
            origin.z
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

    XMFLOAT4 GetLevelUpColor(PlayerClass playerClass, int level)
    {
        const float intensity = level >= 3 ? 1.18f : 1.0f;
        switch (playerClass)
        {
        case PlayerClass::Warrior:
            return { 1.18f * intensity, 0.58f * intensity, 0.16f * intensity, 0.96f };
        case PlayerClass::Mage:
            return { 0.48f * intensity, 0.92f * intensity, 1.22f * intensity, 0.96f };
        case PlayerClass::Archer:
            return { 0.42f * intensity, 1.10f * intensity, 0.46f * intensity, 0.96f };
        case PlayerClass::None:
        default:
            return { 1.0f, 1.0f, 1.0f, 0.92f };
        }
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
    ClearWeaponSkillGlow();
    mLocalArcherBuffLoopActive = false;
    mArcherHasteAuraPulseTimer = 0.0f;
    mLastLocalArcherBuffPosition = { 0.0f, 0.0f, 0.0f };
    mLastLocalArcherBuffRotY = 0.0f;
    SetArcherBuffLoopVisible(false);

    for (auto& effect : mEffects)
    {
        DeactivateEffect(effect);
    }
}

void SkillEffectManager::Update(float dt)
{
    UpdateWeaponSkillGlow(dt);
    UpdateLocalArcherHasteAura(dt);

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
            currentPosition = Lerp3(
                effect.BasePosition,
                effect.TargetPosition,
                effect.UseLinearMotion ? fallT : EaseOutQuart(fallT));

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

        if (effect.Style == EffectStyle::ArrowRainArrow)
        {
            if (effect.Age < effect.StartDelay)
            {
                effect.Ritem->Visible = false;
                effect.Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
                effect.Ritem->NumFramesDirty = gNumFrameResources;
                continue;
            }

            const float arrowAge = effect.Age - effect.StartDelay;
            const float fallT = effect.MotionDuration > 0.0f
                ? (std::clamp)(arrowAge / effect.MotionDuration, 0.0f, 1.0f)
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

            effect.Object->SetPosition(currentPosition.x, currentPosition.y, currentPosition.z);
            effect.Object->SetScale(currentScale.x, currentScale.y, currentScale.z);
            effect.Object->SetRotation(effect.RotX, effect.RotY, effect.RotZ);
            effect.Object->Update();

            effect.Ritem->ColorMultiplier = currentColor;
            effect.Ritem->Visible = currentColor.w > 0.001f;
            effect.Ritem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        XMFLOAT3 animatedScale = currentScale;
        if (effect.Style == EffectStyle::ArcherWindRibbon)
        {
            const float pulse = 0.98f + 0.24f * std::sin(effect.Age * 15.0f + effect.BasePosition.x * 1.7f);
            animatedScale.x *= pulse;
            animatedScale.y *= 1.00f + 0.16f * std::sin(effect.Age * 12.0f + effect.BasePosition.z * 1.3f);
            currentColor.w *= 0.92f + 0.18f * std::sin(effect.Age * 14.0f + 0.25f);
            currentPosition.x += std::sin(effect.Age * 7.5f + effect.BasePosition.y * 3.1f) * 0.06f;
            currentPosition.z += std::cos(effect.Age * 8.2f + effect.BasePosition.x * 1.6f) * 0.06f;
        }

        effect.Object->SetPosition(currentPosition.x, currentPosition.y, currentPosition.z);
        effect.Object->SetScale(animatedScale.x, animatedScale.y, animatedScale.z);
        if (effect.Style == EffectStyle::ArcherWindRibbon)
        {
            const XMFLOAT3 cameraPosition = mGame != nullptr && mGame->GetCamera() != nullptr
                ? mGame->GetCamera()->GetPosition3f()
                : XMFLOAT3(0.0f, 0.0f, 1.0f);
            const float dx = cameraPosition.x - currentPosition.x;
            const float dz = cameraPosition.z - currentPosition.z;
            const float cameraFacingYaw = std::atan2(dx, dz);
            const float swayYaw = 0.18f * std::sin(effect.Age * 10.0f + effect.BasePosition.y * 2.7f);
            const float swayRoll = 0.12f * std::sin(effect.Age * 13.0f + effect.BasePosition.x * 1.9f);
            effect.Object->SetRotation(0.0f, cameraFacingYaw + effect.RotY + swayYaw, effect.RotZ + swayRoll);
        }
        else if (!effect.Object->mIsBillboard)
        {
            effect.Object->SetRotation(effect.RotX, effect.RotY, effect.RotZ);
        }
        effect.Object->Update();

        effect.Ritem->ColorMultiplier = currentColor;
        effect.Ritem->Visible = currentColor.w > 0.001f;
        effect.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::SpawnArcherBuffStartEffect(const XMFLOAT3& origin, float rotY)
{
    // ArcherBuff_Start
    const XMFLOAT3 groundPosition = ArcherBuffGroundPosition(origin);
    const XMFLOAT4 outerColor = { 1.26f, 1.72f, 1.40f, 1.0f };
    const XMFLOAT4 outerFade = { 0.20f, 0.52f, 0.34f, 0.0f };
    const XMFLOAT4 innerColor = { 1.50f, 1.94f, 1.66f, 1.0f };
    const XMFLOAT4 innerFade = { 0.22f, 0.58f, 0.40f, 0.0f };

    SpawnGroundDecal(
        groundPosition,
        rotY,
        0.82f,
        1.08f,
        0.24f,
        outerColor,
        outerFade,
        mArcherCircleMaterial);
    SpawnGroundDecal(
        { groundPosition.x, groundPosition.y + 0.004f, groundPosition.z },
        rotY,
        0.58f,
        0.74f,
        0.20f,
        innerColor,
        innerFade,
        mArcherCircleMaterial);
}

void SkillEffectManager::SpawnArcherBuffLoopEffect(const XMFLOAT3& origin, float rotY, float intensity)
{
    // ArcherBuff_Loop
    const XMFLOAT3 groundPosition = ArcherBuffGroundPosition(origin);
    const XMFLOAT4 ringColor =
    {
        0.92f * intensity,
        1.44f * intensity,
        1.10f * intensity,
        0.96f
    };
    const XMFLOAT4 ringFade =
    {
        0.18f * intensity,
        0.58f * intensity,
        0.38f * intensity,
        0.0f
    };
    const XMFLOAT4 coreColor =
    {
        1.18f * intensity,
        1.72f * intensity,
        1.34f * intensity,
        1.0f
    };
    const XMFLOAT4 coreFade =
    {
        0.22f * intensity,
        0.62f * intensity,
        0.42f * intensity,
        0.0f
    };

    SpawnGroundDecal(
        groundPosition,
        rotY,
        0.92f,
        0.98f,
        0.16f,
        ringColor,
        ringFade,
        mArcherCircleMaterial);
    SpawnGroundDecal(
        { groundPosition.x, groundPosition.y + 0.004f, groundPosition.z },
        rotY,
        0.66f,
        0.70f,
        0.14f,
        coreColor,
        coreFade,
        mArcherCircleMaterial);
}

void SkillEffectManager::SpawnArcherBuffFrontEffect(const XMFLOAT3& origin, float rotY, float intensity)
{
    const XMFLOAT3 groundPosition = ArcherBuffGroundPosition(origin);
    const XMFLOAT4 innerColor =
    {
        1.26f * intensity,
        1.84f * intensity,
        1.42f * intensity,
        1.0f
    };
    const XMFLOAT4 innerFade =
    {
        0.20f * intensity,
        0.60f * intensity,
        0.44f * intensity,
        0.0f
    };

    SpawnGroundDecal(
        { groundPosition.x, groundPosition.y + 0.006f, groundPosition.z },
        rotY,
        0.48f,
        0.54f,
        0.10f,
        innerColor,
        innerFade,
        mArcherCircleMaterial);
}

void SkillEffectManager::SpawnArcherBuffEndEffect(const XMFLOAT3& origin, float rotY)
{
    // ArcherBuff_End
    const XMFLOAT3 groundPosition = ArcherBuffGroundPosition(origin);
    const XMFLOAT4 endColor = { 0.76f, 1.06f, 0.88f, 0.72f };
    const XMFLOAT4 endFade = { 0.12f, 0.24f, 0.18f, 0.0f };

    SpawnGroundDecal(
        groundPosition,
        rotY,
        0.98f,
        0.52f,
        0.18f,
        endColor,
        endFade,
        mArcherCircleMaterial);
    SpawnArcherDustBurst(origin, rotY, 1.0f, 1.0f);
}

void SkillEffectManager::TriggerLevelUpEffect(const XMFLOAT3& origin, float rotY, PlayerClass playerClass, int newLevel)
{
    const XMFLOAT3 groundPosition =
    {
        origin.x,
        origin.y - Player::DefaultColliderHalfHeight + 0.015f,
        origin.z
    };
    const XMFLOAT3 bodyCenter =
    {
        origin.x,
        origin.y + Player::DefaultColliderHalfHeight * 0.92f,
        origin.z
    };
    const XMFLOAT4 coreColor = GetLevelUpColor(playerClass, newLevel);
    const XMFLOAT4 outerFade = FadeColor(coreColor, 0.0f);
    const XMFLOAT4 innerColor =
    {
        (std::min)(coreColor.x * 1.18f, 1.8f),
        (std::min)(coreColor.y * 1.18f, 1.8f),
        (std::min)(coreColor.z * 1.18f, 1.8f),
        1.0f
    };
    const bool isFinalLevel = newLevel >= 3;
    const float levelScale = isFinalLevel ? 1.36f : 1.0f;
    const int ribbonCount = isFinalLevel ? 8 : 6;

    SpawnGroundDecal(
        groundPosition,
        rotY,
        0.62f * levelScale,
        1.56f * levelScale,
        0.55f,
        { coreColor.x, coreColor.y, coreColor.z, 0.88f },
        outerFade,
        mArcherCircleMaterial);
    SpawnGroundDecal(
        { groundPosition.x, groundPosition.y + 0.004f, groundPosition.z },
        rotY,
        0.40f * levelScale,
        1.06f * levelScale,
        0.36f,
        { innerColor.x, innerColor.y, innerColor.z, 0.96f },
        { innerColor.x, innerColor.y, innerColor.z, 0.0f },
        mArcherCircleMaterial);

    SpawnVerticalBeam(
        { bodyCenter.x, groundPosition.y, bodyCenter.z },
        rotY,
        0.26f * levelScale,
        1.95f * levelScale,
        0.42f,
        { innerColor.x, innerColor.y, innerColor.z, 0.62f },
        { coreColor.x, coreColor.y, coreColor.z, 0.0f });
    SpawnVerticalBeam(
        { bodyCenter.x, groundPosition.y, bodyCenter.z },
        rotY + XM_PIDIV4,
        0.16f * levelScale,
        1.62f * levelScale,
        0.34f,
        { coreColor.x, coreColor.y, coreColor.z, 0.42f },
        { coreColor.x, coreColor.y, coreColor.z, 0.0f });
    SpawnVerticalBeam(
        { bodyCenter.x, groundPosition.y, bodyCenter.z },
        rotY - XM_PIDIV4,
        0.16f * levelScale,
        1.62f * levelScale,
        0.34f,
        { coreColor.x, coreColor.y, coreColor.z, 0.42f },
        { coreColor.x, coreColor.y, coreColor.z, 0.0f });

    if (isFinalLevel)
    {
        SpawnGroundDecal(
            { groundPosition.x, groundPosition.y + 0.008f, groundPosition.z },
            rotY,
            0.86f * levelScale,
            1.92f * levelScale,
            0.60f,
            { innerColor.x, innerColor.y, innerColor.z, 0.78f },
            { innerColor.x, innerColor.y, innerColor.z, 0.0f },
            mArcherCircleMaterial);
    }

    for (int i = 0; i < ribbonCount; ++i)
    {
        const float angle = rotY + XM_2PI * (static_cast<float>(i) / static_cast<float>(ribbonCount));
        const XMFLOAT3 radial = { std::sin(angle), 0.0f, std::cos(angle) };
        const XMFLOAT3 ribbonOrigin =
        {
            origin.x + radial.x * (isFinalLevel ? 0.24f : 0.18f),
            groundPosition.y + 0.32f + 0.05f * static_cast<float>(i % 2),
            origin.z + radial.z * (isFinalLevel ? 0.24f : 0.18f)
        };
        const XMFLOAT3 ribbonVelocity =
        {
            radial.x * (isFinalLevel ? 0.28f : 0.20f),
            (isFinalLevel ? 1.92f : 1.55f) + 0.08f * static_cast<float>(i),
            radial.z * (isFinalLevel ? 0.28f : 0.20f)
        };

        SpawnArcherWindRibbon(
            ribbonOrigin,
            ribbonVelocity,
            (isFinalLevel ? 0.15f : 0.12f) * levelScale,
            (isFinalLevel ? 0.72f : 0.58f) * levelScale,
            0.04f,
            (isFinalLevel ? 0.32f : 0.24f) * levelScale,
            isFinalLevel ? 0.52f : 0.42f,
            { coreColor.x, coreColor.y, coreColor.z, isFinalLevel ? 0.70f : 0.58f },
            { innerColor.x, innerColor.y, innerColor.z, 0.0f },
            angle,
            0.0f,
            mArcherWindMaterial);
    }
}

void SkillEffectManager::SetArcherBuffLoopVisible(bool visible)
{
    auto ApplyVisibility = [visible](RenderItem* ritem)
    {
        if (ritem == nullptr)
        {
            return;
        }

        ritem->Visible = visible;
        if (!visible)
        {
            ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
        }
        ritem->NumFramesDirty = gNumFrameResources;
    };

    ApplyVisibility(mArcherBuffLoopOuterRitem);
    ApplyVisibility(mArcherBuffLoopInnerRitem);
    for (RenderItem* ritem : mArcherBuffLoopFlowRitems)
    {
        ApplyVisibility(ritem);
    }
}

void SkillEffectManager::UpdateArcherBuffLoopVisuals(const XMFLOAT3& origin, float rotY, float intensity)
{
    EnsureArcherBuffLoopVisuals();
    if (mArcherBuffLoopOuterObject == nullptr ||
        mArcherBuffLoopInnerObject == nullptr ||
        mArcherBuffLoopOuterRitem == nullptr ||
        mArcherBuffLoopInnerRitem == nullptr)
    {
        return;
    }

    const XMFLOAT3 groundPosition = ArcherBuffGroundPosition(origin);
    const float flowBaseAngle = mArcherHasteAuraPulseTimer * 1.6f + rotY * 0.18f;

    mArcherBuffLoopOuterObject->SetPosition(groundPosition.x, groundPosition.y, groundPosition.z);
    mArcherBuffLoopOuterObject->SetRotation(XM_PIDIV2, rotY, 0.0f);
    mArcherBuffLoopOuterObject->SetScale(1.00f, 1.00f, 1.0f);
    mArcherBuffLoopOuterObject->Update();

    mArcherBuffLoopInnerObject->SetPosition(groundPosition.x, groundPosition.y + 0.004f, groundPosition.z);
    mArcherBuffLoopInnerObject->SetRotation(XM_PIDIV2, rotY, 0.0f);
    mArcherBuffLoopInnerObject->SetScale(0.74f, 0.74f, 1.0f);
    mArcherBuffLoopInnerObject->Update();

    mArcherBuffLoopOuterRitem->Mat = mArcherCircleMaterial != nullptr ? mArcherCircleMaterial : mDecalMaterial;
    mArcherBuffLoopOuterRitem->ColorMultiplier =
    {
        1.22f * intensity,
        1.72f * intensity,
        1.34f * intensity,
        0.98f
    };
    mArcherBuffLoopOuterRitem->Visible = true;
    mArcherBuffLoopOuterRitem->NumFramesDirty = gNumFrameResources;

    mArcherBuffLoopInnerRitem->Mat = mArcherCircleMaterial != nullptr ? mArcherCircleMaterial : mDecalMaterial;
    mArcherBuffLoopInnerRitem->ColorMultiplier =
    {
        1.52f * intensity,
        2.02f * intensity,
        1.64f * intensity,
        1.0f
    };
    mArcherBuffLoopInnerRitem->Visible = true;
    mArcherBuffLoopInnerRitem->NumFramesDirty = gNumFrameResources;

    for (size_t i = 0; i < mArcherBuffLoopFlowObjects.size() && i < mArcherBuffLoopFlowRitems.size(); ++i)
    {
        GameObject* flowObject = mArcherBuffLoopFlowObjects[i];
        RenderItem* flowRitem = mArcherBuffLoopFlowRitems[i];
        if (flowObject == nullptr || flowRitem == nullptr)
        {
            continue;
        }

        const bool outerLayer = (i % 2) == 0;
        const float layerPhase = outerLayer
            ? flowBaseAngle
            : (-flowBaseAngle * 0.78f + 0.18f);
        const float angle =
            static_cast<float>(i) * XM_2PI / static_cast<float>(kArcherBuffColumnPanelCount) +
            layerPhase;
        const float radius = outerLayer ? 0.56f : 0.70f;
        const float heightCenter =
            groundPosition.y +
            (outerLayer ? 0.84f : 0.90f) +
            0.05f * std::sin(mArcherHasteAuraPulseTimer * 2.6f + static_cast<float>(i) * 0.7f);
        const float width = outerLayer ? 0.28f : 0.34f;
        const float height = outerLayer ? 1.42f : 1.60f;
        const XMFLOAT3 flowPosition =
        {
            groundPosition.x + std::cos(angle) * radius,
            heightCenter,
            groundPosition.z + std::sin(angle) * radius
        };
        const float tangentYaw = angle + XM_PIDIV2;

        flowObject->SetPosition(flowPosition.x, flowPosition.y, flowPosition.z);
        flowObject->SetRotation(0.0f, tangentYaw, 0.0f);
        flowObject->SetScale(width, height, 1.0f);
        flowObject->Update();

        flowRitem->Mat = mArcherColumnMaterial != nullptr ? mArcherColumnMaterial : mArcherCircleMaterial;
        flowRitem->ColorMultiplier =
        {
            1.24f * intensity,
            1.86f * intensity,
            1.58f * intensity,
            outerLayer ? 0.90f : 0.72f
        };
        flowRitem->Visible = true;
        flowRitem->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::OnSkillCast(PlayerClass playerClass, int skillIndex, const XMFLOAT3& origin, float rotY, float activeDuration)
{
    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT4 skillColor = GetSkillColor(playerClass, skillIndex);
    const XMFLOAT4 fadeColor = FadeColor(skillColor, 0.0f);
    const float glowDuration = (std::max)(activeDuration, 0.10f);

    switch (playerClass)
    {
    case PlayerClass::Warrior:
        if (skillIndex == 1)
        {
            TriggerWeaponSkillGlow({ 3.00f, 0.60f, 0.48f, 1.0f }, glowDuration);
        }
        else if (skillIndex == 2)
        {
            TriggerWeaponSkillGlow({ 0.72f, 1.60f, 3.20f, 1.0f }, glowDuration);
        }
        break;

    case PlayerClass::Mage:
        if (skillIndex == 1)
        {
            SpawnGroundDecal({ origin.x, origin.y + 0.05f, origin.z }, rotY, 0.28f, 0.96f, 0.45f, skillColor, fadeColor);
        }
        else if (skillIndex == 2)
        {
            const XMFLOAT3 target = AddScaled(origin, forward, 2.0f);
            SpawnGroundDecal({ target.x, origin.y + 0.05f, target.z }, rotY, 0.40f, 1.60f, 0.58f, skillColor, fadeColor);
        }
        break;

    case PlayerClass::Archer:
        if (skillIndex == 1)
        {
            SpawnArcherBuffStartEffect(origin, rotY);
        }
        break;

    case PlayerClass::None:
    default:
        break;
    }
}

void SkillEffectManager::OnRemoteSkillCast(
    PlayerClass playerClass,
    int skillIndex,
    const XMFLOAT3& origin,
    const XMFLOAT3& impactCenter,
    float rotY,
    float effectRadius)
{
    if (skillIndex < 1 || skillIndex > 2)
    {
        return;
    }

    if (playerClass == PlayerClass::Warrior)
    {
        if (skillIndex == 1)
        {
            // The local-weapon glow is intentionally excluded for remote players.
            OnSkillResolved(playerClass, skillIndex, impactCenter, rotY, effectRadius);
        }
        else
        {
            OnSkillResolved(playerClass, skillIndex, impactCenter, rotY, effectRadius);
            PreviewWarriorSwordStrike(impactCenter, rotY, effectRadius, 0.18f, 0.0f);
        }
        return;
    }

    if (playerClass == PlayerClass::Archer && skillIndex == 2)
    {
        OnSkillResolved(playerClass, skillIndex, impactCenter, rotY, effectRadius);
        return;
    }

    OnSkillCast(playerClass, skillIndex, origin, rotY, 0.55f);
}

void SkillEffectManager::OnSkillImpact(PlayerClass playerClass, int skillIndex, const XMFLOAT3& hitPosition)
{
    (void)playerClass;
    (void)skillIndex;
    (void)hitPosition;
}

void SkillEffectManager::OnArcherHasteBasicShot(const XMFLOAT3& origin, float rotY, float intensity)
{
    (void)origin;
    (void)rotY;
    (void)intensity;
}

void SkillEffectManager::SpawnArcherBasicArrow(const XMFLOAT3& origin, float rotY, float travelDistance, float startDelay)
{
    EnsureArcherArrowRainPool();

    EffectInstance* effect = AcquireEffect(EffectStyle::ArrowRainArrow);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT3 right = RightFromYaw(rotY);
    const float clampedDistance = (std::clamp)(
        travelDistance,
        kArcherBasicArrowMinDistance,
        kArcherBasicArrowMaxDistance);
    const XMFLOAT3 startPosition =
    {
        origin.x + forward.x * kArcherBasicArrowStartForwardOffset + right.x * kArcherBasicArrowStartRightOffset,
        origin.y + kArcherBasicArrowStartHeight,
        origin.z + forward.z * kArcherBasicArrowStartForwardOffset + right.z * kArcherBasicArrowStartRightOffset
    };
    const XMFLOAT3 targetPosition =
    {
        startPosition.x + forward.x * clampedDistance,
        startPosition.y,
        startPosition.z + forward.z * clampedDistance
    };
    const float motionDuration = (std::max)(clampedDistance / kArcherBasicArrowSpeed, 0.12f);
    const float clampedStartDelay = (std::max)(startDelay, 0.0f);

    effect->Style = EffectStyle::ArrowRainArrow;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = clampedStartDelay + motionDuration + 0.10f;
    effect->BasePosition = startPosition;
    effect->TargetPosition = targetPosition;
    effect->Velocity = { 0.0f, 0.0f, 0.0f };
    effect->StartScale = { 1.0f, 1.0f, 1.0f };
    effect->EndScale = effect->StartScale;
    effect->StartColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    effect->EndColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    effect->RotX = 0.0f;
    effect->RotY = rotY;
    effect->RotZ = 0.0f;
    effect->StartDelay = clampedStartDelay;
    effect->MotionDuration = motionDuration;
    effect->FadeStartTime = clampedStartDelay + motionDuration;
    effect->UseLinearMotion = true;

    effect->Object->mIsBillboard = false;
    effect->Object->mIsAnimated = false;
    effect->Object->SetPosition(startPosition.x, startPosition.y, startPosition.z);
    effect->Object->SetScale(1.0f, 1.0f, 1.0f);
    effect->Object->SetRotation(effect->RotX, effect->RotY, effect->RotZ);
    effect->Object->Update();

    effect->Ritem->Mat = mArcherArrowMaterial != nullptr
        ? mArcherArrowMaterial
        : effect->Ritem->Mat;
    effect->Ritem->Visible = clampedStartDelay <= 0.0001f;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, effect->Ritem->Visible ? 1.0f : 0.0f };
    effect->Ritem->NumFramesDirty = gNumFrameResources;
}

void SkillEffectManager::UpdateLocalArcherHasteAura(float dt)
{
    (void)dt;

    if (mGame == nullptr)
    {
        return;
    }

    auto* archer = dynamic_cast<Archer*>(mGame->GetPlayer());
    const bool buffActive = archer != nullptr && archer->HasAttackSpeedBuff();
    if (!buffActive)
    {
        if (mLocalArcherBuffLoopActive)
        {
            SpawnArcherBuffEndEffect(mLastLocalArcherBuffPosition, mLastLocalArcherBuffRotY);
        }

        mLocalArcherBuffLoopActive = false;
        SetArcherBuffLoopVisible(false);
        mArcherHasteAuraPulseTimer = 0.0f;
        return;
    }

    const XMFLOAT3 origin = archer->GetPosition();
    const float rotY = archer->GetFacingRotY();
    const float intensity = (std::max)(archer->GetSkillEffectIntensityMultiplier(), 1.0f);
    mLastLocalArcherBuffPosition = origin;
    mLastLocalArcherBuffRotY = rotY;

    if (!mLocalArcherBuffLoopActive)
    {
        mLocalArcherBuffLoopActive = true;
        EnsureArcherBuffLoopVisuals();
    }

    mArcherHasteAuraPulseTimer += dt;
    UpdateArcherBuffLoopVisuals(origin, rotY, intensity);
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

void SkillEffectManager::PreviewArcherArrowRain(
    const XMFLOAT3& targetPosition,
    float effectRadius,
    float impactDelay)
{
    const float clampedDelay = (std::max)(impactDelay, 0.16f);
    const float radius = (std::max)(effectRadius, 0.90f);
    const float telegraphLife = clampedDelay + 0.08f;
    const XMFLOAT4 outerColor = { 1.24f, 0.98f, 0.42f, 0.96f };
    const XMFLOAT4 outerFade = { 0.54f, 0.36f, 0.10f, 0.20f };
    const XMFLOAT4 innerColor = { 1.38f, 1.18f, 0.62f, 0.88f };
    const XMFLOAT4 innerFade = { 0.72f, 0.54f, 0.16f, 0.12f };

    SpawnGroundDecal(
        { targetPosition.x, targetPosition.y + 0.035f, targetPosition.z },
        0.0f,
        radius * 0.82f,
        radius * 0.92f,
        telegraphLife,
        outerColor,
        outerFade,
        mArcherArrowRainDecalMaterial != nullptr ? mArcherArrowRainDecalMaterial : mArcherCircleMaterial);
    SpawnGroundDecal(
        { targetPosition.x, targetPosition.y + 0.040f, targetPosition.z },
        XM_PIDIV4,
        radius * 0.46f,
        radius * 0.58f,
        telegraphLife,
        innerColor,
        innerFade,
        mArcherArrowRainDecalMaterial != nullptr ? mArcherArrowRainDecalMaterial : mArcherCircleMaterial);

    struct RainArrowSpec
    {
        float angle;
        float radialScale;
        float yaw;
        float startDelayScale;
        float spawnHeight;
        float scale;
    };

    static constexpr RainArrowSpec kRainArrows[] =
    {
        { 0.10f, 0.00f, 0.04f, 0.00f, 3.8f, 0.82f },
        { 0.84f, 0.30f, 0.22f, 0.05f, 4.1f, 0.78f },
        { 1.62f, 0.54f, -0.18f, 0.08f, 4.3f, 0.80f },
        { 2.34f, 0.40f, 0.34f, 0.02f, 3.9f, 0.76f },
        { 3.04f, 0.66f, -0.12f, 0.10f, 4.5f, 0.74f },
        { 3.92f, 0.22f, 0.28f, 0.00f, 3.7f, 0.84f },
        { 4.76f, 0.48f, -0.26f, 0.06f, 4.0f, 0.80f },
        { 5.54f, 0.34f, 0.16f, 0.03f, 3.85f, 0.78f },
        { 2.92f, 0.18f, -0.05f, 0.09f, 4.2f, 0.76f }
    };

    for (const RainArrowSpec& arrow : kRainArrows)
    {
        const XMFLOAT3 impactPosition =
        {
            targetPosition.x + std::cos(arrow.angle) * radius * arrow.radialScale,
            targetPosition.y,
            targetPosition.z + std::sin(arrow.angle) * radius * arrow.radialScale
        };
        const float startDelay = clampedDelay * arrow.startDelayScale;
        const float fallDuration = (std::max)(clampedDelay - startDelay, 0.16f);
        SpawnArcherArrowRainArrow(
            impactPosition,
            arrow.yaw,
            startDelay,
            fallDuration,
            arrow.scale,
            arrow.spawnHeight);
    }
}

void SkillEffectManager::OnSkillResolved(PlayerClass playerClass, int skillIndex, const XMFLOAT3& impactCenter, float rotY, float effectRadius)
{
    if (playerClass == PlayerClass::Warrior && skillIndex == 1)
    {
        const float decalScale = (std::max)(effectRadius, 0.1f);
        const XMFLOAT4 crackColor = { 1.18f, 1.18f, 1.18f, 1.0f };
        const XMFLOAT3 decalPosition =
        {
            impactCenter.x,
            impactCenter.y - Player::DefaultColliderHalfHeight + 0.05f,
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
    else if (playerClass == PlayerClass::Archer && skillIndex == 2)
    {
        const float ringScale = (std::max)(effectRadius, 0.9f);
        const XMFLOAT4 ringColor = { 1.42f, 1.12f, 0.42f, 1.0f };
        const XMFLOAT4 ringFade = { 1.0f, 0.52f, 0.14f, 0.0f };
        const XMFLOAT4 flashColor = { 1.64f, 1.34f, 0.66f, 0.96f };
        const XMFLOAT4 flashFade = { 0.92f, 0.56f, 0.12f, 0.0f };

        SpawnGroundDecal(
            { impactCenter.x, impactCenter.y + 0.04f, impactCenter.z },
            rotY,
            ringScale * 0.54f,
            ringScale * 1.14f,
            0.24f,
            ringColor,
            ringFade,
            mArcherArrowRainDecalMaterial != nullptr ? mArcherArrowRainDecalMaterial : mArcherCircleMaterial);
        SpawnGroundDecal(
            { impactCenter.x, impactCenter.y + 0.045f, impactCenter.z },
            rotY + XM_PIDIV4,
            ringScale * 0.26f,
            ringScale * 0.70f,
            0.18f,
            flashColor,
            flashFade,
            mArcherArrowRainDecalMaterial != nullptr ? mArcherArrowRainDecalMaterial : mArcherCircleMaterial);
    }
}

void SkillEffectManager::EnsureResources()
{
    auto* resources = (mGame != nullptr) ? mGame->GetResources() : nullptr;
    if (resources == nullptr)
    {
        return;
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
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.04f, 0.04f, 0.04f),
            0.22f);
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

    const std::string archerCircleTextureName =
        resources->GetTexture("Effect_Circle03") != nullptr ? "Effect_Circle03" :
        (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white");
    if (resources->GetMaterial("SkillFx_ArcherCircleMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherCircleMat",
            static_cast<int>(resources->mMaterials.size()),
            archerCircleTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.64f, 2.06f, 1.78f, 1.0f),
            XMFLOAT3(0.08f, 0.12f, 0.10f),
            0.02f);
    }

    const std::string archerArrowRainTextureName =
        resources->GetTexture("Effect_Circle02") != nullptr ? "Effect_Circle02" :
        (resources->GetTexture("Effect_Circle03") != nullptr ? "Effect_Circle03" :
            (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white"));
    if (resources->GetMaterial("SkillFx_ArcherArrowRainDecalMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherArrowRainDecalMat",
            static_cast<int>(resources->mMaterials.size()),
            archerArrowRainTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.48f, 1.24f, 0.58f, 1.0f),
            XMFLOAT3(0.08f, 0.10f, 0.08f),
            0.02f);
    }

    const std::string archerWindTextureName =
        resources->GetTexture("WindRibbon_Archer") != nullptr ? "WindRibbon_Archer" :
        (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white");
    if (resources->GetMaterial("SkillFx_ArcherWindMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherWindMat",
            static_cast<int>(resources->mMaterials.size()),
            archerWindTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.86f, 1.0f, 0.94f, 0.92f),
            XMFLOAT3(0.02f, 0.04f, 0.03f),
            0.04f);
    }

    const std::string archerColumnTextureName =
        resources->GetTexture("Effect_Scratch01") != nullptr ? "Effect_Scratch01" : "white";
    if (resources->GetMaterial("SkillFx_ArcherColumnMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherColumnMat",
            static_cast<int>(resources->mMaterials.size()),
            archerColumnTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.00f, 1.24f, 1.12f, 0.96f),
            XMFLOAT3(0.08f, 0.12f, 0.10f),
            0.02f);
    }

    const std::string archerSlashTextureName =
        resources->GetTexture("Effect_Scratch01") != nullptr ? "Effect_Scratch01" :
        (resources->GetTexture("WindRibbon_Archer") != nullptr ? "WindRibbon_Archer" : "white");
    if (resources->GetMaterial("SkillFx_ArcherSlashMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherSlashMat",
            static_cast<int>(resources->mMaterials.size()),
            archerSlashTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.28f, 1.54f, 1.30f, 1.0f),
            XMFLOAT3(0.08f, 0.12f, 0.10f),
            0.02f);
    }

    const std::string archerDustTextureName =
        resources->GetTexture("Effect_Dirt01") != nullptr ? "Effect_Dirt01" : "white";
    if (resources->GetMaterial("SkillFx_ArcherDustMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherDustMat",
            static_cast<int>(resources->mMaterials.size()),
            archerDustTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.96f, 1.12f, 1.00f, 0.82f),
            XMFLOAT3(0.04f, 0.06f, 0.05f),
            0.06f);
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
        mEarthshatterDecalMaterial->DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        mEarthshatterDecalMaterial->Roughness = 0.22f;
        mEarthshatterDecalMaterial->IsTransparent = 3;
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

    mArcherCircleMaterial = resources->GetMaterial("SkillFx_ArcherCircleMat");
    if (mArcherCircleMaterial != nullptr)
    {
        mArcherCircleMaterial->DiffuseMapName = archerCircleTextureName;
        mArcherCircleMaterial->DiffuseAlbedo = { 1.88f, 2.24f, 1.98f, 1.0f };
        mArcherCircleMaterial->FresnelR0 = { 0.12f, 0.18f, 0.14f };
        mArcherCircleMaterial->Roughness = 0.01f;
        mArcherCircleMaterial->IsTransparent = 1;
        mArcherCircleMaterial->IsToon = 0;
        mArcherCircleMaterial->OutlineThickness = 0.0f;
        mArcherCircleMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherArrowRainDecalMaterial = resources->GetMaterial("SkillFx_ArcherArrowRainDecalMat");
    if (mArcherArrowRainDecalMaterial != nullptr)
    {
        mArcherArrowRainDecalMaterial->DiffuseMapName = archerArrowRainTextureName;
        mArcherArrowRainDecalMaterial->DiffuseAlbedo = { 1.56f, 1.30f, 0.64f, 1.0f };
        mArcherArrowRainDecalMaterial->FresnelR0 = { 0.10f, 0.12f, 0.10f };
        mArcherArrowRainDecalMaterial->Roughness = 0.01f;
        mArcherArrowRainDecalMaterial->IsTransparent = 1;
        mArcherArrowRainDecalMaterial->IsToon = 0;
        mArcherArrowRainDecalMaterial->OutlineThickness = 0.0f;
        mArcherArrowRainDecalMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherColumnMaterial = resources->GetMaterial("SkillFx_ArcherColumnMat");
    if (mArcherColumnMaterial != nullptr)
    {
        mArcherColumnMaterial->DiffuseMapName = archerColumnTextureName;
        mArcherColumnMaterial->DiffuseAlbedo = { 1.38f, 1.72f, 1.60f, 1.0f };
        mArcherColumnMaterial->FresnelR0 = { 0.10f, 0.14f, 0.12f };
        mArcherColumnMaterial->Roughness = 0.01f;
        mArcherColumnMaterial->IsTransparent = 1;
        mArcherColumnMaterial->IsToon = 0;
        mArcherColumnMaterial->OutlineThickness = 0.0f;
        mArcherColumnMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherWindMaterial = resources->GetMaterial("SkillFx_ArcherWindMat");
    if (mArcherWindMaterial != nullptr)
    {
        mArcherWindMaterial->DiffuseMapName = archerWindTextureName;
        mArcherWindMaterial->DiffuseAlbedo = { 1.28f, 1.36f, 1.34f, 1.0f };
        mArcherWindMaterial->FresnelR0 = { 0.08f, 0.12f, 0.10f };
        mArcherWindMaterial->Roughness = 0.02f;
        mArcherWindMaterial->IsTransparent = 1;
        mArcherWindMaterial->IsToon = 0;
        mArcherWindMaterial->OutlineThickness = 0.0f;
        mArcherWindMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherSlashMaterial = resources->GetMaterial("SkillFx_ArcherSlashMat");
    if (mArcherSlashMaterial != nullptr)
    {
        mArcherSlashMaterial->DiffuseMapName = archerSlashTextureName;
        mArcherSlashMaterial->DiffuseAlbedo = { 1.42f, 1.62f, 1.46f, 1.0f };
        mArcherSlashMaterial->FresnelR0 = { 0.10f, 0.14f, 0.11f };
        mArcherSlashMaterial->Roughness = 0.01f;
        mArcherSlashMaterial->IsTransparent = 1;
        mArcherSlashMaterial->IsToon = 0;
        mArcherSlashMaterial->OutlineThickness = 0.0f;
        mArcherSlashMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherDustMaterial = resources->GetMaterial("SkillFx_ArcherDustMat");
    if (mArcherDustMaterial != nullptr)
    {
        mArcherDustMaterial->DiffuseMapName = archerDustTextureName;
        mArcherDustMaterial->DiffuseAlbedo = { 1.06f, 1.18f, 1.10f, 0.92f };
        mArcherDustMaterial->FresnelR0 = { 0.05f, 0.07f, 0.06f };
        mArcherDustMaterial->Roughness = 0.05f;
        mArcherDustMaterial->IsTransparent = 1;
        mArcherDustMaterial->IsToon = 0;
        mArcherDustMaterial->OutlineThickness = 0.0f;
        mArcherDustMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherArrowMaterial = resources->GetMaterial("ArcherArrowMat");
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
        if (style == EffectStyle::VerticalBeam)
        {
            renderItem->Mat = mBeamMaterial;
        }
        else if (style == EffectStyle::ArcherWindRibbon)
        {
            renderItem->Mat = mArcherWindMaterial != nullptr ? mArcherWindMaterial : mBeamMaterial;
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
        object->mIsBillboard = false;
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

    for (int i = 0; i < kGroundPoolSize; ++i)
    {
        CreateEffect(EffectStyle::GroundDecal);
    }

    for (int i = 0; i < kBeamPoolSize; ++i)
    {
        CreateEffect(EffectStyle::VerticalBeam);
    }

    for (int i = 0; i < kArcherWindRibbonPoolSize; ++i)
    {
        CreateEffect(EffectStyle::ArcherWindRibbon);
    }

    EnsureArcherArrowRainPool();
    EnsureArcherBuffLoopVisuals();
}

void SkillEffectManager::EnsureArcherArrowRainPool()
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
            return effect.Style == EffectStyle::ArrowRainArrow;
        });
    if (alreadyCreated)
    {
        return;
    }

    auto* resources = mGame->GetResources();
    auto geoIt = resources->mGeometries.find("archerBasicArrowGeo");
    if (geoIt == resources->mGeometries.end() || geoIt->second == nullptr)
    {
        return;
    }

    Material* material = mArcherArrowMaterial != nullptr ? mArcherArrowMaterial : resources->GetMaterial("ArcherArrowMat");
    if (material == nullptr)
    {
        return;
    }

    auto submeshIt = geoIt->second->DrawArgs.find("mesh");
    if (submeshIt == geoIt->second->DrawArgs.end())
    {
        return;
    }

    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    for (int i = 0; i < kArcherArrowRainPoolSize; ++i)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Geo = geoIt->second.get();
        renderItem->Mat = material;
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->IndexCount = submeshIt->second.IndexCount;
        renderItem->StartIndexLocation = submeshIt->second.StartIndexLocation;
        renderItem->BaseVertexLocation = submeshIt->second.BaseVertexLocation;
        renderItem->Visible = false;
        renderItem->CastShadow = false;
        renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->mIsBillboard = false;
        object->mIsAnimated = false;
        object->SetScale(0.0f, 0.0f, 0.0f);
        object->SetPosition(0.0f, -1000.0f, 0.0f);
        object->SetRotation(XM_PIDIV2, 0.0f, 0.0f);
        object->Update();

        EffectInstance instance;
        instance.Object = object.get();
        instance.Ritem = renderItem.get();
        instance.Style = EffectStyle::ArrowRainArrow;
        mEffects.push_back(instance);

        if (mTrackOwned)
        {
            mTrackOwned(object.get(), renderItem.get());
        }

        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    }
}

void SkillEffectManager::EnsureArcherBuffLoopVisuals()
{
    if (mGame == nullptr || mGame->GetResources() == nullptr)
    {
        return;
    }

    if (mArcherBuffLoopOuterObject != nullptr &&
        mArcherBuffLoopInnerObject != nullptr &&
        mArcherBuffLoopOuterRitem != nullptr &&
        mArcherBuffLoopInnerRitem != nullptr &&
        mArcherBuffLoopFlowObjects.size() == kArcherBuffColumnPanelCount &&
        mArcherBuffLoopFlowRitems.size() == kArcherBuffColumnPanelCount)
    {
        return;
    }

    auto* resources = mGame->GetResources();
    auto geoIt = resources->mGeometries.find("quadGeo");
    if (geoIt == resources->mGeometries.end() || geoIt->second == nullptr)
    {
        return;
    }

    const auto& drawArgs = geoIt->second->DrawArgs["quad"];
    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    auto CreateLoopRing = [&](GameObject*& outObject, RenderItem*& outRitem)
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Geo = geoIt->second.get();
        renderItem->Mat = mArcherCircleMaterial != nullptr ? mArcherCircleMaterial : mDecalMaterial;
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->IndexCount = drawArgs.IndexCount;
        renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
        renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        renderItem->Visible = false;
        renderItem->CastShadow = false;
        renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->mIsBillboard = false;
        object->mIsAnimated = false;
        object->SetScale(0.0f, 0.0f, 1.0f);
        object->SetPosition(0.0f, -1000.0f, 0.0f);
        object->SetRotation(XM_PIDIV2, 0.0f, 0.0f);
        object->Update();

        outObject = object.get();
        outRitem = renderItem.get();

        if (mTrackOwned)
        {
            mTrackOwned(object.get(), renderItem.get());
        }

        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    };

    CreateLoopRing(mArcherBuffLoopOuterObject, mArcherBuffLoopOuterRitem);
    CreateLoopRing(mArcherBuffLoopInnerObject, mArcherBuffLoopInnerRitem);

    if (mArcherBuffLoopFlowObjects.size() != kArcherBuffColumnPanelCount ||
        mArcherBuffLoopFlowRitems.size() != kArcherBuffColumnPanelCount)
    {
        mArcherBuffLoopFlowObjects.clear();
        mArcherBuffLoopFlowRitems.clear();

        auto CreateFlowStreak = [&]()
        {
            auto renderItem = std::make_unique<RenderItem>();
            renderItem->World = MathHelper::Identity4x4();
            renderItem->TexTransform = MathHelper::Identity4x4();
            renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
            renderItem->Geo = geoIt->second.get();
            renderItem->Mat = mArcherColumnMaterial != nullptr ? mArcherColumnMaterial : mArcherCircleMaterial;
            renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            renderItem->IndexCount = drawArgs.IndexCount;
            renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
            renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
            renderItem->Visible = false;
            renderItem->CastShadow = false;
            renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };

            auto object = std::make_unique<GameObject>();
            object->Ritem = renderItem.get();
            object->mIsBillboard = false;
            object->mIsAnimated = false;
            object->SetScale(0.0f, 0.0f, 1.0f);
            object->SetPosition(0.0f, -1000.0f, 0.0f);
            object->SetRotation(0.0f, 0.0f, 0.0f);
            object->Update();

            mArcherBuffLoopFlowObjects.push_back(object.get());
            mArcherBuffLoopFlowRitems.push_back(renderItem.get());

            if (mTrackOwned)
            {
                mTrackOwned(object.get(), renderItem.get());
            }

            ritems.push_back(std::move(renderItem));
            objects.push_back(std::move(object));
        };

        for (int i = 0; i < kArcherBuffColumnPanelCount; ++i)
        {
            CreateFlowStreak();
        }
    }

    SetArcherBuffLoopVisible(false);
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

    auto* resources = mGame->GetResources();
    auto geoIt = resources->mGeometries.find("warriorLv3SwordGeo");
    MeshGeometry* geometry = geoIt != resources->mGeometries.end() ? geoIt->second.get() : nullptr;
    Material* material = resources->GetMaterial("PlayerSwordMat");

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
    effect.UseLinearMotion = false;

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

void SkillEffectManager::TriggerWeaponSkillGlow(const XMFLOAT4& glowColor, float duration)
{
    Material* glowMaterial = EnsureWeaponGlowMaterial();
    mWeaponGlowColor = glowColor;
    mWeaponGlowDuration = (std::max)(duration, 0.05f);
    mWeaponGlowTimer = mWeaponGlowDuration;

    auto* weaponObject = (mGame != nullptr) ? mGame->GetPlayerWeaponObject() : nullptr;
    auto* weaponRitem = weaponObject != nullptr ? weaponObject->Ritem : nullptr;
    if (weaponRitem != nullptr)
    {
        weaponRitem->ColorMultiplier = glowColor;
        weaponRitem->NumFramesDirty = gNumFrameResources;
    }

    if (glowMaterial != nullptr)
    {
        const XMFLOAT4 outlineColor =
        {
            (std::min)(glowColor.x, 1.0f),
            (std::min)(glowColor.y, 1.0f),
            (std::min)(glowColor.z, 1.0f),
            1.0f
        };

        glowMaterial->DiffuseAlbedo =
        {
            mWeaponBaseDiffuseAlbedo.x * 1.10f,
            mWeaponBaseDiffuseAlbedo.y * 1.10f,
            mWeaponBaseDiffuseAlbedo.z * 1.10f,
            mWeaponBaseDiffuseAlbedo.w
        };
        glowMaterial->FresnelR0 =
        {
            0.18f + outlineColor.x * 0.70f,
            0.18f + outlineColor.y * 0.70f,
            0.18f + outlineColor.z * 0.70f
        };
        glowMaterial->Roughness = 0.05f;
        glowMaterial->OutlineThickness = (std::max)(mWeaponBaseOutlineThickness, 0.028f);
        glowMaterial->OutlineColor = outlineColor;
        glowMaterial->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::UpdateWeaponSkillGlow(float dt)
{
    auto* weaponObject = (mGame != nullptr) ? mGame->GetPlayerWeaponObject() : nullptr;
    auto* weaponRitem = weaponObject != nullptr ? weaponObject->Ritem : nullptr;
    if (weaponRitem == nullptr)
    {
        mWeaponGlowTimer = 0.0f;
        mWeaponGlowDuration = 0.0f;
        return;
    }

    if (mWeaponGlowTimer <= 0.0f || mWeaponGlowDuration <= 0.0f)
    {
        ClearWeaponSkillGlow();
        return;
    }

    Material* glowMaterial = EnsureWeaponGlowMaterial();
    if (glowMaterial == nullptr)
    {
        return;
    }

    mWeaponGlowTimer = (std::max)(0.0f, mWeaponGlowTimer - dt);
    const float progress = 1.0f - (mWeaponGlowTimer / (std::max)(mWeaponGlowDuration, 0.0001f));
    const float fadeStart = 0.82f;
    const float fadeFactor = progress < fadeStart
        ? 1.0f
        : 1.0f - ((progress - fadeStart) / (1.0f - fadeStart));
    const float pulse = 0.86f + 0.14f * std::sin(progress * XM_2PI * 6.0f);
    const float glowStrength = (std::clamp)(fadeFactor * pulse, 0.0f, 1.0f);
    const XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    weaponRitem->ColorMultiplier = Lerp4(baseColor, mWeaponGlowColor, glowStrength);
    weaponRitem->NumFramesDirty = gNumFrameResources;

    if (glowMaterial != nullptr)
    {
        const XMFLOAT4 glowOutline =
        {
            (std::min)(mWeaponGlowColor.x, 1.0f),
            (std::min)(mWeaponGlowColor.y, 1.0f),
            (std::min)(mWeaponGlowColor.z, 1.0f),
            1.0f
        };

        glowMaterial->DiffuseAlbedo =
        {
            mWeaponBaseDiffuseAlbedo.x * (1.0f + 0.24f * glowStrength),
            mWeaponBaseDiffuseAlbedo.y * (1.0f + 0.24f * glowStrength),
            mWeaponBaseDiffuseAlbedo.z * (1.0f + 0.24f * glowStrength),
            mWeaponBaseDiffuseAlbedo.w
        };
        glowMaterial->FresnelR0 =
        {
            mWeaponBaseFresnelR0.x + (0.16f + glowOutline.x * 0.78f - mWeaponBaseFresnelR0.x) * glowStrength,
            mWeaponBaseFresnelR0.y + (0.16f + glowOutline.y * 0.78f - mWeaponBaseFresnelR0.y) * glowStrength,
            mWeaponBaseFresnelR0.z + (0.16f + glowOutline.z * 0.78f - mWeaponBaseFresnelR0.z) * glowStrength
        };
        glowMaterial->Roughness = mWeaponBaseRoughness + (0.05f - mWeaponBaseRoughness) * glowStrength;
        glowMaterial->OutlineThickness =
            mWeaponBaseOutlineThickness +
            ((std::max)(mWeaponBaseOutlineThickness, 0.028f) - mWeaponBaseOutlineThickness) * glowStrength;
        glowMaterial->OutlineColor = Lerp4(mWeaponBaseOutlineColor, glowOutline, glowStrength);
        glowMaterial->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::ClearWeaponSkillGlow()
{
    mWeaponGlowTimer = 0.0f;
    mWeaponGlowDuration = 0.0f;
    mWeaponGlowColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    auto* weaponObject = (mGame != nullptr) ? mGame->GetPlayerWeaponObject() : nullptr;
    auto* weaponRitem = weaponObject != nullptr ? weaponObject->Ritem : nullptr;
    if (weaponRitem != nullptr)
    {
        weaponRitem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
        weaponRitem->NumFramesDirty = gNumFrameResources;
    }

    if (mWeaponGlowMaterial != nullptr)
    {
        mWeaponGlowMaterial->DiffuseAlbedo = mWeaponBaseDiffuseAlbedo;
        mWeaponGlowMaterial->FresnelR0 = mWeaponBaseFresnelR0;
        mWeaponGlowMaterial->Roughness = mWeaponBaseRoughness;
        mWeaponGlowMaterial->OutlineThickness = mWeaponBaseOutlineThickness;
        mWeaponGlowMaterial->OutlineColor = mWeaponBaseOutlineColor;
        mWeaponGlowMaterial->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::SpawnArcherSlashBurst(const XMFLOAT3& position, float rotY, float intensity, float scaleMultiplier)
{
    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT3 right = RightFromYaw(rotY);
    const float scale = (std::max)(scaleMultiplier, 0.1f);
    const XMFLOAT4 slashColor =
    {
        1.10f * intensity,
        1.48f * intensity,
        1.24f * intensity,
        0.92f
    };
    const XMFLOAT4 slashFade =
    {
        0.18f * intensity,
        0.42f * intensity,
        0.28f * intensity,
        0.0f
    };

    SpawnArcherWindRibbon(
        { position.x, position.y + 0.02f * scale, position.z },
        { forward.x * 0.28f, 0.14f, forward.z * 0.28f },
        0.58f * scale,
        1.28f * scale,
        0.72f * scale,
        1.48f * scale,
        0.12f,
        slashColor,
        slashFade,
        0.00f,
        0.16f,
        mArcherSlashMaterial);
    SpawnArcherWindRibbon(
        { position.x + right.x * 0.07f * scale, position.y - 0.02f * scale, position.z + right.z * 0.07f * scale },
        { forward.x * 0.20f + right.x * 0.12f, 0.12f, forward.z * 0.20f + right.z * 0.12f },
        0.52f * scale,
        1.16f * scale,
        0.66f * scale,
        1.34f * scale,
        0.13f,
        slashColor,
        slashFade,
        0.26f,
        -0.10f,
        mArcherSlashMaterial);
    SpawnArcherWindRibbon(
        { position.x - right.x * 0.07f * scale, position.y + 0.04f * scale, position.z - right.z * 0.07f * scale },
        { forward.x * 0.18f - right.x * 0.12f, 0.16f, forward.z * 0.18f - right.z * 0.12f },
        0.50f * scale,
        1.10f * scale,
        0.62f * scale,
        1.30f * scale,
        0.11f,
        slashColor,
        slashFade,
        -0.26f,
        0.20f,
        mArcherSlashMaterial);
}

void SkillEffectManager::SpawnArcherDustBurst(const XMFLOAT3& position, float rotY, float intensity, float scaleMultiplier)
{
    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT3 right = RightFromYaw(rotY);
    const float scale = (std::max)(scaleMultiplier, 0.1f);
    const XMFLOAT4 dustColor =
    {
        0.82f * intensity,
        1.08f * intensity,
        0.94f * intensity,
        0.72f
    };
    const XMFLOAT4 dustFade =
    {
        0.08f * intensity,
        0.18f * intensity,
        0.14f * intensity,
        0.0f
    };

    SpawnArcherWindRibbon(
        { position.x + right.x * 0.10f * scale, position.y + 0.08f, position.z + right.z * 0.10f * scale },
        { right.x * 0.12f + forward.x * 0.08f, 0.10f, right.z * 0.12f + forward.z * 0.08f },
        0.34f * scale,
        0.34f * scale,
        0.48f * scale,
        0.48f * scale,
        0.14f,
        dustColor,
        dustFade,
        0.18f,
        0.0f,
        mArcherDustMaterial);
    SpawnArcherWindRibbon(
        { position.x - right.x * 0.10f * scale, position.y + 0.08f, position.z - right.z * 0.10f * scale },
        { -right.x * 0.12f + forward.x * 0.06f, 0.10f, -right.z * 0.12f + forward.z * 0.06f },
        0.30f * scale,
        0.30f * scale,
        0.44f * scale,
        0.44f * scale,
        0.14f,
        dustColor,
        dustFade,
        -0.18f,
        0.0f,
        mArcherDustMaterial);
}

Material* SkillEffectManager::EnsureWeaponGlowMaterial()
{
    auto* resources = (mGame != nullptr) ? mGame->GetResources() : nullptr;
    auto* weaponObject = (mGame != nullptr) ? mGame->GetPlayerWeaponObject() : nullptr;
    auto* weaponRitem = weaponObject != nullptr ? weaponObject->Ritem : nullptr;
    if (resources == nullptr || weaponRitem == nullptr || weaponRitem->Mat == nullptr)
    {
        return nullptr;
    }

    if (mWeaponGlowMaterial != nullptr &&
        mWeaponGlowOwnerRitem == weaponRitem &&
        weaponRitem->Mat == mWeaponGlowMaterial)
    {
        return mWeaponGlowMaterial;
    }

    Material* sourceMaterial = weaponRitem->Mat;
    if (mWeaponGlowOwnerRitem == weaponRitem &&
        mWeaponGlowMaterial != nullptr &&
        weaponRitem->Mat == mWeaponGlowMaterial &&
        mWeaponGlowBaseMaterial != nullptr)
    {
        sourceMaterial = mWeaponGlowBaseMaterial;
    }

    const std::string glowMaterialName = "LocalWeaponGlowMat_" + std::to_string(weaponRitem->ObjCBIndex);
    Material* glowMaterial = resources->GetMaterial(glowMaterialName);
    if (glowMaterial == nullptr)
    {
        auto newMaterial = std::make_unique<Material>(*sourceMaterial);
        newMaterial->Name = glowMaterialName;
        newMaterial->MatCBIndex = static_cast<int>(resources->mMaterials.size());
        auto insertResult = resources->mMaterials.emplace(glowMaterialName, std::move(newMaterial));
        glowMaterial = insertResult.first->second.get();
    }
    else
    {
        const int cloneIndex = glowMaterial->MatCBIndex;
        *glowMaterial = *sourceMaterial;
        glowMaterial->Name = glowMaterialName;
        glowMaterial->MatCBIndex = cloneIndex;
    }

    mWeaponGlowOwnerRitem = weaponRitem;
    mWeaponGlowBaseMaterial = sourceMaterial;
    mWeaponGlowMaterial = glowMaterial;
    mWeaponBaseDiffuseAlbedo = sourceMaterial->DiffuseAlbedo;
    mWeaponBaseFresnelR0 = sourceMaterial->FresnelR0;
    mWeaponBaseRoughness = sourceMaterial->Roughness;
    mWeaponBaseOutlineThickness = sourceMaterial->OutlineThickness;
    mWeaponBaseOutlineColor = sourceMaterial->OutlineColor;

    weaponRitem->Mat = glowMaterial;
    weaponRitem->NumFramesDirty = gNumFrameResources;
    glowMaterial->NumFramesDirty = gNumFrameResources;
    return glowMaterial;
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

void SkillEffectManager::SpawnArcherWindRibbon(
    const XMFLOAT3& position,
    const XMFLOAT3& velocity,
    float startWidth,
    float startHeight,
    float endWidth,
    float endHeight,
    float lifeTime,
    const XMFLOAT4& startColor,
    const XMFLOAT4& endColor,
    float yawOffset,
    float rollOffset,
    Material* materialOverride)
{
    EffectInstance* effect = AcquireEffect(EffectStyle::ArcherWindRibbon);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    effect->Style = EffectStyle::ArcherWindRibbon;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = (std::max)(lifeTime, 0.04f);
    effect->BasePosition = position;
    effect->Velocity = velocity;
    effect->StartScale = { startWidth, startHeight, 1.0f };
    effect->EndScale = { endWidth, endHeight, 1.0f };
    effect->StartColor = startColor;
    effect->EndColor = endColor;
    effect->RotX = 0.0f;
    effect->RotY = yawOffset;
    effect->RotZ = rollOffset;

    effect->Object->mIsBillboard = false;
    effect->Object->mIsAnimated = false;
    effect->Object->SetPosition(position.x, position.y, position.z);
    effect->Object->SetScale(startWidth, startHeight, 1.0f);
    effect->Object->SetRotation(0.0f, yawOffset, rollOffset);
    effect->Object->Update();

    effect->Ritem->Mat = materialOverride != nullptr
        ? materialOverride
        : (mArcherWindMaterial != nullptr
            ? mArcherWindMaterial
            : (mBeamMaterial != nullptr ? mBeamMaterial : mDecalMaterial));
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

void SkillEffectManager::SpawnArcherArrowRainArrow(
    const XMFLOAT3& targetPosition,
    float yaw,
    float startDelay,
    float fallDuration,
    float uniformScale,
    float spawnHeight)
{
    EnsureArcherArrowRainPool();

    EffectInstance* effect = AcquireEffect(EffectStyle::ArrowRainArrow);
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
    const XMFLOAT3 impactPosition =
    {
        targetPosition.x,
        targetPosition.y + 0.34f,
        targetPosition.z
    };
    const float arrowScale = (std::max)(uniformScale, 0.05f);

    effect->Style = EffectStyle::ArrowRainArrow;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = (std::max)(startDelay + fallDuration + kArcherArrowRainPostImpactLife, 0.10f);
    effect->BasePosition = startPosition;
    effect->TargetPosition = impactPosition;
    effect->Velocity = { 0.0f, 0.0f, 0.0f };
    effect->StartScale = { arrowScale, arrowScale, arrowScale };
    effect->EndScale = effect->StartScale;
    effect->StartColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    effect->EndColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    effect->RotX = -XM_PIDIV2;
    effect->RotY = yaw;
    effect->RotZ = 0.0f;
    effect->StartDelay = (std::max)(startDelay, 0.0f);
    effect->MotionDuration = (std::max)(fallDuration, 0.05f);
    effect->FadeStartTime = effect->StartDelay + effect->MotionDuration;
    effect->UseLinearMotion = false;

    effect->Object->mIsBillboard = false;
    effect->Object->mIsAnimated = false;
    effect->Object->SetPosition(startPosition.x, startPosition.y, startPosition.z);
    effect->Object->SetScale(arrowScale, arrowScale, arrowScale);
    effect->Object->SetRotation(effect->RotX, effect->RotY, effect->RotZ);
    effect->Object->Update();

    effect->Ritem->Mat = mArcherArrowMaterial != nullptr
        ? mArcherArrowMaterial
        : effect->Ritem->Mat;
    effect->Ritem->Visible = effect->StartDelay <= 0.0001f;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, effect->Ritem->Visible ? 1.0f : 0.0f };
    effect->Ritem->NumFramesDirty = gNumFrameResources;
}
