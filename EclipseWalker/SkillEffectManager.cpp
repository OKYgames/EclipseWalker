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
    constexpr int kGroundPoolSize = 44;
    constexpr int kBeamPoolSize = 26;
    constexpr int kMageBasicOrbPoolSize = 104;
    constexpr int kMageBasicOrbCorePoolSize = 16;
    constexpr int kArcherWindRibbonPoolSize = 96;
    constexpr int kWarriorSwordTrailSegmentPoolSize = 42;
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
    constexpr float kArcherBasicArrowSpeed = 20.0f;
    constexpr float kArcherBasicArrowMinDistance = 3.0f;
    constexpr float kArcherBasicArrowMaxDistance = 30.0f;
    constexpr float kMageBasicOrbStartForwardOffset = 0.48f;
    constexpr float kMageBasicOrbStartHeight = 0.92f;
    constexpr float kMageBasicOrbStartRightOffset = 0.10f;
    constexpr float kMageBasicOrbSpeed = 8.0f;
    constexpr float kMageBasicOrbMinDistance = 2.5f;
    constexpr float kMageBasicOrbMaxDistance = 18.0f;
    constexpr float kMageMeteorSpawnHeight = 8.8f;
    constexpr float kMageMeteorMinFallDuration = 0.50f;
    constexpr float kMageMeteorImpactBurstLife = 0.26f;
    constexpr float kMageMeteorSpriteFadeOutDuration = 0.10f;
    constexpr float kMageMeteorRingFadeOutDuration = 0.16f;
    constexpr int kMageMeteorShardCount = 12;

    constexpr float kWarriorSwordTrailAttack1StartDelay = 0.3f;
    constexpr float kWarriorSwordTrailAttack1EmitDuration = 0.30f;
    constexpr float kWarriorSwordTrailAttack2StartDelay = 0.4f;
    constexpr float kWarriorSwordTrailAttack2EmitDuration = 0.30f;
    constexpr float kWarriorSwordTrailEmitInterval = 0.15f;
    constexpr float kWarriorSwordTrailSegmentLifeTime = 0.3f;
    constexpr float kWarriorSwordTrailSegmentMinLength = 0.025f;
    constexpr float kWarriorSwordTrailStartLengthPadding = 0.05f;
    constexpr float kWarriorSwordTrailEndLengthPadding = 1.5f;

    constexpr float kWarriorSwordTrailStartBladeSpan = 0.4f;
    constexpr float kWarriorSwordTrailEndBladeSpan = 0.9f;

    constexpr float kWarriorSwordTrailVerticalOffset = 0.00f;

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

    XMFLOAT3 Subtract3(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    float LengthSq3(const XMFLOAT3& v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    XMFLOAT3 NormalizeOr(const XMFLOAT3& v, const XMFLOAT3& fallback)
    {
        const float lengthSq = LengthSq3(v);
        if (lengthSq <= 0.000001f)
        {
            return fallback;
        }

        const float invLength = 1.0f / std::sqrt(lengthSq);
        return { v.x * invLength, v.y * invLength, v.z * invLength };
    }

    XMFLOAT3 Cross3(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return
        {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    XMFLOAT4 WarriorSwordTrailStartColor(ClassTier weaponTier, float emitT)
    {
        const float alpha = 0.90f * (1.0f - emitT * 0.20f);
        switch (weaponTier)
        {
        case ClassTier::Tier2:
            return { 1.42f, 0.72f, 2.35f, alpha };
        case ClassTier::Tier3:
            return { 2.85f, 0.72f, 0.52f, alpha };
        case ClassTier::Tier1:
        default:
            return { 2.15f, 1.84f, 1.14f, alpha };
        }
    }

    XMFLOAT4 WarriorSwordTrailEndColor(ClassTier weaponTier)
    {
        switch (weaponTier)
        {
        case ClassTier::Tier2:
            return { 0.82f, 0.30f, 1.50f, 0.0f };
        case ClassTier::Tier3:
            return { 1.90f, 0.36f, 0.24f, 0.0f };
        case ClassTier::Tier1:
        default:
            return { 1.38f, 0.96f, 0.52f, 0.0f };
        }
    }

    XMFLOAT3 TransformPoint(const XMFLOAT3& point, const XMFLOAT4X4& transform)
    {
        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3TransformCoord(XMLoadFloat3(&point), XMLoadFloat4x4(&transform)));
        return result;
    }

    bool TryResolveBladeTrailLocalPoints(const RenderItem* renderItem, XMFLOAT3& outTipLocal, XMFLOAT3& outInnerLocal)
    {
        if (renderItem == nullptr || renderItem->Geo == nullptr)
        {
            return false;
        }

        const SubmeshGeometry* submesh = nullptr;
        auto meshIt = renderItem->Geo->DrawArgs.find("mesh");
        if (meshIt != renderItem->Geo->DrawArgs.end())
        {
            submesh = &meshIt->second;
        }
        else if (!renderItem->Geo->DrawArgs.empty())
        {
            submesh = &renderItem->Geo->DrawArgs.begin()->second;
        }

        if (submesh == nullptr)
        {
            return false;
        }

        const XMFLOAT3 center = submesh->Bounds.Center;
        const XMFLOAT3 extents = submesh->Bounds.Extents;
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

        const float minCoord = mins[dominantAxis];
        const float maxCoord = maxs[dominantAxis];
        const float tipCoord = std::fabs(maxCoord) >= std::fabs(minCoord) ? maxCoord : minCoord;
        const float centerCoord = dominantAxis == 0 ? center.x : (dominantAxis == 1 ? center.y : center.z);
        const float tipSign = tipCoord >= centerCoord ? 1.0f : -1.0f;

        outTipLocal = center;
        outInnerLocal = center;
        switch (dominantAxis)
        {
        case 0:
            outTipLocal.x = tipCoord + extents.x * 0.36f * tipSign;
            outInnerLocal.x = center.x - extents.x * 0.7f * tipSign;
            break;
        case 1:
            outTipLocal.y = tipCoord + extents.y * 0.36f * tipSign;
            outInnerLocal.y = center.y - extents.y * 0.7f * tipSign;
            break;
        default:
            outTipLocal.z = tipCoord + extents.z * 0.36f * tipSign;
            outInnerLocal.z = center.z - extents.z * 0.7f * tipSign;
            break;
        }

        return true;
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
    mWarriorSwordTrailElapsed = 0.0f;
    mWarriorSwordTrailTotalDuration = 0.0f;
    mWarriorSwordTrailEmitTimer = 0.0f;
    mWarriorSwordTrailVariant = 1;
    mWarriorSwordTrailEmitAnchorValid = false;
    mWarriorSwordTrailSkippedFirstEmit = false;
    mWarriorSwordTrailSpawnedSegment = false;
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
    UpdateWarriorWeaponTrailState();
    UpdateWarriorBasicSwordTrail(dt);
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

        const float displayAge = effect.Age - effect.StartDelay;

        if (displayAge < 0.0f)
        {
            effect.Ritem->Visible = false;
            effect.Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
            effect.Ritem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        const float visibleLife = (std::max)(effect.LifeTime - effect.StartDelay, 0.0001f);
        const float t = (std::clamp)(displayAge / visibleLife, 0.0f, 1.0f);
        const float eased = 1.0f - (1.0f - t) * (1.0f - t);
        XMFLOAT3 currentScale = Lerp3(effect.StartScale, effect.EndScale, eased);
        XMFLOAT4 currentColor = Lerp4(effect.StartColor, effect.EndColor, t);
        XMFLOAT3 currentPosition =
        {
            effect.BasePosition.x + effect.Velocity.x * displayAge,
            effect.BasePosition.y + effect.Velocity.y * displayAge,
            effect.BasePosition.z + effect.Velocity.z * displayAge
        };

        if (effect.FadeStartTime > effect.StartDelay)
        {
            const float solidDuration = (std::max)(effect.FadeStartTime - effect.StartDelay, 0.0001f);
            const float solidT = (std::clamp)(displayAge / solidDuration, 0.0f, 1.0f);
            const float solidEased = 1.0f - (1.0f - solidT) * (1.0f - solidT);
            currentScale = Lerp3(effect.StartScale, effect.EndScale, solidEased);
            currentColor = Lerp4(effect.StartColor, effect.EndColor, solidT);

            if (effect.UseLinearMotion && effect.MotionDuration > 0.0f)
            {
                const float motionT = (std::clamp)(displayAge / effect.MotionDuration, 0.0f, 1.0f);
                currentPosition = Lerp3(effect.BasePosition, effect.TargetPosition, motionT);
            }

            if (effect.Age >= effect.FadeStartTime)
            {
                const float fadeDuration = (std::max)(effect.LifeTime - effect.FadeStartTime, 0.0001f);
                const float fadeT = (std::clamp)((effect.Age - effect.FadeStartTime) / fadeDuration, 0.0f, 1.0f);
                currentColor.w *= 1.0f - fadeT;
            }
        }

        if (effect.Style == EffectStyle::WarriorSwordTrailSegment)
        {
            effect.Ritem->ColorMultiplier = currentColor;
            effect.Ritem->Visible = currentColor.w > 0.001f;
            effect.Ritem->NumFramesDirty = gNumFrameResources;
            continue;
        }

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
        if (effect.Style == EffectStyle::ArcherWindRibbon && effect.UseStyleAnimation)
        {
            const float pulse = 0.98f + 0.10f * std::sin(displayAge * 15.0f + effect.BasePosition.x * 1.7f);
            animatedScale.x *= pulse;
            animatedScale.y *= 1.00f + 0.06f * std::sin(displayAge * 12.0f + effect.BasePosition.z * 1.3f);
            currentColor.w *= 0.94f + 0.08f * std::sin(displayAge * 14.0f + 0.25f);
            currentPosition.x += std::sin(displayAge * 7.5f + effect.BasePosition.y * 3.1f) * 0.025f;
            currentPosition.z += std::cos(displayAge * 8.2f + effect.BasePosition.x * 1.6f) * 0.025f;
        }
        else if (effect.Style == EffectStyle::MageBasicOrb && effect.UseStyleAnimation)
        {
            const float pulse = 0.96f + 0.14f * std::sin(displayAge * 18.0f + effect.BasePosition.x * 2.1f);
            animatedScale.x *= pulse;
            animatedScale.y *= 0.96f + 0.12f * std::cos(displayAge * 16.0f + effect.BasePosition.z * 1.8f);
            currentColor.w *= 0.90f + 0.10f * std::sin(displayAge * 20.0f + 0.35f);
            currentPosition.y += 0.02f * std::sin(displayAge * 10.0f + effect.BasePosition.x * 1.2f);
        }
        else if (effect.Style == EffectStyle::MageBasicOrbCore)
        {
            const float pulse = 0.98f + 0.10f * std::sin(displayAge * 20.0f);
            animatedScale.x *= pulse;
            animatedScale.y *= pulse;
            animatedScale.z *= pulse;
            currentColor.w *= 0.92f + 0.08f * std::cos(displayAge * 22.0f);
            currentPosition.y += 0.016f * std::sin(displayAge * 12.0f);
        }

        effect.Object->SetPosition(currentPosition.x, currentPosition.y, currentPosition.z);
        effect.Object->SetScale(animatedScale.x, animatedScale.y, animatedScale.z);
        if (effect.Style == EffectStyle::ArcherWindRibbon)
        {
            if (effect.UseStyleAnimation)
            {
                const XMFLOAT3 cameraPosition = mGame != nullptr && mGame->GetCamera() != nullptr
                    ? mGame->GetCamera()->GetPosition3f()
                    : XMFLOAT3(0.0f, 0.0f, 1.0f);
                const float dx = cameraPosition.x - currentPosition.x;
                const float dz = cameraPosition.z - currentPosition.z;
                const float cameraFacingYaw = std::atan2(dx, dz);
                const float swayYaw = 0.06f * std::sin(displayAge * 10.0f + effect.BasePosition.y * 2.7f);
                const float swayRoll = 0.04f * std::sin(displayAge * 13.0f + effect.BasePosition.x * 1.9f);
                effect.Object->SetRotation(0.0f, cameraFacingYaw + effect.RotY + swayYaw, effect.RotZ + swayRoll);
            }
            else
            {
                effect.Object->SetRotation(effect.RotX, effect.RotY, effect.RotZ);
            }
        }
        else if (effect.Style == EffectStyle::MageBasicOrbCore)
        {
            effect.Object->SetRotation(displayAge * 9.0f, displayAge * 12.0f, displayAge * 7.0f);
        }
        else if (!effect.Object->mIsBillboard)
        {
            effect.Object->SetRotation(effect.RotX, effect.RotY, effect.RotZ + displayAge * effect.SpinRate);
        }
        effect.Object->Update();

        effect.Ritem->ColorMultiplier = currentColor;
        effect.Ritem->Visible = currentColor.w > 0.001f;
        effect.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void SkillEffectManager::SpawnArcherBuffStartEffect(const XMFLOAT3& origin, float rotY)
{
    (void)origin;
    (void)rotY;
}

void SkillEffectManager::SpawnArcherBuffLoopEffect(const XMFLOAT3& origin, float rotY, float intensity)
{
    (void)origin;
    (void)rotY;
    (void)intensity;
}

void SkillEffectManager::SpawnArcherBuffFrontEffect(const XMFLOAT3& origin, float rotY, float intensity)
{
    (void)origin;
    (void)rotY;
    (void)intensity;
}

void SkillEffectManager::SpawnArcherBuffEndEffect(const XMFLOAT3& origin, float rotY)
{
    (void)origin;
    (void)rotY;
}

void SkillEffectManager::SpawnMageHealingLightEffect(const XMFLOAT3& origin, float startDelay)
{
    Material* sparkleMaterial = mMageHealSparkleMaterial != nullptr
        ? mMageHealSparkleMaterial
        : (mBeamMaterial != nullptr ? mBeamMaterial : mDecalMaterial);
    Material* smokeMaterial = mMageHealSmokeMaterial != nullptr ? mMageHealSmokeMaterial : sparkleMaterial;
    Material* pointMaterial = mMageHealPointMaterial != nullptr ? mMageHealPointMaterial : sparkleMaterial;
    if (sparkleMaterial == nullptr || smokeMaterial == nullptr || pointMaterial == nullptr)
    {
        return;
    }

    constexpr int kSmokeCount = 9;
    constexpr int kTrailCount = 10;
    constexpr int kFlareCount = 6;
    constexpr int kPointCount = 12;
    constexpr int kSparkleCount = 8;
    const XMFLOAT4 smokeStartColor = { 0.56f, 1.10f, 0.84f, 0.56f };
    const XMFLOAT4 smokeEndColor = { 0.24f, 0.54f, 0.38f, 0.0f };
    const XMFLOAT4 trailStartColor = { 0.86f, 1.40f, 2.42f, 0.94f };
    const XMFLOAT4 trailEndColor = { 0.24f, 0.72f, 1.34f, 0.0f };
    const XMFLOAT4 flareStartColor = { 0.92f, 1.50f, 2.56f, 0.90f };
    const XMFLOAT4 flareEndColor = { 0.30f, 0.84f, 1.58f, 0.0f };
    const XMFLOAT4 pointStartColor = { 0.92f, 1.46f, 2.50f, 0.96f };
    const XMFLOAT4 pointEndColor = { 0.26f, 0.74f, 1.44f, 0.0f };
    const XMFLOAT4 startColor = { 0.72f, 1.36f, 2.10f, 1.0f };
    const XMFLOAT4 endColor = { 0.18f, 0.62f, 1.16f, 0.0f };

    const float clampedStartDelay = (std::max)(startDelay, 0.0f);
    const XMFLOAT3 cameraPosition = mGame != nullptr && mGame->GetCamera() != nullptr
        ? mGame->GetCamera()->GetPosition3f()
        : XMFLOAT3(0.0f, 0.0f, 1.0f);
    const float cameraDx = cameraPosition.x - origin.x;
    const float cameraDz = cameraPosition.z - origin.z;
    const float cameraFacingYaw = std::atan2(cameraDx, cameraDz);
    auto spawnBillboardSparkle =
        [this, clampedStartDelay](
            Material* material,
            const XMFLOAT3& position,
            const XMFLOAT3& velocity,
            float startScaleX,
            float startScaleY,
            float endScaleX,
            float endScaleY,
            float lifeTime,
            const XMFLOAT4& startColor,
            const XMFLOAT4& endColor)
    {
        EffectInstance* effect = AcquireEffect(EffectStyle::VerticalBeam);
        if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
        {
            return;
        }

        effect->Style = EffectStyle::VerticalBeam;
        effect->Active = true;
        effect->Age = 0.0f;
        effect->LifeTime = clampedStartDelay + (std::max)(lifeTime, 0.05f);
        effect->BasePosition = position;
        effect->Velocity = velocity;
        effect->StartScale = { startScaleX, startScaleY, 1.0f };
        effect->EndScale = { endScaleX, endScaleY, 1.0f };
        effect->StartColor = startColor;
        effect->EndColor = endColor;
        effect->RotX = 0.0f;
        effect->RotY = 0.0f;
        effect->RotZ = 0.0f;
        effect->StartDelay = clampedStartDelay;
        effect->MotionDuration = 0.0f;
        effect->FadeStartTime = 0.0f;
        effect->UseLinearMotion = false;

        effect->Object->mIsBillboard = true;
        effect->Object->mIsAnimated = false;
        effect->Object->SetPosition(position.x, position.y, position.z);
        effect->Object->SetScale(startScaleX, startScaleY, 1.0f);
        effect->Object->Update();

        effect->Ritem->Mat = material;
        effect->Ritem->Visible = clampedStartDelay <= 0.0f;
        effect->Ritem->CastShadow = false;
        effect->Ritem->ColorMultiplier = startColor;
        effect->Ritem->TexTransform = MathHelper::Identity4x4();
        effect->Ritem->NumFramesDirty = gNumFrameResources;
    };

    auto spawnLiftTrail =
        [this, clampedStartDelay, cameraFacingYaw](
            Material* material,
            const XMFLOAT3& position,
            const XMFLOAT3& velocity,
            float startScaleX,
            float startScaleY,
            float endScaleX,
            float endScaleY,
            float lifeTime,
            const XMFLOAT4& startColor,
            const XMFLOAT4& endColor,
            float rotZ)
    {
        EffectInstance* effect = AcquireEffect(EffectStyle::VerticalBeam);
        if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
        {
            return;
        }

        effect->Style = EffectStyle::VerticalBeam;
        effect->Active = true;
        effect->Age = 0.0f;
        effect->LifeTime = clampedStartDelay + (std::max)(lifeTime, 0.05f);
        effect->BasePosition = position;
        effect->Velocity = velocity;
        effect->StartScale = { startScaleX, startScaleY, 1.0f };
        effect->EndScale = { endScaleX, endScaleY, 1.0f };
        effect->StartColor = startColor;
        effect->EndColor = endColor;
        effect->RotX = 0.0f;
        effect->RotY = cameraFacingYaw;
        effect->RotZ = rotZ;
        effect->StartDelay = clampedStartDelay;
        effect->MotionDuration = 0.0f;
        effect->FadeStartTime = 0.0f;
        effect->UseLinearMotion = false;

        effect->Object->mIsBillboard = false;
        effect->Object->mIsAnimated = false;
        effect->Object->SetPosition(position.x, position.y, position.z);
        effect->Object->SetRotation(0.0f, cameraFacingYaw, rotZ);
        effect->Object->SetScale(startScaleX, startScaleY, 1.0f);
        effect->Object->Update();

        effect->Ritem->Mat = material;
        effect->Ritem->Visible = clampedStartDelay <= 0.0f;
        effect->Ritem->CastShadow = false;
        effect->Ritem->ColorMultiplier = startColor;
        effect->Ritem->TexTransform = MathHelper::Identity4x4();
        effect->Ritem->NumFramesDirty = gNumFrameResources;
    };

    for (int i = 0; i < kSmokeCount; ++i)
    {
        const float angle = MathHelper::RandF(0.0f, XM_2PI);
        const float radius = MathHelper::RandF(0.04f, 0.38f);
        const float height = MathHelper::RandF(0.12f, 0.82f);
        const float lateralDrift = MathHelper::RandF(-0.05f, 0.05f);
        const float verticalSpeed = MathHelper::RandF(0.03f, 0.07f);
        const float startWidth = MathHelper::RandF(1.08f, 1.54f);
        const float startHeight = startWidth * MathHelper::RandF(1.04f, 1.22f);
        const float endWidth = startWidth * MathHelper::RandF(1.72f, 2.18f);
        const float endHeight = startHeight * MathHelper::RandF(1.64f, 2.06f);
        const XMFLOAT3 radial = { std::cos(angle), 0.0f, std::sin(angle) };
        const XMFLOAT3 position =
        {
            origin.x + radial.x * radius,
            origin.y + height,
            origin.z + radial.z * radius
        };
        const XMFLOAT3 velocity =
        {
            radial.x * lateralDrift,
            verticalSpeed,
            radial.z * lateralDrift
        };

        spawnBillboardSparkle(
            smokeMaterial,
            position,
            velocity,
            startWidth,
            startHeight,
            endWidth,
            endHeight,
            MathHelper::RandF(1.25f, 1.72f),
            smokeStartColor,
            smokeEndColor);
    }

    for (int i = 0; i < kTrailCount; ++i)
    {
        const float angle = MathHelper::RandF(0.0f, XM_2PI);
        const float radius = MathHelper::RandF(0.10f, 0.34f);
        const float height = MathHelper::RandF(0.06f, 0.18f);
        const float outwardSpeed = MathHelper::RandF(0.01f, 0.04f);
        const float verticalSpeed = MathHelper::RandF(0.44f, 0.92f);
        const float startWidth = MathHelper::RandF(0.020f, 0.028f);
        const float startHeight = MathHelper::RandF(0.34f, 0.56f);
        const float endWidth = startWidth * MathHelper::RandF(0.70f, 0.90f);
        const float endHeight = startHeight * MathHelper::RandF(0.62f, 0.82f);
        const XMFLOAT3 radial = { std::cos(angle), 0.0f, std::sin(angle) };
        const XMFLOAT3 position =
        {
            origin.x + radial.x * radius,
            origin.y + height,
            origin.z + radial.z * radius
        };
        const XMFLOAT3 velocity =
        {
            radial.x * outwardSpeed,
            verticalSpeed,
            radial.z * outwardSpeed
        };

        spawnLiftTrail(
            pointMaterial,
            position,
            velocity,
            startWidth,
            startHeight,
            endWidth,
            endHeight,
            MathHelper::RandF(0.50f, 0.86f),
            trailStartColor,
            trailEndColor,
            0.0f);
    }

    for (int i = 0; i < kFlareCount; ++i)
    {
        const float angle = MathHelper::RandF(0.0f, XM_2PI);
        const float radius = MathHelper::RandF(0.12f, 0.30f);
        const float height = MathHelper::RandF(0.08f, 0.22f);
        const float outwardSpeed = MathHelper::RandF(0.01f, 0.04f);
        const float verticalSpeed = MathHelper::RandF(0.38f, 0.74f);
        const float startWidth = MathHelper::RandF(0.12f, 0.18f);
        const float startHeight = MathHelper::RandF(0.28f, 0.42f);
        const float endWidth = startWidth * MathHelper::RandF(0.82f, 0.96f);
        const float endHeight = startHeight * MathHelper::RandF(0.82f, 0.96f);
        const XMFLOAT3 radial = { std::cos(angle), 0.0f, std::sin(angle) };
        const XMFLOAT3 position =
        {
            origin.x + radial.x * radius,
            origin.y + height,
            origin.z + radial.z * radius
        };
        const XMFLOAT3 velocity =
        {
            radial.x * outwardSpeed,
            verticalSpeed,
            radial.z * outwardSpeed
        };

        spawnLiftTrail(
            sparkleMaterial,
            position,
            velocity,
            startWidth,
            startHeight,
            endWidth,
            endHeight,
            MathHelper::RandF(0.56f, 0.94f),
            flareStartColor,
            flareEndColor,
            0.0f);
    }

    for (int i = 0; i < kPointCount; ++i)
    {
        const float angle = MathHelper::RandF(0.0f, XM_2PI);
        const float radius = MathHelper::RandF(0.02f, 0.20f);
        const float height = MathHelper::RandF(0.04f, 0.26f);
        const float outwardSpeed = MathHelper::RandF(0.01f, 0.05f);
        const float verticalSpeed = MathHelper::RandF(0.34f, 0.78f);
        const float startWidth = MathHelper::RandF(0.08f, 0.16f);
        const float startHeight = startWidth * MathHelper::RandF(1.18f, 1.86f);
        const float endWidth = startWidth * MathHelper::RandF(0.52f, 0.80f);
        const float endHeight = startHeight * MathHelper::RandF(0.82f, 1.24f);
        const XMFLOAT3 radial = { std::cos(angle), 0.0f, std::sin(angle) };
        const XMFLOAT3 position =
        {
            origin.x + radial.x * radius,
            origin.y + height,
            origin.z + radial.z * radius
        };
        const XMFLOAT3 velocity =
        {
            radial.x * outwardSpeed,
            verticalSpeed,
            radial.z * outwardSpeed
        };

        spawnBillboardSparkle(
            pointMaterial,
            position,
            velocity,
            startWidth,
            startHeight,
            endWidth,
            endHeight,
            MathHelper::RandF(0.52f, 0.88f),
            pointStartColor,
            pointEndColor);
    }

    for (int i = 0; i < kSparkleCount; ++i)
    {
        const float angle = MathHelper::RandF(0.0f, XM_2PI);
        const float radius = MathHelper::RandF(0.03f, 0.22f);
        const float height = MathHelper::RandF(0.10f, 0.82f);
        const float verticalSpeed = MathHelper::RandF(0.05f, 0.10f);
        const float startWidth = MathHelper::RandF(0.18f, 0.26f);
        const float startHeight = startWidth * MathHelper::RandF(1.50f, 1.72f);
        const float endWidth = startWidth * MathHelper::RandF(0.52f, 0.70f);
        const float endHeight = startHeight * MathHelper::RandF(0.52f, 0.70f);
        const XMFLOAT3 position =
        {
            origin.x + std::cos(angle) * radius,
            origin.y + height,
            origin.z + std::sin(angle) * radius
        };
        const XMFLOAT3 velocity = { 0.0f, verticalSpeed, 0.0f };

        spawnBillboardSparkle(
            sparkleMaterial,
            position,
            velocity,
            startWidth,
            startHeight,
            endWidth,
            endHeight,
            MathHelper::RandF(0.55f, 0.92f),
            startColor,
            endColor);
    }
}

void SkillEffectManager::SpawnMageMeteorFlameSprite(
    const XMFLOAT3& startPosition,
    const XMFLOAT3& endPosition,
    float visibleDuration,
    float startDelay,
    float startScaleX,
    float startScaleY,
    float endScaleX,
    float endScaleY,
    const XMFLOAT4& startColor,
    const XMFLOAT4& endColor,
    Material* material,
    bool billboard,
    float rotY,
    float fadeOutDuration)
{
    EnsureResources();
    EnsurePool();

    EffectInstance* effect = AcquireEffect(EffectStyle::MageBasicOrb);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    const float clampedDuration = (std::max)(visibleDuration, 0.06f);
    const float clampedStartDelay = (std::max)(startDelay, 0.0f);
    const float resolvedFadeOutDuration = fadeOutDuration >= 0.0f
        ? fadeOutDuration
        : kMageMeteorSpriteFadeOutDuration;
    const XMFLOAT3 velocity =
    {
        (endPosition.x - startPosition.x) / clampedDuration,
        (endPosition.y - startPosition.y) / clampedDuration,
        (endPosition.z - startPosition.z) / clampedDuration
    };

    effect->Style = EffectStyle::MageBasicOrb;
    effect->Active = true;
    effect->Age = 0.0f;
    effect->LifeTime = clampedStartDelay + clampedDuration + resolvedFadeOutDuration;
    effect->BasePosition = startPosition;
    effect->TargetPosition = endPosition;
    effect->Velocity = velocity;
    effect->StartScale = { startScaleX, startScaleY, 1.0f };
    effect->EndScale = { endScaleX, endScaleY, 1.0f };
    effect->StartColor = startColor;
    effect->EndColor = endColor;
    effect->RotX = 0.0f;
    effect->RotY = rotY;
    effect->RotZ = billboard ? 0.0f : -XM_PIDIV2;
    effect->StartDelay = clampedStartDelay;
    effect->MotionDuration = clampedDuration;
    effect->FadeStartTime = clampedStartDelay + clampedDuration;
    effect->UseLinearMotion = true;
    effect->UseStyleAnimation = false;

    effect->Object->mIsBillboard = billboard;
    effect->Object->mIsAnimated = false;
    effect->Object->SetPosition(startPosition.x, startPosition.y, startPosition.z);
    effect->Object->SetScale(startScaleX, startScaleY, 1.0f);
    if (!billboard)
    {
        effect->Object->SetRotation(0.0f, rotY, -XM_PIDIV2);
    }
    effect->Object->Update();

    effect->Ritem->Mat = material != nullptr
        ? material
        : (mMageMeteorFlameMaterials[0] != nullptr ? mMageMeteorFlameMaterials[0] : effect->Ritem->Mat);
    effect->Ritem->Visible = clampedStartDelay <= 0.0001f;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = startColor;
    effect->Ritem->TexTransform = MathHelper::Identity4x4();
    if (!effect->Ritem->Visible)
    {
        effect->Ritem->ColorMultiplier.w = 0.0f;
    }
    effect->Ritem->NumFramesDirty = gNumFrameResources;
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
    const XMFLOAT3 cameraPosition = mGame != nullptr && mGame->GetCamera() != nullptr
        ? mGame->GetCamera()->GetPosition3f()
        : XMFLOAT3(0.0f, 0.0f, 1.0f);
    const float dx = cameraPosition.x - groundPosition.x;
    const float dz = cameraPosition.z - groundPosition.z;
    const float cameraFacingYaw = std::atan2(dx, dz);

    auto applyLoopSprite =
        [this, cameraFacingYaw](
            GameObject* object,
            RenderItem* ritem,
            Material* material,
            const XMFLOAT3& position,
            float scaleX,
            float scaleY,
            float rotZ,
            const XMFLOAT4& color)
        {
            if (object == nullptr || ritem == nullptr)
            {
                return;
            }

            object->SetPosition(position.x, position.y, position.z);
            object->SetRotation(0.0f, cameraFacingYaw, rotZ);
            object->SetScale(scaleX, scaleY, 1.0f);
            object->Update();

            ritem->Mat = material != nullptr ? material : ritem->Mat;
            ritem->ColorMultiplier = color;
            ritem->Visible = color.w > 0.001f;
            ritem->NumFramesDirty = gNumFrameResources;
        };

    auto configureTrail =
        [&](GameObject* object, RenderItem* ritem, float laneSeed, float progressOffset, float radiusBase)
        {
            float progress = std::fmod(mArcherHasteAuraPulseTimer * 1.24f + progressOffset, 1.0f);
            if (progress < 0.0f)
            {
                progress += 1.0f;
            }

            const float laneAngle = rotY + laneSeed * 0.82f + 0.12f * std::sin(mArcherHasteAuraPulseTimer * 1.8f + laneSeed);
            const float laneRadius = radiusBase + 0.07f * std::sin(mArcherHasteAuraPulseTimer * 2.2f + laneSeed * 1.3f);
            const float riseHeight = 0.08f + progress * (1.18f + 0.12f * std::cos(laneSeed));
            const float width = 0.020f + 0.006f * (0.5f + 0.5f * std::sin(mArcherHasteAuraPulseTimer * 3.2f + laneSeed));
            const float height = 0.34f + 0.58f * (0.65f + 0.35f * (1.0f - progress));
            const float alpha = (0.20f + 0.72f * (1.0f - std::fabs(progress * 2.0f - 1.0f))) * intensity;
            const XMFLOAT3 radial = { std::cos(laneAngle), 0.0f, std::sin(laneAngle) };
            const XMFLOAT3 position =
            {
                groundPosition.x + radial.x * laneRadius,
                groundPosition.y + riseHeight,
                groundPosition.z + radial.z * laneRadius
            };

            applyLoopSprite(
                object,
                ritem,
                mArcherBuffPointMaterial != nullptr ? mArcherBuffPointMaterial : mArcherColumnMaterial,
                position,
                width,
                height,
                0.0f,
                { 1.18f * intensity, 1.62f * intensity, 1.34f * intensity, alpha });
        };

    auto configureArrow =
        [&](GameObject* object, RenderItem* ritem, float laneSeed, float progressOffset, float radiusBase)
        {
            float progress = std::fmod(mArcherHasteAuraPulseTimer * 1.18f + progressOffset, 1.0f);
            if (progress < 0.0f)
            {
                progress += 1.0f;
            }

            const float laneAngle = rotY + laneSeed * 0.78f + 0.10f * std::cos(mArcherHasteAuraPulseTimer * 1.6f + laneSeed * 0.9f);
            const float laneRadius = radiusBase + 0.05f * std::cos(mArcherHasteAuraPulseTimer * 2.0f + laneSeed);
            const float riseHeight = 0.14f + progress * (1.06f + 0.16f * std::sin(laneSeed));
            const float scaleX = 0.16f + 0.03f * (0.5f + 0.5f * std::sin(mArcherHasteAuraPulseTimer * 2.4f + laneSeed));
            const float scaleY = 0.10f + 0.02f * (0.5f + 0.5f * std::cos(mArcherHasteAuraPulseTimer * 2.8f + laneSeed));
            const float alpha = (0.32f + 0.60f * (1.0f - std::fabs(progress * 2.0f - 1.0f))) * intensity;
            const XMFLOAT3 radial = { std::cos(laneAngle), 0.0f, std::sin(laneAngle) };
            const XMFLOAT3 position =
            {
                groundPosition.x + radial.x * laneRadius,
                groundPosition.y + riseHeight,
                groundPosition.z + radial.z * laneRadius
            };

            applyLoopSprite(
                object,
                ritem,
                mArcherBuffArrowMaterial != nullptr ? mArcherBuffArrowMaterial : mArcherBuffPointMaterial,
                position,
                scaleX,
                scaleY,
                -XM_PIDIV2,
                { 1.26f * intensity, 1.76f * intensity, 1.42f * intensity, alpha });
        };

    configureTrail(mArcherBuffLoopOuterObject, mArcherBuffLoopOuterRitem, 0.4f, 0.08f, 0.18f);
    configureTrail(mArcherBuffLoopInnerObject, mArcherBuffLoopInnerRitem, 1.7f, 0.38f, 0.30f);

    for (size_t i = 0; i < mArcherBuffLoopFlowObjects.size() && i < mArcherBuffLoopFlowRitems.size(); ++i)
    {
        GameObject* flowObject = mArcherBuffLoopFlowObjects[i];
        RenderItem* flowRitem = mArcherBuffLoopFlowRitems[i];
        if (flowObject == nullptr || flowRitem == nullptr)
        {
            continue;
        }

        const float laneSeed = static_cast<float>(i) * 0.86f + 0.9f;
        const float progressOffset = static_cast<float>(i) / (std::max)(1.0f, static_cast<float>(mArcherBuffLoopFlowObjects.size()));
        const float radiusBase = 0.18f + 0.08f * static_cast<float>(i % 3) + 0.03f * static_cast<float>(i / 3);

        if ((i % 2) == 0)
        {
            configureTrail(flowObject, flowRitem, laneSeed, progressOffset, radiusBase);
        }
        else
        {
            configureArrow(flowObject, flowRitem, laneSeed, progressOffset, radiusBase + 0.08f);
        }
    }
}

void SkillEffectManager::OnSkillCast(
    PlayerClass playerClass,
    int skillIndex,
    const XMFLOAT3& origin,
    float rotY,
    float activeDuration,
    float startDelay)
{
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
            SpawnMageHealingLightEffect(origin, startDelay);
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
    float effectRadius,
    float startDelay)
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

    if (playerClass == PlayerClass::Mage && skillIndex == 2)
    {
        OnSkillResolved(playerClass, skillIndex, impactCenter, rotY, effectRadius);
        return;
    }

    OnSkillCast(playerClass, skillIndex, origin, rotY, 0.55f, startDelay);
}

void SkillEffectManager::OnSkillImpact(PlayerClass playerClass, int skillIndex, const XMFLOAT3& hitPosition)
{
    (void)playerClass;
    (void)skillIndex;
    (void)hitPosition;
}

void SkillEffectManager::OnArcherHasteBasicShot(
    const XMFLOAT3& origin,
    float rotY,
    float travelDistance,
    float startDelay,
    float intensity)
{
    EnsureResources();

    const float effectIntensity = (std::max)(intensity, 1.0f);
    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT3 right = RightFromYaw(rotY);
    const float clampedDistance = (std::clamp)(
        travelDistance,
        kArcherBasicArrowMinDistance,
        kArcherBasicArrowMaxDistance);
    const float clampedStartDelay = (std::max)(startDelay, 0.0f);
    const float motionDuration = (std::max)(clampedDistance / kArcherBasicArrowSpeed, 0.12f);
    const XMFLOAT3 startPosition =
    {
        origin.x + forward.x * kArcherBasicArrowStartForwardOffset + right.x * 0.10f,
        origin.y + kArcherBasicArrowStartHeight,
        origin.z + forward.z * kArcherBasicArrowStartForwardOffset + right.z * 0.10f
    };
    const XMFLOAT3 targetPosition =
    {
        startPosition.x + forward.x * clampedDistance,
        startPosition.y,
        startPosition.z + forward.z * clampedDistance
    };
    constexpr float kTrailBehindArrowOffset = 0.42f;
    const XMFLOAT3 trailStartPosition =
    {
        startPosition.x - forward.x * kTrailBehindArrowOffset,
        startPosition.y,
        startPosition.z - forward.z * kTrailBehindArrowOffset
    };
    const XMFLOAT3 trailTargetPosition =
    {
        targetPosition.x - forward.x * kTrailBehindArrowOffset,
        targetPosition.y,
        targetPosition.z - forward.z * kTrailBehindArrowOffset
    };
    const XMFLOAT4 trailColor =
    {
        0.88f * effectIntensity,
        2.20f * effectIntensity,
        1.72f * effectIntensity,
        1.0f
    };
    const XMFLOAT4 trailFade =
    {
        0.24f * effectIntensity,
        0.72f * effectIntensity,
        0.58f * effectIntensity,
        0.0f
    };

    auto spawnArrowTrail = [&](const XMFLOAT3& positionOffset, float startWidth, float startHeight, float endWidth, float endHeight, const XMFLOAT4& color)
    {
        EffectInstance* trail = AcquireEffect(EffectStyle::ArcherWindRibbon);
        if (trail == nullptr || trail->Object == nullptr || trail->Ritem == nullptr)
        {
            return;
        }

        const XMFLOAT3 offsetStart =
        {
            trailStartPosition.x + positionOffset.x,
            trailStartPosition.y + positionOffset.y,
            trailStartPosition.z + positionOffset.z
        };
        const XMFLOAT3 offsetTarget =
        {
            trailTargetPosition.x + positionOffset.x,
            trailTargetPosition.y + positionOffset.y,
            trailTargetPosition.z + positionOffset.z
        };

        trail->Style = EffectStyle::ArcherWindRibbon;
        trail->Active = true;
        trail->Age = 0.0f;
        trail->LifeTime = clampedStartDelay + motionDuration + 0.14f;
        trail->BasePosition = offsetStart;
        trail->TargetPosition = offsetTarget;
        trail->Velocity = { 0.0f, 0.0f, 0.0f };
        trail->StartScale = { startWidth, startHeight, 1.0f };
        trail->EndScale = { endWidth, endHeight, 1.0f };
        trail->StartColor = color;
        trail->EndColor = { trailFade.x, trailFade.y, trailFade.z, color.w * 0.52f };
        trail->RotX = 0.0f;
        trail->RotY = rotY - XM_PIDIV2;
        trail->RotZ = 0.0f;
        trail->StartDelay = clampedStartDelay;
        trail->MotionDuration = motionDuration;
        trail->FadeStartTime = clampedStartDelay + motionDuration;
        trail->SpinRate = 0.0f;
        trail->UseLinearMotion = true;
        trail->UseStyleAnimation = false;

        trail->Object->mIsBillboard = false;
        trail->Object->mIsAnimated = false;
        trail->Object->SetPosition(offsetStart.x, offsetStart.y, offsetStart.z);
        trail->Object->SetScale(trail->StartScale.x, trail->StartScale.y, trail->StartScale.z);
        trail->Object->SetRotation(trail->RotX, trail->RotY, trail->RotZ);
        trail->Object->Update();

        trail->Ritem->Mat = mArcherWindMaterial != nullptr
            ? mArcherWindMaterial
            : (mBeamMaterial != nullptr ? mBeamMaterial : mDecalMaterial);
        trail->Ritem->Visible = clampedStartDelay <= 0.0001f;
        trail->Ritem->CastShadow = false;
        trail->Ritem->ColorMultiplier = { color.x, color.y, color.z, trail->Ritem->Visible ? color.w : 0.0f };
        trail->Ritem->NumFramesDirty = gNumFrameResources;
    };

    spawnArrowTrail({ 0.0f, 0.0f, 0.0f }, 2.35f, 0.34f, 3.10f, 0.22f, trailColor);
    spawnArrowTrail(
        { right.x * 0.035f, 0.02f, right.z * 0.035f },
        3.10f,
        0.58f,
        3.80f,
        0.34f,
        { trailColor.x * 0.72f, trailColor.y * 0.78f, trailColor.z * 0.80f, 0.58f });
}

void SkillEffectManager::SpawnArcherBasicArrow(
    const XMFLOAT3& origin,
    float rotY,
    float travelDistance,
    float startDelay,
    float startHeight,
    float startRightOffset)
{
    EnsureArcherArrowRainPool();

    EffectInstance* effect = AcquireEffect(EffectStyle::ArrowRainArrow);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT3 right = RightFromYaw(rotY);
    const float resolvedStartHeight = startHeight >= 0.0f
        ? startHeight
        : kArcherBasicArrowStartHeight;
    const float clampedDistance = (std::clamp)(
        travelDistance,
        kArcherBasicArrowMinDistance,
        kArcherBasicArrowMaxDistance);
    const XMFLOAT3 startPosition =
    {
        origin.x + forward.x * kArcherBasicArrowStartForwardOffset + right.x * startRightOffset,
        origin.y + resolvedStartHeight,
        origin.z + forward.z * kArcherBasicArrowStartForwardOffset + right.z * startRightOffset
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

void SkillEffectManager::SpawnMageBasicOrb(const XMFLOAT3& origin, float rotY, float travelDistance, float startDelay)
{
    EnsureResources();
    EnsurePool();
    EnsureMageBasicOrbCorePool();

    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT3 right = RightFromYaw(rotY);
    const float clampedDistance = (std::clamp)(
        travelDistance,
        kMageBasicOrbMinDistance,
        kMageBasicOrbMaxDistance);
    const XMFLOAT3 startPosition =
    {
        origin.x + forward.x * kMageBasicOrbStartForwardOffset + right.x * kMageBasicOrbStartRightOffset,
        origin.y + kMageBasicOrbStartHeight,
        origin.z + forward.z * kMageBasicOrbStartForwardOffset + right.z * kMageBasicOrbStartRightOffset
    };
    const XMFLOAT3 targetPosition =
    {
        startPosition.x + forward.x * clampedDistance,
        startPosition.y,
        startPosition.z + forward.z * clampedDistance
    };
    const float clampedStartDelay = (std::max)(startDelay, 0.0f);
    const float motionDuration = (std::max)(clampedDistance / kMageBasicOrbSpeed, 0.12f);
    const XMFLOAT3 projectileVelocity =
    {
        forward.x * (clampedDistance / motionDuration),
        0.0f,
        forward.z * (clampedDistance / motionDuration)
    };

    auto spawnOrbSprite =
        [this](
            const XMFLOAT3& basePosition,
            const XMFLOAT3& velocity,
            float visibleDuration,
            float delayedStart,
            float startScaleX,
            float startScaleY,
            float endScaleX,
            float endScaleY,
            const XMFLOAT4& startColor,
            const XMFLOAT4& endColor,
            Material* material,
            bool billboard,
            float spriteRotY)
    {
        EffectInstance* effect = AcquireEffect(EffectStyle::MageBasicOrb);
        if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
        {
            return;
        }

        effect->Style = EffectStyle::MageBasicOrb;
        effect->Active = true;
        effect->Age = 0.0f;
        effect->LifeTime = delayedStart + (std::max)(visibleDuration, 0.05f);
        effect->BasePosition = basePosition;
        effect->Velocity = velocity;
        effect->StartScale = { startScaleX, startScaleY, 1.0f };
        effect->EndScale = { endScaleX, endScaleY, 1.0f };
        effect->StartColor = startColor;
        effect->EndColor = endColor;
        effect->RotX = 0.0f;
        effect->RotY = spriteRotY;
        effect->RotZ = billboard ? 0.0f : -XM_PIDIV2;
        effect->StartDelay = delayedStart;
        effect->UseStyleAnimation = true;
        effect->MotionDuration = 0.0f;
        effect->FadeStartTime = 0.0f;
        effect->UseLinearMotion = false;

        effect->Object->mIsBillboard = billboard;
        effect->Object->mIsAnimated = false;
        effect->Object->SetPosition(basePosition.x, basePosition.y, basePosition.z);
        effect->Object->SetScale(startScaleX, startScaleY, 1.0f);
        if (!billboard)
        {
            effect->Object->SetRotation(0.0f, spriteRotY, -XM_PIDIV2);
        }
        effect->Object->Update();

        effect->Ritem->Mat = material != nullptr ? material : effect->Ritem->Mat;
        effect->Ritem->Visible = delayedStart <= 0.0001f;
        effect->Ritem->CastShadow = false;
        effect->Ritem->ColorMultiplier = startColor;
        effect->Ritem->TexTransform = MathHelper::Identity4x4();
        if (!effect->Ritem->Visible)
        {
            effect->Ritem->ColorMultiplier.w = 0.0f;
        }
        effect->Ritem->NumFramesDirty = gNumFrameResources;
    };

    if (EffectInstance* core = AcquireEffect(EffectStyle::MageBasicOrbCore);
        core != nullptr && core->Object != nullptr && core->Ritem != nullptr)
    {
        core->Style = EffectStyle::MageBasicOrbCore;
        core->Active = true;
        core->Age = 0.0f;
        core->LifeTime = clampedStartDelay + motionDuration;
        core->BasePosition = startPosition;
        core->Velocity = projectileVelocity;
        core->StartScale = { 0.22f, 0.22f, 0.22f };
        core->EndScale = { 0.12f, 0.12f, 0.12f };
        core->StartColor = { 0.64f, 1.22f, 2.25f, 0.96f };
        core->EndColor = { 0.18f, 0.56f, 1.18f, 0.0f };
        core->RotX = 0.0f;
        core->RotY = 0.0f;
        core->RotZ = 0.0f;
        core->StartDelay = clampedStartDelay;
        core->MotionDuration = 0.0f;
        core->FadeStartTime = 0.0f;
        core->UseLinearMotion = false;
        core->UseStyleAnimation = true;

        core->Object->mIsBillboard = false;
        core->Object->mIsAnimated = false;
        core->Object->SetPosition(startPosition.x, startPosition.y, startPosition.z);
        core->Object->SetScale(0.22f, 0.22f, 0.22f);
        core->Object->SetRotation(0.0f, 0.0f, 0.0f);
        core->Object->Update();

        core->Ritem->Mat = mMageBasicOrbCoreMaterial != nullptr ? mMageBasicOrbCoreMaterial : core->Ritem->Mat;
        core->Ritem->Visible = clampedStartDelay <= 0.0001f;
        core->Ritem->CastShadow = false;
        core->Ritem->ColorMultiplier = core->StartColor;
        if (!core->Ritem->Visible)
        {
            core->Ritem->ColorMultiplier.w = 0.0f;
        }
        core->Ritem->NumFramesDirty = gNumFrameResources;
    }

    spawnOrbSprite(
        startPosition,
        projectileVelocity,
        motionDuration,
        clampedStartDelay,
        0.72f,
        0.88f,
        0.32f,
        0.42f,
        { 0.40f, 0.90f, 1.92f, 0.80f },
        { 0.08f, 0.34f, 0.92f, 0.0f },
        mMageBasicOrbAuraMaterial,
        false,
        rotY + XM_PIDIV2);
    spawnOrbSprite(
        {
            startPosition.x - forward.x * 0.18f,
            startPosition.y,
            startPosition.z - forward.z * 0.18f
        },
        projectileVelocity,
        motionDuration,
        clampedStartDelay,
        0.94f,
        1.14f,
        0.44f,
        0.54f,
        { 0.24f, 0.62f, 1.52f, 0.64f },
        { 0.04f, 0.20f, 0.62f, 0.0f },
        mMageBasicOrbTrailMaterial,
        false,
        rotY + XM_PIDIV2);
    spawnOrbSprite(
        {
            startPosition.x - forward.x * 0.30f,
            startPosition.y,
            startPosition.z - forward.z * 0.30f
        },
        projectileVelocity,
        motionDuration,
        clampedStartDelay,
        1.08f,
        1.30f,
        0.58f,
        0.70f,
        { 0.16f, 0.44f, 1.22f, 0.52f },
        { 0.02f, 0.12f, 0.44f, 0.0f },
        mMageBasicOrbOuterTrailMaterial,
        false,
        rotY + XM_PIDIV2);
    spawnOrbSprite(
        {
            startPosition.x - forward.x * 0.08f,
            startPosition.y,
            startPosition.z - forward.z * 0.08f
        },
        { 0.0f, 0.0f, 0.0f },
        0.10f,
        clampedStartDelay,
        0.46f,
        0.56f,
        0.10f,
        0.14f,
        { 0.68f, 1.28f, 2.30f, 0.92f },
        { 0.22f, 0.54f, 1.10f, 0.0f },
        mMageBasicOrbFlashMaterial,
        false,
        rotY + XM_PIDIV2);
    spawnOrbSprite(
        targetPosition,
        { 0.0f, 0.0f, 0.0f },
        0.16f,
        clampedStartDelay + motionDuration,
        0.34f,
        0.40f,
        0.92f,
        1.08f,
        { 0.72f, 1.36f, 2.42f, 0.94f },
        { 0.24f, 0.58f, 1.22f, 0.0f },
        mMageBasicOrbImpactMaterial,
        false,
        rotY + XM_PIDIV2);
}

void SkillEffectManager::UpdateLocalArcherHasteAura(float dt)
{
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
            ClearWeaponSkillGlow();
        }

        mLocalArcherBuffLoopActive = false;
        SetArcherBuffLoopVisible(false);
        mArcherHasteAuraPulseTimer = 0.0f;
        return;
    }

    const XMFLOAT3 origin = archer->GetPosition();
    const float rotY = archer->GetFacingRotY();
    mLastLocalArcherBuffPosition = origin;
    mLastLocalArcherBuffRotY = rotY;

    if (!mLocalArcherBuffLoopActive)
    {
        mLocalArcherBuffLoopActive = true;
        EnsureArcherBuffLoopVisuals();
        TriggerWeaponSkillGlow({ 0.58f, 1.82f, 1.26f, 1.0f }, 0.72f);
    }

    mArcherHasteAuraPulseTimer += dt;
    if (mWeaponGlowTimer <= 0.12f)
    {
        TriggerWeaponSkillGlow({ 0.58f, 1.82f, 1.26f, 1.0f }, 0.72f);
    }
    UpdateArcherBuffLoopVisuals(origin, rotY, 1.0f);
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

void SkillEffectManager::PreviewMageMeteor(
    const XMFLOAT3& targetPosition,
    float effectRadius,
    float impactDelay)
{
    const float clampedDelay = (std::max)(impactDelay, 0.65f);
    const float radius = (std::max)(effectRadius, 1.10f);
    const float telegraphLife = clampedDelay + 0.08f;
    const float fallDuration = (std::clamp)(clampedDelay * 0.62f, kMageMeteorMinFallDuration, clampedDelay);
    const float fallStartDelay = (std::max)(clampedDelay - fallDuration, 0.0f);
    const float spawnHeight = (std::max)(kMageMeteorSpawnHeight, radius * 3.25f);
    const XMFLOAT3 impactVisualPosition =
    {
        targetPosition.x,
        targetPosition.y + 1.05f,
        targetPosition.z
    };

    SpawnGroundDecal(
        { targetPosition.x, targetPosition.y + 0.035f, targetPosition.z },
        0.0f,
        radius * 1.12f,
        radius * 1.12f,
        telegraphLife,
        { 1.32f, 0.12f, 0.06f, 0.94f },
        { 0.44f, 0.02f, 0.02f, 0.12f },
        mMageMeteorCircleMaterial != nullptr ? mMageMeteorCircleMaterial : mDecalMaterial);
    SpawnGroundDecal(
        { targetPosition.x, targetPosition.y + 0.041f, targetPosition.z },
        XM_PIDIV4,
        radius * 0.82f,
        radius * 0.82f,
        telegraphLife,
        { 1.54f, 0.18f, 0.08f, 0.78f },
        { 0.52f, 0.03f, 0.02f, 0.08f },
        mMageMeteorCircleMaterial != nullptr ? mMageMeteorCircleMaterial : mDecalMaterial);

    const XMFLOAT3 coreStart =
    {
        targetPosition.x,
        targetPosition.y + spawnHeight,
        targetPosition.z
    };
    SpawnMageMeteorFlameSprite(
        coreStart,
        impactVisualPosition,
        fallDuration,
        fallStartDelay,
        radius * 1.34f,
        radius * 1.62f,
        radius * 0.94f,
        radius * 1.10f,
        { 1.30f, 0.18f, 0.06f, 1.00f },
        { 0.78f, 0.04f, 0.02f, 1.00f },
        mMageMeteorFlameMaterials[5],
        true);
    SpawnMageMeteorFlameSprite(
        { targetPosition.x, targetPosition.y + spawnHeight + 0.10f, targetPosition.z },
        { targetPosition.x, impactVisualPosition.y + 0.08f, targetPosition.z },
        fallDuration,
        fallStartDelay,
        radius * 0.84f,
        radius * 1.14f,
        radius * 0.68f,
        radius * 0.90f,
        { 1.46f, 0.22f, 0.07f, 1.00f },
        { 0.88f, 0.05f, 0.02f, 1.00f },
        mMageMeteorFlameMaterials[6],
        true);
    SpawnMageMeteorFlameSprite(
        { targetPosition.x - radius * 0.12f, targetPosition.y + spawnHeight + 0.25f, targetPosition.z + radius * 0.10f },
        { targetPosition.x - radius * 0.04f, impactVisualPosition.y + 0.10f, targetPosition.z + radius * 0.02f },
        fallDuration,
        fallStartDelay + 0.02f,
        radius * 1.82f,
        radius * 2.26f,
        radius * 1.08f,
        radius * 1.34f,
        { 1.12f, 0.10f, 0.04f, 1.00f },
        { 0.58f, 0.02f, 0.01f, 0.96f },
        mMageMeteorFlameMaterials[4],
        true);
    SpawnMageMeteorFlameSprite(
        { targetPosition.x + radius * 0.08f, targetPosition.y + spawnHeight + 0.50f, targetPosition.z - radius * 0.08f },
        { targetPosition.x + radius * 0.03f, impactVisualPosition.y + 0.22f, targetPosition.z - radius * 0.02f },
        fallDuration,
        fallStartDelay,
        radius * 1.82f,
        radius * 2.34f,
        radius * 1.04f,
        radius * 1.38f,
        { 0.96f, 0.06f, 0.03f, 0.96f },
        { 0.46f, 0.01f, 0.01f, 0.92f },
        mMageMeteorFlameMaterials[3],
        true);
    SpawnMageMeteorFlameSprite(
        { targetPosition.x, targetPosition.y + spawnHeight + 0.80f, targetPosition.z },
        { targetPosition.x, impactVisualPosition.y + 0.52f, targetPosition.z },
        fallDuration,
        fallStartDelay + 0.04f,
        radius * 1.96f,
        radius * 2.64f,
        radius * 1.12f,
        radius * 1.54f,
        { 0.72f, 0.03f, 0.02f, 0.92f },
        { 0.30f, 0.00f, 0.00f, 0.84f },
        mMageMeteorFlameMaterials[7],
        true);

    for (int i = 0; i < 8; ++i)
    {
        const float angle = (XM_2PI / 8.0f) * static_cast<float>(i) + 0.34f;
        const float startRadius = radius * (0.22f + 0.035f * static_cast<float>(i % 3));
        const float endRadius = radius * (0.54f + 0.06f * static_cast<float>((i + 1) % 3));
        const float delay = fallStartDelay + 0.025f * static_cast<float>(i % 4);
        const XMFLOAT3 shardStart =
        {
            targetPosition.x + std::cos(angle) * startRadius,
            targetPosition.y + spawnHeight + 0.20f * static_cast<float>(i % 2),
            targetPosition.z + std::sin(angle) * startRadius
        };
        const XMFLOAT3 shardEnd =
        {
            targetPosition.x + std::cos(angle + 0.28f) * endRadius,
            impactVisualPosition.y + 0.32f + 0.06f * static_cast<float>(i % 2),
            targetPosition.z + std::sin(angle + 0.28f) * endRadius
        };

        SpawnMageMeteorFlameSprite(
            shardStart,
            shardEnd,
            fallDuration * (0.82f + 0.03f * static_cast<float>(i % 3)),
            delay,
            radius * 0.26f,
            radius * 0.72f,
            radius * 0.10f,
            radius * 0.28f,
            { 1.04f, 0.16f, 0.05f, 0.96f },
            { 0.38f, 0.02f, 0.01f, 0.86f },
            mMageMeteorFlameMaterials[i % kMageMeteorFlameMaterialCount],
            true);
    }
}

void SkillEffectManager::PreviewArcherArrowRain(
    const XMFLOAT3& targetPosition,
    float effectRadius,
    float fallStartDelay,
    float fallDuration)
{
    const float clampedStartDelay = (std::max)(fallStartDelay, 0.0f);
    const float clampedFallDuration = (std::max)(fallDuration, 0.05f);
    const float radius = (std::max)(effectRadius, 0.90f);
    constexpr float kArrowRainWaveDelay = 0.16f;
    constexpr int kArrowRainLastWaveIndex = 2;
    const float telegraphLife =
        clampedStartDelay +
        kArrowRainWaveDelay * static_cast<float>(kArrowRainLastWaveIndex) +
        clampedFallDuration +
        0.16f;
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
        int waveIndex;
        float localDelay;
        float spawnHeight;
        float scale;
    };

    static constexpr RainArrowSpec kRainArrows[] =
    {
        { 0.10f, 0.00f, 0.04f, 0, 0.00f, 3.8f, 0.88f },
        { 3.92f, 0.22f, 0.28f, 0, 0.04f, 3.9f, 0.82f },
        { 2.92f, 0.18f, -0.05f, 0, 0.08f, 4.0f, 0.80f },
        { 0.84f, 0.34f, 0.22f, 1, 0.00f, 4.2f, 0.80f },
        { 2.34f, 0.44f, 0.34f, 1, 0.04f, 4.1f, 0.78f },
        { 5.54f, 0.38f, 0.16f, 1, 0.08f, 4.0f, 0.78f },
        { 1.62f, 0.58f, -0.18f, 2, 0.00f, 4.5f, 0.82f },
        { 3.04f, 0.70f, -0.12f, 2, 0.04f, 4.7f, 0.78f },
        { 4.76f, 0.54f, -0.26f, 2, 0.08f, 4.4f, 0.80f }
    };

    for (const RainArrowSpec& arrow : kRainArrows)
    {
        const XMFLOAT3 impactPosition =
        {
            targetPosition.x + std::cos(arrow.angle) * radius * arrow.radialScale,
            targetPosition.y,
            targetPosition.z + std::sin(arrow.angle) * radius * arrow.radialScale
        };
        const float startDelay =
            clampedStartDelay +
            kArrowRainWaveDelay * static_cast<float>(arrow.waveIndex) +
            arrow.localDelay;
        const float impactDelay = startDelay + clampedFallDuration;
        SpawnArcherArrowRainArrow(
            impactPosition,
            arrow.yaw,
            startDelay,
            clampedFallDuration,
            arrow.scale,
            arrow.spawnHeight);

        const float impactScale = radius * (0.16f + 0.035f * static_cast<float>(arrow.waveIndex));
        SpawnGroundDecal(
            { impactPosition.x, impactPosition.y + 0.052f, impactPosition.z },
            arrow.yaw,
            impactScale * 0.28f,
            impactScale * 1.58f,
            0.20f,
            { 1.48f, 1.84f, 1.24f, 0.86f },
            { 0.22f, 0.56f, 0.32f, 0.0f },
            mArcherArrowRainDecalMaterial != nullptr ? mArcherArrowRainDecalMaterial : mArcherCircleMaterial,
            2.8f,
            0.14f,
            impactDelay);
        SpawnGroundDecal(
            { impactPosition.x, impactPosition.y + 0.058f, impactPosition.z },
            arrow.yaw + XM_PIDIV4,
            impactScale * 0.18f,
            impactScale * 0.92f,
            0.16f,
            { 0.86f, 1.08f, 0.78f, 0.62f },
            { 0.16f, 0.22f, 0.16f, 0.0f },
            mArcherDustMaterial != nullptr ? mArcherDustMaterial : mDecalMaterial,
            -1.9f,
            0.10f,
            impactDelay + 0.015f);
    }
}

void SkillEffectManager::OnSkillResolved(PlayerClass playerClass, int skillIndex, const XMFLOAT3& impactCenter, float rotY, float effectRadius)
{
    if (playerClass == PlayerClass::Warrior && skillIndex == 1)
    {
        const float decalScale = (std::max)(effectRadius, 0.1f);
        const XMFLOAT3 decalPosition =
        {
            impactCenter.x,
            impactCenter.y - Player::DefaultColliderHalfHeight + 0.05f,
            impactCenter.z
        };
        const XMFLOAT4 crackColor = { 1.08f, 0.88f, 0.70f, 0.94f };
        const XMFLOAT4 crackFade = { 0.42f, 0.28f, 0.16f, 0.0f };
        Material* smokeMaterial = mEarthshatterSmokeMaterial != nullptr ? mEarthshatterSmokeMaterial : mBeamMaterial;
        Material* stoneMaterial = mEarthshatterStoneMaterial != nullptr ? mEarthshatterStoneMaterial : mBeamMaterial;

        SpawnGroundDecal(
            decalPosition,
            rotY + 0.08f,
            decalScale * 0.74f,
            decalScale * 1.08f,
            0.85f,
            crackColor,
            crackFade,
            mEarthshatterDecalMaterial,
            0.08f,
            0.22f);
        SpawnGroundDecal(
            { decalPosition.x, decalPosition.y + 0.004f, decalPosition.z },
            rotY - XM_PIDIV4 * 0.25f,
            decalScale * 0.52f,
            decalScale * 0.88f,
            0.74f,
            { 0.96f, 0.74f, 0.56f, 0.64f },
            { 0.28f, 0.16f, 0.08f, 0.0f },
            mEarthshatterDecalMaterial,
            -0.06f,
            0.18f,
            0.02f);

        for (int i = 0; i < 14; ++i)
        {
            const float angle = (XM_2PI / 14.0f) * static_cast<float>(i) + MathHelper::RandF(-0.18f, 0.18f);
            const float radius = MathHelper::RandF(0.08f, decalScale * 0.30f);
            const float outward = MathHelper::RandF(decalScale * 0.32f, decalScale * 0.82f);
            const XMFLOAT3 radial = { std::cos(angle), 0.0f, std::sin(angle) };
            const XMFLOAT3 smokeStart =
            {
                decalPosition.x + radial.x * radius,
                decalPosition.y + MathHelper::RandF(0.16f, 0.34f),
                decalPosition.z + radial.z * radius
            };
            const XMFLOAT3 smokeEnd =
            {
                smokeStart.x + radial.x * outward,
                smokeStart.y + MathHelper::RandF(0.48f, 0.88f),
                smokeStart.z + radial.z * outward
            };

            SpawnMageMeteorFlameSprite(
                smokeStart,
                smokeEnd,
                MathHelper::RandF(0.52f, 0.78f),
                MathHelper::RandF(0.00f, 0.03f),
                MathHelper::RandF(0.60f, 0.92f),
                MathHelper::RandF(0.52f, 0.84f),
                MathHelper::RandF(1.36f, 1.92f),
                MathHelper::RandF(1.20f, 1.72f),
                { 1.08f, 0.92f, 0.78f, 0.82f },
                { 0.54f, 0.42f, 0.32f, 0.0f },
                smokeMaterial,
                true,
                rotY,
                0.28f);
        }

        for (int i = 0; i < 24; ++i)
        {
            const float angle = MathHelper::RandF(0.0f, XM_2PI);
            const float radius = MathHelper::RandF(0.03f, decalScale * 0.16f);
            const float outward = MathHelper::RandF(decalScale * 0.34f, decalScale * 1.02f);
            const XMFLOAT3 radial = { std::cos(angle), 0.0f, std::sin(angle) };
            const XMFLOAT3 stoneStart =
            {
                decalPosition.x + radial.x * radius,
                decalPosition.y + MathHelper::RandF(0.08f, 0.18f),
                decalPosition.z + radial.z * radius
            };
            const XMFLOAT3 stoneEnd =
            {
                stoneStart.x + radial.x * outward,
                stoneStart.y + MathHelper::RandF(0.24f, 0.56f),
                stoneStart.z + radial.z * outward
            };

            SpawnMageMeteorFlameSprite(
                stoneStart,
                stoneEnd,
                MathHelper::RandF(0.32f, 0.54f),
                MathHelper::RandF(0.00f, 0.02f),
                MathHelper::RandF(0.14f, 0.22f),
                MathHelper::RandF(0.14f, 0.22f),
                MathHelper::RandF(0.18f, 0.28f),
                MathHelper::RandF(0.18f, 0.28f),
                { 1.24f, 1.04f, 0.78f, 1.00f },
                { 0.92f, 0.70f, 0.42f, 0.0f },
                stoneMaterial,
                true,
                rotY,
                0.16f);
        }
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
    else if (playerClass == PlayerClass::Mage && skillIndex == 2)
    {
        const float ringScale = (std::max)(effectRadius, 1.10f);
        const XMFLOAT3 ringPosition = { impactCenter.x, impactCenter.y + 1.24f, impactCenter.z };
        Material* shockwaveFallbackMaterial = mMageMeteorShockwaveMaterial != nullptr
            ? mMageMeteorShockwaveMaterial
            : (mMageMeteorCircleMaterial != nullptr ? mMageMeteorCircleMaterial : mDecalMaterial);
        auto pickShockwaveMaterial =
            [this, shockwaveFallbackMaterial](int index) -> Material*
            {
                if (index >= 0 &&
                    index < kMageMeteorShockwaveMaterialCount &&
                    mMageMeteorShockwaveMaterials[index] != nullptr)
                {
                    return mMageMeteorShockwaveMaterials[index];
                }
                return shockwaveFallbackMaterial;
            };

        SpawnMageMeteorFlameSprite(
            ringPosition,
            ringPosition,
            0.54f,
            0.0f,
            ringScale * 0.46f,
            ringScale * 0.46f,
            ringScale * 1.20f,
            ringScale * 1.20f,
            { 3.20f, 1.18f, 0.36f, 1.00f },
            { 0.92f, 0.18f, 0.04f, 0.96f },
            pickShockwaveMaterial(0),
            true,
            rotY,
            kMageMeteorRingFadeOutDuration);
        SpawnMageMeteorFlameSprite(
            { impactCenter.x, impactCenter.y + 1.34f, impactCenter.z },
            { impactCenter.x, impactCenter.y + 1.34f, impactCenter.z },
            0.44f,
            0.035f,
            ringScale * 0.26f,
            ringScale * 0.26f,
            ringScale * 0.78f,
            ringScale * 0.78f,
            { 3.80f, 1.38f, 0.42f, 1.00f },
            { 1.02f, 0.20f, 0.05f, 0.96f },
            pickShockwaveMaterial(1),
            true,
            rotY + XM_PIDIV4,
            kMageMeteorRingFadeOutDuration);
        SpawnMageMeteorFlameSprite(
            { impactCenter.x, impactCenter.y + 1.44f, impactCenter.z },
            { impactCenter.x, impactCenter.y + 1.44f, impactCenter.z },
            0.62f,
            0.070f,
            ringScale * 0.56f,
            ringScale * 0.56f,
            ringScale * 1.78f,
            ringScale * 1.78f,
            { 2.38f, 0.72f, 0.20f, 0.98f },
            { 0.54f, 0.08f, 0.02f, 0.90f },
            pickShockwaveMaterial(2),
            true,
            rotY + XM_PIDIV2,
            kMageMeteorRingFadeOutDuration);
        SpawnMageMeteorFlameSprite(
            { impactCenter.x, impactCenter.y + 1.52f, impactCenter.z },
            { impactCenter.x, impactCenter.y + 1.52f, impactCenter.z },
            0.70f,
            0.105f,
            ringScale * 0.70f,
            ringScale * 0.70f,
            ringScale * 2.05f,
            ringScale * 2.05f,
            { 1.78f, 0.44f, 0.14f, 0.92f },
            { 0.34f, 0.04f, 0.01f, 0.82f },
            pickShockwaveMaterial(3),
            true,
            rotY + XM_PI,
            kMageMeteorRingFadeOutDuration);

        const XMFLOAT3 burstStart =
        {
            impactCenter.x,
            impactCenter.y + 0.95f,
            impactCenter.z
        };
        SpawnMageMeteorFlameSprite(
            burstStart,
            { impactCenter.x, impactCenter.y + 2.20f, impactCenter.z },
            kMageMeteorImpactBurstLife,
            0.0f,
            ringScale * 1.06f,
            ringScale * 1.24f,
            ringScale * 0.52f,
            ringScale * 0.66f,
            { 2.56f, 0.40f, 0.24f, 1.00f },
            { 1.34f, 0.10f, 0.08f, 0.92f },
            mMageMeteorFlameMaterials[6],
            true);
        SpawnMageMeteorFlameSprite(
            { impactCenter.x, impactCenter.y + 1.02f, impactCenter.z },
            { impactCenter.x, impactCenter.y + 2.46f, impactCenter.z },
            kMageMeteorImpactBurstLife,
            0.0f,
            ringScale * 0.76f,
            ringScale * 0.96f,
            ringScale * 0.36f,
            ringScale * 0.48f,
            { 3.00f, 0.52f, 0.30f, 1.00f },
            { 1.80f, 0.14f, 0.10f, 0.94f },
            mMageMeteorFlameMaterials[5],
            true);
        SpawnMageMeteorFlameSprite(
            { impactCenter.x - ringScale * 0.12f, impactCenter.y + 1.04f, impactCenter.z + ringScale * 0.08f },
            { impactCenter.x - ringScale * 0.18f, impactCenter.y + 2.00f, impactCenter.z + ringScale * 0.10f },
            kMageMeteorImpactBurstLife,
            0.01f,
            ringScale * 1.34f,
            ringScale * 1.68f,
            ringScale * 0.66f,
            ringScale * 0.84f,
            { 2.10f, 0.18f, 0.16f, 0.98f },
            { 1.04f, 0.04f, 0.08f, 0.88f },
            mMageMeteorFlameMaterials[3],
            true);
        SpawnMageMeteorFlameSprite(
            { impactCenter.x + ringScale * 0.10f, impactCenter.y + 1.12f, impactCenter.z - ringScale * 0.10f },
            { impactCenter.x + ringScale * 0.16f, impactCenter.y + 2.40f, impactCenter.z - ringScale * 0.16f },
            kMageMeteorImpactBurstLife,
            0.0f,
            ringScale * 1.54f,
            ringScale * 1.96f,
            ringScale * 0.74f,
            ringScale * 0.92f,
            { 1.54f, 0.06f, 0.10f, 0.92f },
            { 0.78f, 0.02f, 0.06f, 0.78f },
            mMageMeteorFlameMaterials[2],
            true);

        for (int i = 0; i < kMageMeteorShardCount; ++i)
        {
            const float angle = (XM_2PI / static_cast<float>(kMageMeteorShardCount)) * static_cast<float>(i) + rotY;
            const float distance = ringScale * (0.72f + 0.10f * static_cast<float>(i % 4));
            const float height = 0.42f + 0.14f * static_cast<float>(i % 3);
            const XMFLOAT3 shardStart =
            {
                impactCenter.x + std::cos(angle) * ringScale * 0.14f,
                impactCenter.y + 0.82f,
                impactCenter.z + std::sin(angle) * ringScale * 0.14f
            };
            const XMFLOAT3 shardEnd =
            {
                impactCenter.x + std::cos(angle + 0.10f) * distance,
                impactCenter.y + height,
                impactCenter.z + std::sin(angle + 0.10f) * distance
            };

            SpawnMageMeteorFlameSprite(
                shardStart,
                shardEnd,
                0.30f + 0.025f * static_cast<float>(i % 3),
                0.012f * static_cast<float>(i % 4),
                ringScale * 0.28f,
                ringScale * 0.58f,
                ringScale * 0.08f,
                ringScale * 0.18f,
                { 2.80f, 0.72f, 0.22f, 0.98f },
                { 0.76f, 0.04f, 0.02f, 0.86f },
                mMageMeteorFlameMaterials[i % kMageMeteorFlameMaterialCount],
                true);
        }
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
            resources->GetTexture("Effect_Earthshatter_Crater") != nullptr ? "Effect_Earthshatter_Crater" :
            (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white"),
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.04f, 0.04f, 0.04f),
            0.22f);
    }

    if (resources->GetMaterial("SkillFx_EarthshatterStoneMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_EarthshatterStoneMat",
            static_cast<int>(resources->mMaterials.size()),
            resources->GetTexture("Effect_Earthshatter_Stone") != nullptr ? "Effect_Earthshatter_Stone" : "white",
            "",
            "",
            "",
            XMFLOAT4(0.92f, 0.82f, 0.66f, 0.96f),
            XMFLOAT3(0.08f, 0.06f, 0.04f),
            0.20f);
    }

    if (resources->GetMaterial("SkillFx_EarthshatterSmokeMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_EarthshatterSmokeMat",
            static_cast<int>(resources->mMaterials.size()),
            resources->GetTexture("Effect_Earthshatter_Smoke") != nullptr ? "Effect_Earthshatter_Smoke" : "white",
            "",
            "",
            "",
            XMFLOAT4(0.78f, 0.72f, 0.66f, 0.84f),
            XMFLOAT3(0.05f, 0.05f, 0.05f),
            0.18f);
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

    const std::string mageOrbFlashTextureName =
        resources->GetTexture("Effect_MageBasic_Muzzle01") != nullptr ? "Effect_MageBasic_Muzzle01" : "white";
    const std::string mageOrbAuraTextureName =
        resources->GetTexture("Effect_MageBasic_Muzzle02") != nullptr ? "Effect_MageBasic_Muzzle02" : mageOrbFlashTextureName;
    const std::string mageOrbTrailTextureName =
        resources->GetTexture("Effect_MageBasic_Muzzle03") != nullptr ? "Effect_MageBasic_Muzzle03" : mageOrbAuraTextureName;
    const std::string mageOrbOuterTrailTextureName =
        resources->GetTexture("Effect_MageBasic_Muzzle04") != nullptr ? "Effect_MageBasic_Muzzle04" : mageOrbTrailTextureName;
    const std::string mageOrbImpactTextureName =
        resources->GetTexture("Effect_MageBasic_Muzzle05") != nullptr ? "Effect_MageBasic_Muzzle05" : mageOrbFlashTextureName;
    const std::string mageHealSparkleTextureName =
        resources->GetTexture("Effect_MageHeal_Sparkle") != nullptr ? "Effect_MageHeal_Sparkle" : mageOrbFlashTextureName;
    const std::string mageHealSmokeTextureName =
        resources->GetTexture("Effect_MageHeal_Smoke") != nullptr ? "Effect_MageHeal_Smoke" : mageHealSparkleTextureName;
    const std::string mageHealPointTextureName =
        resources->GetTexture("Effect_MageHeal_Point") != nullptr ? "Effect_MageHeal_Point" : mageHealSparkleTextureName;
    const std::string mageMeteorCircleTextureName =
        resources->GetTexture("Effect_MageMeteor_Circle") != nullptr ? "Effect_MageMeteor_Circle" :
        (resources->GetTexture("Effect_Circle02") != nullptr ? "Effect_Circle02" :
        (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white"));

    auto resolveEffectTexture =
        [resources](const std::string& textureName, const std::string& fallbackName)
        {
            return resources->GetTexture(textureName) != nullptr ? textureName : fallbackName;
        };

    const std::string mageMeteorShockwaveTextureNames[kMageMeteorShockwaveMaterialCount] =
    {
        resolveEffectTexture("Effect_MageMeteor_ShockwaveRing01", mageMeteorCircleTextureName),
        resolveEffectTexture("Effect_MageMeteor_ShockwaveRing02", mageMeteorCircleTextureName),
        resolveEffectTexture("Effect_MageMeteor_ShockwaveRing03", mageMeteorCircleTextureName),
        resolveEffectTexture("Effect_MageMeteor_ShockwaveRing04", mageMeteorCircleTextureName)
    };

    const std::string mageMeteorFlameFallbackNames[kMageMeteorFlameMaterialCount] =
    {
        resources->GetTexture("Effect_MageMeteor_Flame01") != nullptr ? "Effect_MageMeteor_Flame01" : mageOrbFlashTextureName,
        resources->GetTexture("Effect_MageMeteor_Flame02") != nullptr ? "Effect_MageMeteor_Flame02" : mageOrbImpactTextureName,
        resources->GetTexture("Effect_MageMeteor_Flame03") != nullptr ? "Effect_MageMeteor_Flame03" : mageOrbTrailTextureName,
        resources->GetTexture("Effect_MageMeteor_Flame04") != nullptr ? "Effect_MageMeteor_Flame04" : mageOrbAuraTextureName,
        mageOrbFlashTextureName,
        mageOrbImpactTextureName,
        mageOrbTrailTextureName,
        mageOrbAuraTextureName
    };

    const std::string mageMeteorFlameTextureNames[kMageMeteorFlameMaterialCount] =
    {
        resolveEffectTexture("Effect_MageMeteor_FireSmoke01", mageMeteorFlameFallbackNames[0]),
        resolveEffectTexture("Effect_MageMeteor_FireSmoke02", mageMeteorFlameFallbackNames[1]),
        resolveEffectTexture("Effect_MageMeteor_FireSmoke03", mageMeteorFlameFallbackNames[2]),
        resolveEffectTexture("Effect_MageMeteor_FireSmoke04", mageMeteorFlameFallbackNames[3]),
        resolveEffectTexture("Effect_MageMeteor_FireSmoke05", mageMeteorFlameFallbackNames[4]),
        resolveEffectTexture("Effect_MageMeteor_FireSmoke06", mageMeteorFlameFallbackNames[5]),
        resolveEffectTexture("Effect_MageMeteor_FireSmoke07", mageMeteorFlameFallbackNames[6]),
        resolveEffectTexture("Effect_MageMeteor_FireSmoke08", mageMeteorFlameFallbackNames[7])
    };

    if (resources->GetMaterial("SkillFx_MageBasicOrbCoreMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageBasicOrbCoreMat",
            static_cast<int>(resources->mMaterials.size()),
            resources->GetTexture("Blue") != nullptr ? "Blue" : "white",
            "",
            "",
            "",
            XMFLOAT4(0.70f, 1.18f, 1.92f, 0.92f),
            XMFLOAT3(0.10f, 0.16f, 0.22f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageBasicOrbAuraMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageBasicOrbAuraMat",
            static_cast<int>(resources->mMaterials.size()),
            mageOrbAuraTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.44f, 0.96f, 1.92f, 0.90f),
            XMFLOAT3(0.08f, 0.14f, 0.20f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageBasicOrbTrailMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageBasicOrbTrailMat",
            static_cast<int>(resources->mMaterials.size()),
            mageOrbTrailTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.30f, 0.74f, 1.54f, 0.82f),
            XMFLOAT3(0.06f, 0.10f, 0.16f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageBasicOrbOuterTrailMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageBasicOrbOuterTrailMat",
            static_cast<int>(resources->mMaterials.size()),
            mageOrbOuterTrailTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.24f, 0.62f, 1.42f, 0.76f),
            XMFLOAT3(0.05f, 0.09f, 0.14f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageBasicOrbFlashMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageBasicOrbFlashMat",
            static_cast<int>(resources->mMaterials.size()),
            mageOrbFlashTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.80f, 1.28f, 2.20f, 0.94f),
            XMFLOAT3(0.10f, 0.16f, 0.22f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageBasicOrbImpactMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageBasicOrbImpactMat",
            static_cast<int>(resources->mMaterials.size()),
            mageOrbImpactTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.86f, 1.34f, 2.26f, 0.96f),
            XMFLOAT3(0.10f, 0.16f, 0.22f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageHealSparkleMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageHealSparkleMat",
            static_cast<int>(resources->mMaterials.size()),
            mageHealSparkleTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.12f, 1.00f, 0.54f, 0.96f),
            XMFLOAT3(0.08f, 0.06f, 0.02f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageHealSmokeMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageHealSmokeMat",
            static_cast<int>(resources->mMaterials.size()),
            mageHealSmokeTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.52f, 0.92f, 0.72f, 0.72f),
            XMFLOAT3(0.04f, 0.08f, 0.05f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageHealPointMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageHealPointMat",
            static_cast<int>(resources->mMaterials.size()),
            mageHealPointTextureName,
            "",
            "",
            "",
            XMFLOAT4(0.86f, 1.36f, 2.40f, 0.98f),
            XMFLOAT3(0.08f, 0.14f, 0.22f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_MageMeteorCircleMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_MageMeteorCircleMat",
            static_cast<int>(resources->mMaterials.size()),
            mageMeteorCircleTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.72f, 1.24f, 0.52f, 0.98f),
            XMFLOAT3(0.12f, 0.08f, 0.03f),
            0.02f);
    }

    for (int i = 0; i < kMageMeteorShockwaveMaterialCount; ++i)
    {
        const std::string materialName = "SkillFx_MageMeteorShockwaveMat0" + std::to_string(i + 1);
        if (resources->GetMaterial(materialName) == nullptr)
        {
            resources->CreateMaterial(
                materialName,
                static_cast<int>(resources->mMaterials.size()),
                mageMeteorShockwaveTextureNames[i],
                "",
                "",
                "",
                XMFLOAT4(1.84f, 1.12f, 0.42f, 0.88f),
                XMFLOAT3(0.10f, 0.06f, 0.02f),
                0.02f);
        }
    }

    for (int i = 0; i < kMageMeteorFlameMaterialCount; ++i)
    {
        const std::string materialName = "SkillFx_MageMeteorFlameMat0" + std::to_string(i + 1);
        if (resources->GetMaterial(materialName) == nullptr)
        {
            resources->CreateMaterial(
                materialName,
                static_cast<int>(resources->mMaterials.size()),
                mageMeteorFlameTextureNames[i],
                "",
                "",
                "",
                XMFLOAT4(1.0f, 1.0f, 1.0f, 0.96f),
                XMFLOAT3(0.08f, 0.05f, 0.02f),
                0.02f);
        }
    }

    const std::string archerCircleTextureName =
        resources->GetTexture("Effect_ArcherBuff_Circle4") != nullptr ? "Effect_ArcherBuff_Circle4" :
        (resources->GetTexture("Effect_ArcherWind_Twirl02") != nullptr ? "Effect_ArcherWind_Twirl02" :
            (resources->GetTexture("Effect_Circle03") != nullptr ? "Effect_Circle03" :
                (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white")));
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
        resources->GetTexture("Effect_ArcherWind_Trail00") != nullptr ? "Effect_ArcherWind_Trail00" :
        (resources->GetTexture("Effect_ArcherWind_Trace03") != nullptr ? "Effect_ArcherWind_Trace03" :
            (resources->GetTexture("Effect_ArcherWind_Trace04") != nullptr ? "Effect_ArcherWind_Trace04" :
                (resources->GetTexture("WindRibbon_Archer") != nullptr ? "WindRibbon_Archer" :
                    (resources->GetTexture("MagicCircle") != nullptr ? "MagicCircle" : "white"))));
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
        resources->GetTexture("Effect_ArcherBuff_Whirl2") != nullptr ? "Effect_ArcherBuff_Whirl2" :
        (resources->GetTexture("Effect_ArcherWind_Trace04") != nullptr ? "Effect_ArcherWind_Trace04" :
            (resources->GetTexture("Effect_Scratch01") != nullptr ? "Effect_Scratch01" : "white"));
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

    const std::string archerBuffPointTextureName =
        resources->GetTexture("Effect_ArcherBuff_Point") != nullptr ? "Effect_ArcherBuff_Point" :
        (resources->GetTexture("Effect_MageHeal_Point") != nullptr ? "Effect_MageHeal_Point" : "white");
    if (resources->GetMaterial("SkillFx_ArcherBuffPointMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherBuffPointMat",
            static_cast<int>(resources->mMaterials.size()),
            archerBuffPointTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.06f, 1.32f, 1.14f, 0.96f),
            XMFLOAT3(0.08f, 0.12f, 0.10f),
            0.02f);
    }

    const std::string archerBuffArrowTextureName =
        resources->GetTexture("Effect_ArcherBuff_Arrow") != nullptr ? "Effect_ArcherBuff_Arrow" :
        archerBuffPointTextureName;
    if (resources->GetMaterial("SkillFx_ArcherBuffArrowMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_ArcherBuffArrowMat",
            static_cast<int>(resources->mMaterials.size()),
            archerBuffArrowTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.10f, 1.36f, 1.18f, 0.96f),
            XMFLOAT3(0.08f, 0.12f, 0.10f),
            0.02f);
    }

    const std::string warriorBasicSlashTextureName =
        resources->GetTexture("Effect_WarriorBasic_Slash") != nullptr ? "Effect_WarriorBasic_Slash" :
        (resources->GetTexture("Effect_Scratch01") != nullptr ? "Effect_Scratch01" : "white");
    if (resources->GetMaterial("SkillFx_WarriorBasicSlashMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_WarriorBasicSlashMat",
            static_cast<int>(resources->mMaterials.size()),
            warriorBasicSlashTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.18f, 1.18f, 1.18f, 0.98f),
            XMFLOAT3(0.10f, 0.10f, 0.10f),
            0.02f);
    }

    const std::string warriorBasicMaskTextureName =
        resources->GetTexture("Effect_WarriorBasic_Mask") != nullptr ? "Effect_WarriorBasic_Mask" :
        warriorBasicSlashTextureName;
    if (resources->GetMaterial("SkillFx_WarriorBasicMaskMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_WarriorBasicMaskMat",
            static_cast<int>(resources->mMaterials.size()),
            warriorBasicMaskTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.04f, 1.04f, 1.04f, 0.92f),
            XMFLOAT3(0.08f, 0.08f, 0.08f),
            0.02f);
    }

    if (resources->GetMaterial("SkillFx_WarriorSwordTrailMat") == nullptr)
    {
        resources->CreateMaterial(
            "SkillFx_WarriorSwordTrailMat",
            static_cast<int>(resources->mMaterials.size()),
            warriorBasicSlashTextureName,
            "",
            "",
            "",
            XMFLOAT4(1.82f, 1.58f, 1.02f, 0.96f),
            XMFLOAT3(0.08f, 0.07f, 0.05f),
            0.03f);
    }

    const std::string archerSlashTextureName =
        resources->GetTexture("Effect_ArcherWind_Slash02") != nullptr ? "Effect_ArcherWind_Slash02" :
            (resources->GetTexture("Effect_Scratch01") != nullptr ? "Effect_Scratch01" :
                (resources->GetTexture("WindRibbon_Archer") != nullptr ? "WindRibbon_Archer" : "white"));
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

    mEarthshatterStoneMaterial = resources->GetMaterial("SkillFx_EarthshatterStoneMat");
    if (mEarthshatterStoneMaterial != nullptr)
    {
        mEarthshatterStoneMaterial->DiffuseAlbedo = { 1.28f, 1.12f, 0.90f, 1.0f };
        mEarthshatterStoneMaterial->FresnelR0 = { 0.08f, 0.06f, 0.04f };
        mEarthshatterStoneMaterial->Roughness = 0.16f;
        mEarthshatterStoneMaterial->IsTransparent = 1;
        mEarthshatterStoneMaterial->IsToon = 0;
        mEarthshatterStoneMaterial->OutlineThickness = 0.0f;
        mEarthshatterStoneMaterial->NumFramesDirty = gNumFrameResources;
    }

    mEarthshatterSmokeMaterial = resources->GetMaterial("SkillFx_EarthshatterSmokeMat");
    if (mEarthshatterSmokeMaterial != nullptr)
    {
        mEarthshatterSmokeMaterial->DiffuseAlbedo = { 1.18f, 1.00f, 0.84f, 1.0f };
        mEarthshatterSmokeMaterial->FresnelR0 = { 0.05f, 0.05f, 0.05f };
        mEarthshatterSmokeMaterial->Roughness = 0.10f;
        mEarthshatterSmokeMaterial->IsTransparent = 1;
        mEarthshatterSmokeMaterial->IsToon = 0;
        mEarthshatterSmokeMaterial->OutlineThickness = 0.0f;
        mEarthshatterSmokeMaterial->NumFramesDirty = gNumFrameResources;
    }

    mBeamMaterial = resources->GetMaterial("SkillFx_BeamMat");
    if (mBeamMaterial != nullptr)
    {
        mBeamMaterial->IsTransparent = 1;
        mBeamMaterial->IsToon = 0;
        mBeamMaterial->OutlineThickness = 0.0f;
        mBeamMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageBasicOrbCoreMaterial = resources->GetMaterial("SkillFx_MageBasicOrbCoreMat");
    if (mMageBasicOrbCoreMaterial != nullptr)
    {
        mMageBasicOrbCoreMaterial->DiffuseMapName = resources->GetTexture("Blue") != nullptr ? "Blue" : "white";
        mMageBasicOrbCoreMaterial->DiffuseAlbedo = { 0.84f, 1.28f, 2.10f, 0.94f };
        mMageBasicOrbCoreMaterial->FresnelR0 = { 0.14f, 0.22f, 0.30f };
        mMageBasicOrbCoreMaterial->Roughness = 0.01f;
        mMageBasicOrbCoreMaterial->IsTransparent = 1;
        mMageBasicOrbCoreMaterial->IsToon = 0;
        mMageBasicOrbCoreMaterial->OutlineThickness = 0.0f;
        mMageBasicOrbCoreMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageBasicOrbAuraMaterial = resources->GetMaterial("SkillFx_MageBasicOrbAuraMat");
    if (mMageBasicOrbAuraMaterial != nullptr)
    {
        mMageBasicOrbAuraMaterial->DiffuseMapName = mageOrbAuraTextureName;
        mMageBasicOrbAuraMaterial->DiffuseAlbedo = { 0.48f, 1.00f, 1.96f, 0.88f };
        mMageBasicOrbAuraMaterial->FresnelR0 = { 0.10f, 0.16f, 0.22f };
        mMageBasicOrbAuraMaterial->Roughness = 0.01f;
        mMageBasicOrbAuraMaterial->IsTransparent = 1;
        mMageBasicOrbAuraMaterial->IsToon = 0;
        mMageBasicOrbAuraMaterial->OutlineThickness = 0.0f;
        mMageBasicOrbAuraMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageBasicOrbTrailMaterial = resources->GetMaterial("SkillFx_MageBasicOrbTrailMat");
    if (mMageBasicOrbTrailMaterial != nullptr)
    {
        mMageBasicOrbTrailMaterial->DiffuseMapName = mageOrbTrailTextureName;
        mMageBasicOrbTrailMaterial->DiffuseAlbedo = { 0.34f, 0.80f, 1.66f, 0.78f };
        mMageBasicOrbTrailMaterial->FresnelR0 = { 0.08f, 0.12f, 0.18f };
        mMageBasicOrbTrailMaterial->Roughness = 0.01f;
        mMageBasicOrbTrailMaterial->IsTransparent = 1;
        mMageBasicOrbTrailMaterial->IsToon = 0;
        mMageBasicOrbTrailMaterial->OutlineThickness = 0.0f;
        mMageBasicOrbTrailMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageBasicOrbOuterTrailMaterial = resources->GetMaterial("SkillFx_MageBasicOrbOuterTrailMat");
    if (mMageBasicOrbOuterTrailMaterial != nullptr)
    {
        mMageBasicOrbOuterTrailMaterial->DiffuseMapName = mageOrbOuterTrailTextureName;
        mMageBasicOrbOuterTrailMaterial->DiffuseAlbedo = { 0.28f, 0.68f, 1.48f, 0.70f };
        mMageBasicOrbOuterTrailMaterial->FresnelR0 = { 0.06f, 0.10f, 0.16f };
        mMageBasicOrbOuterTrailMaterial->Roughness = 0.01f;
        mMageBasicOrbOuterTrailMaterial->IsTransparent = 1;
        mMageBasicOrbOuterTrailMaterial->IsToon = 0;
        mMageBasicOrbOuterTrailMaterial->OutlineThickness = 0.0f;
        mMageBasicOrbOuterTrailMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageBasicOrbFlashMaterial = resources->GetMaterial("SkillFx_MageBasicOrbFlashMat");
    if (mMageBasicOrbFlashMaterial != nullptr)
    {
        mMageBasicOrbFlashMaterial->DiffuseMapName = mageOrbFlashTextureName;
        mMageBasicOrbFlashMaterial->DiffuseAlbedo = { 0.84f, 1.34f, 2.28f, 0.92f };
        mMageBasicOrbFlashMaterial->FresnelR0 = { 0.12f, 0.18f, 0.24f };
        mMageBasicOrbFlashMaterial->Roughness = 0.01f;
        mMageBasicOrbFlashMaterial->IsTransparent = 1;
        mMageBasicOrbFlashMaterial->IsToon = 0;
        mMageBasicOrbFlashMaterial->OutlineThickness = 0.0f;
        mMageBasicOrbFlashMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageBasicOrbImpactMaterial = resources->GetMaterial("SkillFx_MageBasicOrbImpactMat");
    if (mMageBasicOrbImpactMaterial != nullptr)
    {
        mMageBasicOrbImpactMaterial->DiffuseMapName = mageOrbImpactTextureName;
        mMageBasicOrbImpactMaterial->DiffuseAlbedo = { 0.92f, 1.46f, 2.38f, 0.96f };
        mMageBasicOrbImpactMaterial->FresnelR0 = { 0.14f, 0.22f, 0.28f };
        mMageBasicOrbImpactMaterial->Roughness = 0.01f;
        mMageBasicOrbImpactMaterial->IsTransparent = 1;
        mMageBasicOrbImpactMaterial->IsToon = 0;
        mMageBasicOrbImpactMaterial->OutlineThickness = 0.0f;
        mMageBasicOrbImpactMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageHealSparkleMaterial = resources->GetMaterial("SkillFx_MageHealSparkleMat");
    if (mMageHealSparkleMaterial != nullptr)
    {
        mMageHealSparkleMaterial->DiffuseMapName = mageHealSparkleTextureName;
        mMageHealSparkleMaterial->DiffuseAlbedo = { 0.86f, 1.26f, 2.28f, 0.98f };
        mMageHealSparkleMaterial->FresnelR0 = { 0.08f, 0.14f, 0.22f };
        mMageHealSparkleMaterial->Roughness = 0.01f;
        mMageHealSparkleMaterial->IsTransparent = 1;
        mMageHealSparkleMaterial->IsToon = 0;
        mMageHealSparkleMaterial->OutlineThickness = 0.0f;
        mMageHealSparkleMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageHealSmokeMaterial = resources->GetMaterial("SkillFx_MageHealSmokeMat");
    if (mMageHealSmokeMaterial != nullptr)
    {
        mMageHealSmokeMaterial->DiffuseMapName = mageHealSmokeTextureName;
        mMageHealSmokeMaterial->DiffuseAlbedo = { 0.68f, 1.14f, 0.90f, 0.90f };
        mMageHealSmokeMaterial->FresnelR0 = { 0.04f, 0.08f, 0.05f };
        mMageHealSmokeMaterial->Roughness = 0.01f;
        mMageHealSmokeMaterial->IsTransparent = 1;
        mMageHealSmokeMaterial->IsToon = 0;
        mMageHealSmokeMaterial->OutlineThickness = 0.0f;
        mMageHealSmokeMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageHealPointMaterial = resources->GetMaterial("SkillFx_MageHealPointMat");
    if (mMageHealPointMaterial != nullptr)
    {
        mMageHealPointMaterial->DiffuseMapName = mageHealPointTextureName;
        mMageHealPointMaterial->DiffuseAlbedo = { 0.90f, 1.40f, 2.46f, 1.00f };
        mMageHealPointMaterial->FresnelR0 = { 0.08f, 0.14f, 0.22f };
        mMageHealPointMaterial->Roughness = 0.01f;
        mMageHealPointMaterial->IsTransparent = 1;
        mMageHealPointMaterial->IsToon = 0;
        mMageHealPointMaterial->OutlineThickness = 0.0f;
        mMageHealPointMaterial->NumFramesDirty = gNumFrameResources;
    }

    mMageMeteorCircleMaterial = resources->GetMaterial("SkillFx_MageMeteorCircleMat");
    if (mMageMeteorCircleMaterial != nullptr)
    {
        mMageMeteorCircleMaterial->DiffuseMapName = mageMeteorCircleTextureName;
        mMageMeteorCircleMaterial->DiffuseAlbedo = { 1.54f, 0.42f, 0.42f, 1.0f };
        mMageMeteorCircleMaterial->FresnelR0 = { 0.14f, 0.03f, 0.03f };
        mMageMeteorCircleMaterial->Roughness = 0.01f;
        mMageMeteorCircleMaterial->IsTransparent = 1;
        mMageMeteorCircleMaterial->IsToon = 0;
        mMageMeteorCircleMaterial->OutlineThickness = 0.0f;
        mMageMeteorCircleMaterial->NumFramesDirty = gNumFrameResources;
    }

    const XMFLOAT4 shockwaveDiffuse[kMageMeteorShockwaveMaterialCount] =
    {
        XMFLOAT4(1.92f, 1.04f, 0.42f, 1.00f),
        XMFLOAT4(2.16f, 1.18f, 0.46f, 1.00f),
        XMFLOAT4(1.78f, 0.78f, 0.28f, 0.98f),
        XMFLOAT4(1.28f, 0.38f, 0.16f, 0.94f)
    };

    for (int i = 0; i < kMageMeteorShockwaveMaterialCount; ++i)
    {
        const std::string materialName = "SkillFx_MageMeteorShockwaveMat0" + std::to_string(i + 1);
        mMageMeteorShockwaveMaterials[i] = resources->GetMaterial(materialName);
        if (mMageMeteorShockwaveMaterials[i] == nullptr)
        {
            continue;
        }

        mMageMeteorShockwaveMaterials[i]->DiffuseMapName = mageMeteorShockwaveTextureNames[i];
        mMageMeteorShockwaveMaterials[i]->DiffuseAlbedo = shockwaveDiffuse[i];
        mMageMeteorShockwaveMaterials[i]->FresnelR0 = { 0.10f, 0.06f, 0.02f };
        mMageMeteorShockwaveMaterials[i]->Roughness = 0.01f;
        mMageMeteorShockwaveMaterials[i]->IsTransparent = 1;
        mMageMeteorShockwaveMaterials[i]->IsToon = 0;
        mMageMeteorShockwaveMaterials[i]->OutlineThickness = 0.0f;
        mMageMeteorShockwaveMaterials[i]->NumFramesDirty = gNumFrameResources;
    }
    mMageMeteorShockwaveMaterial = mMageMeteorShockwaveMaterials[0];

    const XMFLOAT4 meteorFlameDiffuse[kMageMeteorFlameMaterialCount] =
    {
        XMFLOAT4(1.18f, 0.20f, 0.06f, 1.00f),
        XMFLOAT4(1.30f, 0.24f, 0.07f, 1.00f),
        XMFLOAT4(0.96f, 0.12f, 0.04f, 0.98f),
        XMFLOAT4(1.46f, 0.30f, 0.09f, 1.00f),
        XMFLOAT4(1.24f, 0.22f, 0.06f, 1.00f),
        XMFLOAT4(1.04f, 0.16f, 0.05f, 0.98f),
        XMFLOAT4(0.78f, 0.10f, 0.04f, 0.96f),
        XMFLOAT4(0.56f, 0.06f, 0.03f, 0.92f)
    };
    const XMFLOAT3 meteorFlameFresnel[kMageMeteorFlameMaterialCount] =
    {
        XMFLOAT3(0.10f, 0.02f, 0.02f),
        XMFLOAT3(0.12f, 0.03f, 0.02f),
        XMFLOAT3(0.08f, 0.02f, 0.02f),
        XMFLOAT3(0.16f, 0.04f, 0.03f),
        XMFLOAT3(0.13f, 0.03f, 0.02f),
        XMFLOAT3(0.10f, 0.02f, 0.02f),
        XMFLOAT3(0.08f, 0.02f, 0.02f),
        XMFLOAT3(0.06f, 0.01f, 0.01f)
    };
    for (int i = 0; i < kMageMeteorFlameMaterialCount; ++i)
    {
        const std::string materialName = "SkillFx_MageMeteorFlameMat0" + std::to_string(i + 1);
        mMageMeteorFlameMaterials[i] = resources->GetMaterial(materialName);
        if (mMageMeteorFlameMaterials[i] == nullptr)
        {
            continue;
        }

        mMageMeteorFlameMaterials[i]->DiffuseMapName = mageMeteorFlameTextureNames[i];
        mMageMeteorFlameMaterials[i]->DiffuseAlbedo = meteorFlameDiffuse[i];
        mMageMeteorFlameMaterials[i]->FresnelR0 = meteorFlameFresnel[i];
        mMageMeteorFlameMaterials[i]->Roughness = 0.01f;
        mMageMeteorFlameMaterials[i]->IsTransparent = 1;
        mMageMeteorFlameMaterials[i]->IsToon = 0;
        mMageMeteorFlameMaterials[i]->OutlineThickness = 0.0f;
        mMageMeteorFlameMaterials[i]->NumFramesDirty = gNumFrameResources;
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

    mArcherBuffPointMaterial = resources->GetMaterial("SkillFx_ArcherBuffPointMat");
    if (mArcherBuffPointMaterial != nullptr)
    {
        mArcherBuffPointMaterial->DiffuseMapName = archerBuffPointTextureName;
        mArcherBuffPointMaterial->DiffuseAlbedo = { 1.30f, 1.76f, 1.48f, 0.98f };
        mArcherBuffPointMaterial->FresnelR0 = { 0.10f, 0.14f, 0.12f };
        mArcherBuffPointMaterial->Roughness = 0.01f;
        mArcherBuffPointMaterial->IsTransparent = 1;
        mArcherBuffPointMaterial->IsToon = 0;
        mArcherBuffPointMaterial->OutlineThickness = 0.0f;
        mArcherBuffPointMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherBuffArrowMaterial = resources->GetMaterial("SkillFx_ArcherBuffArrowMat");
    if (mArcherBuffArrowMaterial != nullptr)
    {
        mArcherBuffArrowMaterial->DiffuseMapName = archerBuffArrowTextureName;
        mArcherBuffArrowMaterial->DiffuseAlbedo = { 1.34f, 1.84f, 1.56f, 0.98f };
        mArcherBuffArrowMaterial->FresnelR0 = { 0.10f, 0.14f, 0.12f };
        mArcherBuffArrowMaterial->Roughness = 0.01f;
        mArcherBuffArrowMaterial->IsTransparent = 1;
        mArcherBuffArrowMaterial->IsToon = 0;
        mArcherBuffArrowMaterial->OutlineThickness = 0.0f;
        mArcherBuffArrowMaterial->NumFramesDirty = gNumFrameResources;
    }

    mWarriorBasicSlashMaterial = resources->GetMaterial("SkillFx_WarriorBasicSlashMat");
    if (mWarriorBasicSlashMaterial != nullptr)
    {
        mWarriorBasicSlashMaterial->DiffuseMapName = warriorBasicSlashTextureName;
        mWarriorBasicSlashMaterial->DiffuseAlbedo = { 1.34f, 1.34f, 1.34f, 0.98f };
        mWarriorBasicSlashMaterial->FresnelR0 = { 0.10f, 0.10f, 0.10f };
        mWarriorBasicSlashMaterial->Roughness = 0.01f;
        mWarriorBasicSlashMaterial->IsTransparent = 1;
        mWarriorBasicSlashMaterial->IsToon = 0;
        mWarriorBasicSlashMaterial->OutlineThickness = 0.0f;
        mWarriorBasicSlashMaterial->NumFramesDirty = gNumFrameResources;
    }

    mWarriorBasicMaskMaterial = resources->GetMaterial("SkillFx_WarriorBasicMaskMat");
    if (mWarriorBasicMaskMaterial != nullptr)
    {
        mWarriorBasicMaskMaterial->DiffuseMapName = warriorBasicMaskTextureName;
        mWarriorBasicMaskMaterial->DiffuseAlbedo = { 1.18f, 1.18f, 1.18f, 0.98f };
        mWarriorBasicMaskMaterial->FresnelR0 = { 0.08f, 0.08f, 0.08f };
        mWarriorBasicMaskMaterial->Roughness = 0.01f;
        mWarriorBasicMaskMaterial->IsTransparent = 1;
        mWarriorBasicMaskMaterial->IsToon = 0;
        mWarriorBasicMaskMaterial->OutlineThickness = 0.0f;
        mWarriorBasicMaskMaterial->NumFramesDirty = gNumFrameResources;
    }

    mWarriorSwordTrailMaterial = resources->GetMaterial("SkillFx_WarriorSwordTrailMat");
    if (mWarriorSwordTrailMaterial != nullptr)
    {
        mWarriorSwordTrailMaterial->DiffuseMapName = warriorBasicSlashTextureName;
        mWarriorSwordTrailMaterial->DiffuseAlbedo = { 1.82f, 1.58f, 1.02f, 0.96f };
        mWarriorSwordTrailMaterial->FresnelR0 = { 0.10f, 0.08f, 0.05f };
        mWarriorSwordTrailMaterial->Roughness = 0.02f;
        mWarriorSwordTrailMaterial->IsTransparent = 1;
        mWarriorSwordTrailMaterial->IsToon = 0;
        mWarriorSwordTrailMaterial->OutlineThickness = 0.0f;
        mWarriorSwordTrailMaterial->NumFramesDirty = gNumFrameResources;
    }

    mArcherWindMaterial = resources->GetMaterial("SkillFx_ArcherWindMat");
    if (mArcherWindMaterial != nullptr)
    {
        mArcherWindMaterial->DiffuseMapName = archerWindTextureName;
        mArcherWindMaterial->DiffuseAlbedo = { 1.58f, 2.28f, 1.86f, 1.0f };
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
        else if (style == EffectStyle::MageBasicOrb)
        {
            renderItem->Mat = mMageBasicOrbAuraMaterial != nullptr ? mMageBasicOrbAuraMaterial : mBeamMaterial;
        }
        else if (style == EffectStyle::ArcherWindRibbon)
        {
            renderItem->Mat = mArcherWindMaterial != nullptr ? mArcherWindMaterial : mBeamMaterial;
        }
        else if (style == EffectStyle::WarriorSwordTrailSegment)
        {
            renderItem->Mat = mWarriorSwordTrailMaterial != nullptr ? mWarriorSwordTrailMaterial : mBeamMaterial;
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

    for (int i = 0; i < kMageBasicOrbPoolSize; ++i)
    {
        CreateEffect(EffectStyle::MageBasicOrb);
    }

    for (int i = 0; i < kArcherWindRibbonPoolSize; ++i)
    {
        CreateEffect(EffectStyle::ArcherWindRibbon);
    }

    for (int i = 0; i < kWarriorSwordTrailSegmentPoolSize; ++i)
    {
        CreateEffect(EffectStyle::WarriorSwordTrailSegment);
    }

    EnsureMageBasicOrbCorePool();
    EnsureArcherArrowRainPool();
    EnsureArcherBuffLoopVisuals();
}

void SkillEffectManager::EnsureMageBasicOrbCorePool()
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
            return effect.Style == EffectStyle::MageBasicOrbCore;
        });
    if (alreadyCreated)
    {
        return;
    }

    auto* resources = mGame->GetResources();
    auto geoIt = resources->mGeometries.find("sphereGeo");
    if (geoIt == resources->mGeometries.end() || geoIt->second == nullptr)
    {
        return;
    }

    Material* material = mMageBasicOrbCoreMaterial != nullptr ? mMageBasicOrbCoreMaterial : resources->GetMaterial("SkillFx_MageBasicOrbCoreMat");
    if (material == nullptr)
    {
        return;
    }

    auto submeshIt = geoIt->second->DrawArgs.find("sphere");
    if (submeshIt == geoIt->second->DrawArgs.end())
    {
        return;
    }

    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    for (int i = 0; i < kMageBasicOrbCorePoolSize; ++i)
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
        object->SetRotation(0.0f, 0.0f, 0.0f);
        object->Update();

        EffectInstance instance;
        instance.Object = object.get();
        instance.Ritem = renderItem.get();
        instance.Style = EffectStyle::MageBasicOrbCore;
        mEffects.push_back(instance);

        if (mTrackOwned)
        {
            mTrackOwned(object.get(), renderItem.get());
        }

        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    }
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
    effect.SpinRate = 0.0f;
    effect.UseLinearMotion = false;
    effect.UseStyleAnimation = true;

    if (effect.Object != nullptr)
    {
        effect.Object->mIsAnimated = false;
        effect.Object->mIsBillboard = false;
        effect.Object->ClearWorldTransformOverride();
    }

    if (effect.Ritem != nullptr)
    {
        effect.Ritem->Visible = false;
        effect.Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
        effect.Ritem->TexTransform = MathHelper::Identity4x4();
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
    Material* materialOverride,
    float spinRate,
    float fadeOutDuration,
    float startDelay)
{
    EffectInstance* effect = AcquireEffect(EffectStyle::GroundDecal);
    if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
    {
        return;
    }

    effect->Style = EffectStyle::GroundDecal;
    effect->Active = true;
    effect->Age = 0.0f;
    const float clampedStartDelay = (std::max)(startDelay, 0.0f);
    effect->LifeTime = clampedStartDelay + lifeTime;
    effect->BasePosition = position;
    effect->Velocity = { 0.0f, 0.0f, 0.0f };
    effect->StartScale = { startScale, startScale, 1.0f };
    effect->EndScale = { endScale, endScale, 1.0f };
    effect->StartColor = startColor;
    effect->EndColor = endColor;
    effect->RotX = XM_PIDIV2;
    effect->RotY = rotY;
    effect->RotZ = 0.0f;
    effect->StartDelay = clampedStartDelay;
    effect->SpinRate = spinRate;
    effect->FadeStartTime = fadeOutDuration > 0.0f
        ? clampedStartDelay + (std::max)(0.0f, lifeTime - fadeOutDuration)
        : 0.0f;
    effect->MotionDuration = 0.0f;
    effect->UseLinearMotion = false;
    effect->UseStyleAnimation = true;

    effect->Object->mIsBillboard = false;
    effect->Object->mIsAnimated = false;
    effect->Object->SetPosition(position.x, position.y, position.z);
    effect->Object->SetScale(startScale, startScale, 1.0f);
    effect->Object->SetRotation(effect->RotX, effect->RotY, effect->RotZ);
    effect->Object->Update();

    effect->Ritem->Mat = materialOverride != nullptr ? materialOverride : mDecalMaterial;
    effect->Ritem->Visible = clampedStartDelay <= 0.0001f;
    effect->Ritem->CastShadow = false;
    effect->Ritem->ColorMultiplier = effect->Ritem->Visible
        ? startColor
        : XMFLOAT4(startColor.x, startColor.y, startColor.z, 0.0f);
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

void SkillEffectManager::SpawnWarriorBasicAttackEffect(const XMFLOAT3& position, float rotY, int attackVariant)
{
    const XMFLOAT3 playerForward = ForwardFromYaw(rotY);
    const float sideSign = attackVariant == 2 ? 1.0f : -1.0f;
    XMFLOAT3 weaponBasePosition = { position.x, position.y + 0.88f, position.z };
    XMFLOAT3 forward = playerForward;
    if (mGame != nullptr && mGame->GetPlayerWeaponObject() != nullptr)
    {
        const XMFLOAT4X4& weaponWorld = mGame->GetPlayerWeaponObject()->World;
        weaponBasePosition = { weaponWorld._41, weaponWorld._42, weaponWorld._43 };
    }

    if (mTrackedWarriorWeaponTipWorldValid)
    {
        const XMFLOAT3 fallbackForward = playerForward;
        const XMFLOAT3 tipDelta = Subtract3(mTrackedWarriorWeaponTipWorld, mPreviousWarriorWeaponTipWorld);
        forward = NormalizeOr(tipDelta, fallbackForward);
        weaponBasePosition = mTrackedWarriorWeaponTipWorld;
    }

    XMFLOAT3 right = NormalizeOr(Cross3({ 0.0f, 1.0f, 0.0f }, forward), RightFromYaw(rotY));
    if (LengthSq3(right) <= 0.000001f)
    {
        right = RightFromYaw(rotY);
    }

    const float swingYaw = std::atan2(forward.x, forward.z);
    const float slashYawOffset = swingYaw - rotY;
    const float slashRoll = -1.04f * sideSign;
    const XMFLOAT4 slashColor = { 1.44f, 1.44f, 1.44f, 0.94f };
    const XMFLOAT4 slashFade = { 0.54f, 0.54f, 0.54f, 0.0f };

    const XMFLOAT3 slashCenter =
    {
        weaponBasePosition.x - forward.x * 0.05f + right.x * 0.05f * sideSign,
        weaponBasePosition.y + 0.00f,
        weaponBasePosition.z - forward.z * 0.05f + right.z * 0.05f * sideSign
    };
    const XMFLOAT3 slashVelocity =
    {
        forward.x * 0.52f + right.x * 0.08f * sideSign,
        forward.y * 0.52f + 0.04f,
        forward.z * 0.52f + right.z * 0.08f * sideSign
    };

    SpawnArcherWindRibbon(
        slashCenter,
        slashVelocity,
        0.74f,
        0.96f,
        0.86f,
        1.08f,
        0.24f,
        slashColor,
        slashFade,
        slashYawOffset,
        slashRoll,
        mWarriorBasicSlashMaterial);
}

void SkillEffectManager::StartWarriorBasicSwordTrail(const XMFLOAT3& origin, float rotY, int attackVariant)
{
    mWarriorSwordTrailFallbackOrigin = origin;
    mWarriorSwordTrailFallbackRotY = rotY;
    mWarriorSwordTrailVariant = attackVariant == 2 ? 2 : 1;
    mWarriorSwordTrailElapsed = 0.0f;
    mWarriorSwordTrailEmitTimer = 0.0f;
    mWarriorSwordTrailEmitAnchorValid = false;
    mWarriorSwordTrailSkippedFirstEmit = false;
    mWarriorSwordTrailSpawnedSegment = false;

    const float startDelay = mWarriorSwordTrailVariant == 2
        ? kWarriorSwordTrailAttack2StartDelay
        : kWarriorSwordTrailAttack1StartDelay;
    const float emitDuration = mWarriorSwordTrailVariant == 2
        ? kWarriorSwordTrailAttack2EmitDuration
        : kWarriorSwordTrailAttack1EmitDuration;
    mWarriorSwordTrailTotalDuration = startDelay + emitDuration;

    UpdateWarriorWeaponTrailState();
}

void SkillEffectManager::UpdateWarriorBasicSwordTrail(float dt)
{
    if (mWarriorSwordTrailTotalDuration <= 0.0f)
    {
        return;
    }

    mWarriorSwordTrailElapsed += dt;

    const float startDelay = mWarriorSwordTrailVariant == 2
        ? kWarriorSwordTrailAttack2StartDelay
        : kWarriorSwordTrailAttack1StartDelay;
    const float emitDuration = mWarriorSwordTrailVariant == 2
        ? kWarriorSwordTrailAttack2EmitDuration
        : kWarriorSwordTrailAttack1EmitDuration;
    const float endTime = startDelay + emitDuration;

    if (mWarriorSwordTrailElapsed >= endTime)
    {
        mWarriorSwordTrailTotalDuration = 0.0f;
        mWarriorSwordTrailEmitTimer = 0.0f;
        mWarriorSwordTrailEmitAnchorValid = false;
        mWarriorSwordTrailSkippedFirstEmit = false;
        mWarriorSwordTrailSpawnedSegment = false;
        return;
    }

    if (mWarriorSwordTrailElapsed < startDelay)
    {
        return;
    }

    const XMFLOAT3 fallbackForward = ForwardFromYaw(mWarriorSwordTrailFallbackRotY);
    XMFLOAT3 weaponTipPosition =
    {
        mWarriorSwordTrailFallbackOrigin.x + fallbackForward.x * 0.55f,
        mWarriorSwordTrailFallbackOrigin.y + 0.94f,
        mWarriorSwordTrailFallbackOrigin.z + fallbackForward.z * 0.55f
    };
    XMFLOAT3 weaponInnerPosition =
    {
        mWarriorSwordTrailFallbackOrigin.x + fallbackForward.x * 0.30f,
        mWarriorSwordTrailFallbackOrigin.y + 0.74f,
        mWarriorSwordTrailFallbackOrigin.z + fallbackForward.z * 0.30f
    };

    if (mTrackedWarriorWeaponTipWorldValid)
    {
        weaponTipPosition = mTrackedWarriorWeaponTipWorld;
        weaponInnerPosition = mTrackedWarriorWeaponInnerWorld;
    }

    if (!mWarriorSwordTrailEmitAnchorValid)
    {
        mWarriorSwordTrailLastEmitTipWorld = mTrackedWarriorWeaponTipWorldValid
            ? mPreviousWarriorWeaponTipWorld
            : weaponTipPosition;
        mWarriorSwordTrailLastEmitInnerWorld = mTrackedWarriorWeaponTipWorldValid
            ? mPreviousWarriorWeaponInnerWorld
            : weaponInnerPosition;
        mWarriorSwordTrailEmitAnchorValid = true;
    }

    mWarriorSwordTrailEmitTimer -= dt;
    if (mWarriorSwordTrailEmitTimer > 0.0f)
    {
        return;
    }
    mWarriorSwordTrailEmitTimer = kWarriorSwordTrailEmitInterval;

    const float emitT = (std::clamp)((mWarriorSwordTrailElapsed - startDelay) / (std::max)(emitDuration, 0.0001f), 0.0f, 1.0f);
    if (mWarriorSwordTrailVariant == 1 && !mWarriorSwordTrailSkippedFirstEmit)
    {
        mWarriorSwordTrailSkippedFirstEmit = true;
        mWarriorSwordTrailLastEmitTipWorld = weaponTipPosition;
        mWarriorSwordTrailLastEmitInnerWorld = weaponInnerPosition;
        return;
    }

    const ClassTier weaponTier = mGame != nullptr ? mGame->GetSelectedWeaponTier() : ClassTier::Tier1;
    const XMFLOAT4 startColor = WarriorSwordTrailStartColor(weaponTier, emitT);
    const XMFLOAT4 endColor = WarriorSwordTrailEndColor(weaponTier);

    SpawnWarriorSwordTrailSegment(
        mWarriorSwordTrailLastEmitTipWorld,
        weaponTipPosition,
        mWarriorSwordTrailLastEmitInnerWorld,
        weaponInnerPosition,
        emitT,
        startColor,
        endColor);

    mWarriorSwordTrailSpawnedSegment = true;
    mWarriorSwordTrailLastEmitTipWorld = weaponTipPosition;
    mWarriorSwordTrailLastEmitInnerWorld = weaponInnerPosition;
}

void SkillEffectManager::FlushWarriorBasicSwordTrailBeforeHitStop()
{
    if (mWarriorSwordTrailTotalDuration <= 0.0f || mWarriorSwordTrailSpawnedSegment)
    {
        return;
    }

    const float startDelay = mWarriorSwordTrailVariant == 2
        ? kWarriorSwordTrailAttack2StartDelay
        : kWarriorSwordTrailAttack1StartDelay;
    const float emitDuration = mWarriorSwordTrailVariant == 2
        ? kWarriorSwordTrailAttack2EmitDuration
        : kWarriorSwordTrailAttack1EmitDuration;

    if (mWarriorSwordTrailElapsed < startDelay)
    {
        return;
    }

    UpdateWarriorWeaponTrailState();

    const XMFLOAT3 fallbackForward = ForwardFromYaw(mWarriorSwordTrailFallbackRotY);
    XMFLOAT3 weaponTipPosition =
    {
        mWarriorSwordTrailFallbackOrigin.x + fallbackForward.x * 0.55f,
        mWarriorSwordTrailFallbackOrigin.y + 0.94f,
        mWarriorSwordTrailFallbackOrigin.z + fallbackForward.z * 0.55f
    };
    XMFLOAT3 weaponInnerPosition =
    {
        mWarriorSwordTrailFallbackOrigin.x + fallbackForward.x * 0.30f,
        mWarriorSwordTrailFallbackOrigin.y + 0.74f,
        mWarriorSwordTrailFallbackOrigin.z + fallbackForward.z * 0.30f
    };

    if (mTrackedWarriorWeaponTipWorldValid)
    {
        weaponTipPosition = mTrackedWarriorWeaponTipWorld;
        weaponInnerPosition = mTrackedWarriorWeaponInnerWorld;
    }

    if (!mWarriorSwordTrailEmitAnchorValid)
    {
        mWarriorSwordTrailLastEmitTipWorld = mTrackedWarriorWeaponTipWorldValid
            ? mPreviousWarriorWeaponTipWorld
            : weaponTipPosition;
        mWarriorSwordTrailLastEmitInnerWorld = mTrackedWarriorWeaponTipWorldValid
            ? mPreviousWarriorWeaponInnerWorld
            : weaponInnerPosition;
        mWarriorSwordTrailEmitAnchorValid = true;
    }

    if (mWarriorSwordTrailVariant == 1 && !mWarriorSwordTrailSkippedFirstEmit)
    {
        mWarriorSwordTrailSkippedFirstEmit = true;
    }

    const float emitT = (std::clamp)(
        (mWarriorSwordTrailElapsed - startDelay) / (std::max)(emitDuration, 0.0001f),
        0.0f,
        1.0f);
    const ClassTier weaponTier = mGame != nullptr ? mGame->GetSelectedWeaponTier() : ClassTier::Tier1;
    const XMFLOAT4 startColor = WarriorSwordTrailStartColor(weaponTier, emitT);
    const XMFLOAT4 endColor = WarriorSwordTrailEndColor(weaponTier);

    SpawnWarriorSwordTrailSegment(
        mWarriorSwordTrailLastEmitTipWorld,
        weaponTipPosition,
        mWarriorSwordTrailLastEmitInnerWorld,
        weaponInnerPosition,
        emitT,
        startColor,
        endColor);

    mWarriorSwordTrailSpawnedSegment = true;
    mWarriorSwordTrailLastEmitTipWorld = weaponTipPosition;
    mWarriorSwordTrailLastEmitInnerWorld = weaponInnerPosition;
    mWarriorSwordTrailTotalDuration = 0.0f;
    mWarriorSwordTrailEmitTimer = 0.0f;
    mWarriorSwordTrailEmitAnchorValid = false;
}

void SkillEffectManager::SpawnWarriorSwordTrailSegment(
    const XMFLOAT3& previousTipPosition,
    const XMFLOAT3& currentTipPosition,
    const XMFLOAT3& previousInnerPosition,
    const XMFLOAT3& currentInnerPosition,
    float emitT,
    const XMFLOAT4& startColor,
    const XMFLOAT4& endColor)
{
    const XMFLOAT3 segment = Subtract3(currentTipPosition, previousTipPosition);
    const float segmentLengthSq = LengthSq3(segment);
    if (segmentLengthSq <= kWarriorSwordTrailSegmentMinLength * kWarriorSwordTrailSegmentMinLength)
    {
        return;
    }

    const float segmentLength = std::sqrt(segmentLengthSq);
    const XMFLOAT3 segmentDirection = NormalizeOr(segment, ForwardFromYaw(mWarriorSwordTrailFallbackRotY));
    const XMFLOAT3 tipMid =
    {
        (previousTipPosition.x + currentTipPosition.x) * 0.5f,
        (previousTipPosition.y + currentTipPosition.y) * 0.5f + kWarriorSwordTrailVerticalOffset,
        (previousTipPosition.z + currentTipPosition.z) * 0.5f
    };
    const XMFLOAT3 innerMid =
    {
        (previousInnerPosition.x + currentInnerPosition.x) * 0.5f,
        (previousInnerPosition.y + currentInnerPosition.y) * 0.5f + kWarriorSwordTrailVerticalOffset,
        (previousInnerPosition.z + currentInnerPosition.z) * 0.5f
    };

    XMFLOAT3 bladeDirection = NormalizeOr(Subtract3(innerMid, tipMid), { 0.0f, -1.0f, 0.0f });
    XMFLOAT3 normal = NormalizeOr(Cross3(segmentDirection, bladeDirection), { 0.0f, 1.0f, 0.0f });
    if (LengthSq3(normal) <= 0.000001f)
    {
        normal = NormalizeOr(Cross3(segmentDirection, RightFromYaw(mWarriorSwordTrailFallbackRotY)), { 0.0f, 1.0f, 0.0f });
        bladeDirection = NormalizeOr(Cross3(normal, segmentDirection), bladeDirection);
    }

    const float lengthPadding = kWarriorSwordTrailStartLengthPadding +
        (kWarriorSwordTrailEndLengthPadding - kWarriorSwordTrailStartLengthPadding) * emitT;
    const float bladeSpan = kWarriorSwordTrailStartBladeSpan +
        (kWarriorSwordTrailEndBladeSpan - kWarriorSwordTrailStartBladeSpan) * emitT;
    const float halfLength = segmentLength * 0.5f + lengthPadding;
    const float halfBladeSpan = bladeSpan * 0.5f;

    const auto spawnPlane =
        [&](const XMFLOAT3& planeNormal, const XMFLOAT3& planeBladeDirection, const XMFLOAT4& planeStartColor, const XMFLOAT4& planeEndColor)
    {
        EffectInstance* effect = AcquireEffect(EffectStyle::WarriorSwordTrailSegment);
        if (effect == nullptr || effect->Object == nullptr || effect->Ritem == nullptr)
        {
            return;
        }

        const XMFLOAT3 center =
        {
            tipMid.x + planeBladeDirection.x * halfBladeSpan,
            tipMid.y + planeBladeDirection.y * halfBladeSpan,
            tipMid.z + planeBladeDirection.z * halfBladeSpan
        };

        const XMFLOAT3 xAxis =
        {
            segmentDirection.x * halfLength,
            segmentDirection.y * halfLength,
            segmentDirection.z * halfLength
        };
        const XMFLOAT3 yAxis =
        {
            planeBladeDirection.x * halfBladeSpan,
            planeBladeDirection.y * halfBladeSpan,
            planeBladeDirection.z * halfBladeSpan
        };

        const XMMATRIX world = XMMatrixSet(
            xAxis.x, xAxis.y, xAxis.z, 0.0f,
            yAxis.x, yAxis.y, yAxis.z, 0.0f,
            planeNormal.x, planeNormal.y, planeNormal.z, 0.0f,
            center.x, center.y, center.z, 1.0f);

        effect->Style = EffectStyle::WarriorSwordTrailSegment;
        effect->Active = true;
        effect->Age = 0.0f;
        effect->LifeTime = (std::max)(kWarriorSwordTrailSegmentLifeTime, 0.04f);
        effect->BasePosition = center;
        effect->Velocity = { 0.0f, 0.0f, 0.0f };
        effect->StartScale = { 1.0f, 1.0f, 1.0f };
        effect->EndScale = effect->StartScale;
        effect->StartColor = planeStartColor;
        effect->EndColor = planeEndColor;
        effect->RotX = 0.0f;
        effect->RotY = 0.0f;
        effect->RotZ = 0.0f;
        effect->StartDelay = 0.0f;
        effect->MotionDuration = 0.0f;
        effect->FadeStartTime = 0.0f;
        effect->SpinRate = 0.0f;
        effect->UseLinearMotion = false;
        effect->UseStyleAnimation = false;

        effect->Object->mIsBillboard = false;
        effect->Object->mIsAnimated = false;
        effect->Object->SetWorldTransform(world);

        effect->Ritem->Mat = mWarriorSwordTrailMaterial != nullptr
            ? mWarriorSwordTrailMaterial
            : (mWarriorBasicSlashMaterial != nullptr ? mWarriorBasicSlashMaterial : mArcherWindMaterial);
        effect->Ritem->Visible = true;
        effect->Ritem->CastShadow = false;
        effect->Ritem->ColorMultiplier = planeStartColor;
        effect->Ritem->NumFramesDirty = gNumFrameResources;
    };

    spawnPlane(normal, bladeDirection, startColor, endColor);

}

void SkillEffectManager::UpdateWarriorWeaponTrailState()
{
    auto* weaponObject = (mGame != nullptr) ? mGame->GetPlayerWeaponObject() : nullptr;
    if (weaponObject == nullptr || weaponObject->Ritem == nullptr)
    {
        mTrackedWarriorWeaponObject = nullptr;
        mTrackedWarriorWeaponTipLocalValid = false;
        mTrackedWarriorWeaponTipWorldValid = false;
        mWarriorSwordTrailEmitAnchorValid = false;
        mWarriorSwordTrailSkippedFirstEmit = false;
        mWarriorSwordTrailSpawnedSegment = false;
        return;
    }

    if (weaponObject != mTrackedWarriorWeaponObject)
    {
        mTrackedWarriorWeaponObject = weaponObject;
        mTrackedWarriorWeaponTipLocalValid = false;
        mTrackedWarriorWeaponTipWorldValid = false;
        mWarriorSwordTrailEmitAnchorValid = false;
        mWarriorSwordTrailSkippedFirstEmit = false;
        mWarriorSwordTrailSpawnedSegment = false;
    }

    if (!mTrackedWarriorWeaponTipLocalValid)
    {
        mTrackedWarriorWeaponTipLocalValid =
            TryResolveBladeTrailLocalPoints(
                weaponObject->Ritem,
                mTrackedWarriorWeaponTipLocal,
                mTrackedWarriorWeaponInnerLocal);
    }

    XMFLOAT3 currentTipWorld = { weaponObject->World._41, weaponObject->World._42, weaponObject->World._43 };
    XMFLOAT3 currentInnerWorld = currentTipWorld;
    if (mTrackedWarriorWeaponTipLocalValid)
    {
        currentTipWorld = TransformPoint(mTrackedWarriorWeaponTipLocal, weaponObject->World);
        currentInnerWorld = TransformPoint(mTrackedWarriorWeaponInnerLocal, weaponObject->World);
    }

    if (!mTrackedWarriorWeaponTipWorldValid)
    {
        mPreviousWarriorWeaponTipWorld = currentTipWorld;
        mPreviousWarriorWeaponInnerWorld = currentInnerWorld;
        mTrackedWarriorWeaponTipWorld = currentTipWorld;
        mTrackedWarriorWeaponInnerWorld = currentInnerWorld;
        mTrackedWarriorWeaponTipWorldValid = true;
        return;
    }

    mPreviousWarriorWeaponTipWorld = mTrackedWarriorWeaponTipWorld;
    mPreviousWarriorWeaponInnerWorld = mTrackedWarriorWeaponInnerWorld;
    mTrackedWarriorWeaponTipWorld = currentTipWorld;
    mTrackedWarriorWeaponInnerWorld = currentInnerWorld;
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
    effect->StartDelay = 0.0f;
    effect->MotionDuration = 0.0f;
    effect->FadeStartTime = 0.0f;
    effect->SpinRate = 0.0f;
    effect->UseLinearMotion = false;
    effect->UseStyleAnimation = true;

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
