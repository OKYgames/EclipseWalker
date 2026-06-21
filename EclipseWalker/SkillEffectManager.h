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
    void OnSkillResolved(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& impactCenter, float rotY);
    void OnSkillImpact(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& hitPosition);

private:
    enum class EffectStyle
    {
        BillboardBurst,
        GroundDecal
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
    };

private:
    void EnsureResources();
    void EnsurePool();
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

private:
    EclipseWalkerGame* mGame = nullptr;
    TrackOwnedCallback mTrackOwned;
    Material* mBurstMaterial = nullptr;
    Material* mDecalMaterial = nullptr;
    Material* mEarthshatterDecalMaterial = nullptr;
    std::vector<EffectInstance> mEffects;
};
