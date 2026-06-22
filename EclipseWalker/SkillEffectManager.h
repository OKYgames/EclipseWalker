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

    void OnBasicAttackCast(PlayerClass playerClass, int basicAttackVariant, const DirectX::XMFLOAT3& targetPosition, float travelTime);
    void OnSkillCast(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& origin, float rotY, float activeDuration);
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
        GroundDecal,
        VerticalBeam,
        MageOrbCore,
        MageOrbShell,
        MageOrbGlow,
        MageOrbRune,
        MageOrbSpark,
        MageOrbTrail,
        MageOrbExplosion,
        SummonedSword
    };

    struct EffectInstance
    {
        GameObject* Object = nullptr;
        RenderItem* Ritem = nullptr;
        EffectStyle Style = EffectStyle::GroundDecal;
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
        float OrbitRadiusStart = 0.0f;
        float OrbitRadiusEnd = 0.0f;
        float OrbitSpeed = 0.0f;
        float OrbitPhase = 0.0f;
        float OrbitVerticalScale = 0.0f;
        float UVRotationSpeed = 0.0f;
        float UVRotationPhase = 0.0f;
        float UVScaleX = 1.0f;
        float UVScaleY = 1.0f;
    };

private:
    void EnsureResources();
    void EnsurePool();
    void EnsureSummonedSwordPool();
    EffectInstance* AcquireEffect(EffectStyle style);
    void DeactivateEffect(EffectInstance& effect);
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
    void SpawnMageBasicOrb(const DirectX::XMFLOAT3& targetPosition, float travelTime);
    EffectInstance* SpawnMageOrbLayer(
        EffectStyle style,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& velocity,
        float startScale,
        float endScale,
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
    Material* EnsureWeaponGlowMaterial();
    void TriggerWeaponSkillGlow(const DirectX::XMFLOAT4& glowColor, float duration);
    void UpdateWeaponSkillGlow(float dt);
    void ClearWeaponSkillGlow();

private:
    EclipseWalkerGame* mGame = nullptr;
    TrackOwnedCallback mTrackOwned;
    Material* mDecalMaterial = nullptr;
    Material* mEarthshatterDecalMaterial = nullptr;
    Material* mBeamMaterial = nullptr;
    Material* mMageOrbCoreMaterial = nullptr;
    Material* mMageOrbShellMaterial = nullptr;
    Material* mMageOrbGlowMaterial = nullptr;
    Material* mMageOrbRuneMaterial = nullptr;
    Material* mSummonedSwordMaterial = nullptr;
    Material* mWeaponGlowMaterial = nullptr;
    Material* mWeaponGlowBaseMaterial = nullptr;
    RenderItem* mWeaponGlowOwnerRitem = nullptr;
    DirectX::XMFLOAT3 mSummonedSwordTipAxisLocal = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 mSummonedSwordAnchorLocal = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mSummonedSwordScaleMultiplier = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 mWeaponBaseDiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 mWeaponBaseFresnelR0 = { 0.01f, 0.01f, 0.01f };
    float mWeaponBaseRoughness = 0.25f;
    float mWeaponBaseOutlineThickness = 0.0f;
    DirectX::XMFLOAT4 mWeaponBaseOutlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT4 mWeaponGlowColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float mWeaponGlowTimer = 0.0f;
    float mWeaponGlowDuration = 0.0f;
    std::vector<EffectInstance> mEffects;
};
