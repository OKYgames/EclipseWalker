#pragma once

#include "Player.h"
#include <DirectXMath.h>
#include <functional>
#include <vector>

class EclipseWalkerGame;
class GameObject;
struct Material;
struct RenderItem;

class SkillEffectManager
{
public:
    using TrackOwnedCallback = std::function<void(GameObject*, RenderItem*)>;

    void Initialize(EclipseWalkerGame* game, const TrackOwnedCallback& trackOwned);
    void Reset();
    void Update(float dt);

    void OnSkillCast(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& origin, float rotY);
    void OnSkillResolved(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& impactCenter, float rotY, float effectRadius);
    void OnSkillImpact(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& hitPosition);
    void PreviewWarriorSwordStrike(
        const DirectX::XMFLOAT3& targetPosition,
        float rotY,
        float effectRadius,
        float impactDelay,
        float swordSpawnDelay);

private:
    enum class EffectStyle
    {
        BillboardBurst,
        GroundDecal,
        VerticalBeam,
        SummonedSword
    };

    struct EffectInstance
    {
        GameObject* Object = nullptr;
        RenderItem* Ritem = nullptr;
        EffectStyle Style = EffectStyle::BillboardBurst;
        bool Active = false;
        float Age = 0.0f;
        float LifeTime = 0.0f;
        DirectX::XMFLOAT3 BasePosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Velocity = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 StartScale = { 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 EndScale = { 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 StartColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 EndColor = { 1.0f, 1.0f, 1.0f, 0.0f };
        float RotX = 0.0f;
        float RotY = 0.0f;
        float RotZ = 0.0f;
        DirectX::XMFLOAT3 TargetPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 AnchorLocalPoint = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4X4 RotationMatrix = DirectX::XMFLOAT4X4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
        float StartDelay = 0.0f;
        float MotionDuration = 0.0f;
        float FadeStartTime = 0.0f;
    };

private:
    void EnsureResources();
    void EnsurePool();
    void EnsureSummonedSwordPool();
    EffectInstance* AcquireEffect(EffectStyle style);
    void DeactivateEffect(EffectInstance& effect);
    void SpawnBurst(
        const DirectX::XMFLOAT3& position,
        float startScale,
        float endScale,
        float lifeTime,
        float riseSpeed,
        const DirectX::XMFLOAT4& startColor,
        const DirectX::XMFLOAT4& endColor);
    void SpawnGroundDecal(
        const DirectX::XMFLOAT3& position,
        float rotY,
        float startScale,
        float endScale,
        float lifeTime,
        const DirectX::XMFLOAT4& startColor,
        const DirectX::XMFLOAT4& endColor,
        Material* materialOverride = nullptr);
    void SpawnVerticalBeam(
        const DirectX::XMFLOAT3& position,
        float rotY,
        float width,
        float height,
        float lifeTime,
        const DirectX::XMFLOAT4& startColor,
        const DirectX::XMFLOAT4& endColor);
    void SpawnSummonedSword(
        const DirectX::XMFLOAT3& targetPosition,
        float rotY,
        float uniformScale,
        float spawnHeight,
        float lifeTime,
        float motionDuration = 0.0f,
        float startDelay = 0.0f);

private:
    EclipseWalkerGame* mGame = nullptr;
    TrackOwnedCallback mTrackOwned;
    Material* mBurstMaterial = nullptr;
    Material* mDecalMaterial = nullptr;
    Material* mEarthshatterDecalMaterial = nullptr;
    Material* mBeamMaterial = nullptr;
    Material* mSummonedSwordMaterial = nullptr;
    DirectX::XMFLOAT3 mSummonedSwordTipAxisLocal = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 mSummonedSwordAnchorLocal = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mSummonedSwordScaleMultiplier = { 1.0f, 1.0f, 1.0f };
    std::vector<EffectInstance> mEffects;
};
