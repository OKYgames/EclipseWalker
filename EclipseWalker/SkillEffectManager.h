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

    void OnSkillCast(
        PlayerClass playerClass,
        int skillIndex,
        const DirectX::XMFLOAT3& origin,
        float rotY,
        float activeDuration,
        float startDelay = 0.0f);
    void OnRemoteSkillCast(
        PlayerClass playerClass,
        int skillIndex,
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& impactCenter,
        float rotY,
        float effectRadius,
        float startDelay = 0.0f);
    void OnSkillResolved(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& impactCenter, float rotY, float effectRadius);
    void OnSkillImpact(PlayerClass playerClass, int skillIndex, const DirectX::XMFLOAT3& hitPosition);
    void OnArcherHasteBasicShot(const DirectX::XMFLOAT3& origin, float rotY, float intensity);
    void TriggerLevelUpEffect(const DirectX::XMFLOAT3& origin, float rotY, PlayerClass playerClass, int newLevel);
    void SpawnArcherBasicArrow(
        const DirectX::XMFLOAT3& origin,
        float rotY,
        float travelDistance,
        float startDelay = 0.0f,
        float startHeight = -1.0f,
        float startRightOffset = 0.1f);
    void SpawnMageBasicOrb(const DirectX::XMFLOAT3& origin, float rotY, float travelDistance, float startDelay = 0.0f);
    void PreviewWarriorSwordStrike(
        const DirectX::XMFLOAT3& targetPosition,
        float rotY,
        float effectRadius,
        float impactDelay,
        float swordSpawnDelay);
    void PreviewMageMeteor(
        const DirectX::XMFLOAT3& targetPosition,
        float effectRadius,
        float impactDelay);
    void PreviewArcherArrowRain(
        const DirectX::XMFLOAT3& targetPosition,
        float effectRadius,
        float fallStartDelay,
        float fallDuration);

private:
    enum class EffectStyle
    {
        GroundDecal,
        VerticalBeam,
        MageBasicOrb,
        MageBasicOrbCore,
        ArcherWindRibbon,
        ArrowRainArrow,
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
        bool UseLinearMotion = false;
    };

private:
    void EnsureResources();
    void EnsurePool();
    void EnsureSummonedSwordPool();
    void EnsureArcherArrowRainPool();
    void EnsureMageBasicOrbCorePool();
    void EnsureArcherBuffLoopVisuals();
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
    void SpawnArcherWindRibbon(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& velocity,
        float startWidth,
        float startHeight,
        float endWidth,
        float endHeight,
        float lifeTime,
        const DirectX::XMFLOAT4& startColor,
        const DirectX::XMFLOAT4& endColor,
        float yawOffset = 0.0f,
        float rollOffset = 0.0f,
        Material* materialOverride = nullptr);
    void SpawnSummonedSword(
        const DirectX::XMFLOAT3& targetPosition,
        float rotY,
        float uniformScale,
        float spawnHeight,
        float lifeTime,
        float motionDuration = 0.0f,
        float startDelay = 0.0f);
    void SpawnArcherArrowRainArrow(
        const DirectX::XMFLOAT3& targetPosition,
        float yaw,
        float startDelay,
        float fallDuration,
        float uniformScale,
        float spawnHeight);
    Material* EnsureWeaponGlowMaterial();
    void TriggerWeaponSkillGlow(const DirectX::XMFLOAT4& glowColor, float duration);
    void UpdateWeaponSkillGlow(float dt);
    void ClearWeaponSkillGlow();
    void SpawnArcherSlashBurst(const DirectX::XMFLOAT3& position, float rotY, float intensity, float scaleMultiplier = 1.0f);
    void SpawnArcherDustBurst(const DirectX::XMFLOAT3& position, float rotY, float intensity, float scaleMultiplier = 1.0f);
    void SpawnArcherBuffStartEffect(const DirectX::XMFLOAT3& origin, float rotY);
    void SpawnArcherBuffLoopEffect(const DirectX::XMFLOAT3& origin, float rotY, float intensity);
    void SpawnArcherBuffFrontEffect(const DirectX::XMFLOAT3& origin, float rotY, float intensity);
    void SpawnArcherBuffEndEffect(const DirectX::XMFLOAT3& origin, float rotY);
    void SpawnMageHealingLightEffect(const DirectX::XMFLOAT3& origin, float startDelay);
    void SpawnMageMeteorFlameSprite(
        const DirectX::XMFLOAT3& startPosition,
        const DirectX::XMFLOAT3& endPosition,
        float visibleDuration,
        float startDelay,
        float startScaleX,
        float startScaleY,
        float endScaleX,
        float endScaleY,
        const DirectX::XMFLOAT4& startColor,
        const DirectX::XMFLOAT4& endColor,
        Material* material,
        bool billboard,
        float rotY = 0.0f);
    void SetArcherBuffLoopVisible(bool visible);
    void UpdateArcherBuffLoopVisuals(const DirectX::XMFLOAT3& origin, float rotY, float intensity);
    void UpdateLocalArcherHasteAura(float dt);

private:
    EclipseWalkerGame* mGame = nullptr;
    TrackOwnedCallback mTrackOwned;
    Material* mDecalMaterial = nullptr;
    Material* mEarthshatterDecalMaterial = nullptr;
    Material* mBeamMaterial = nullptr;
    Material* mMageBasicOrbCoreMaterial = nullptr;
    Material* mMageBasicOrbAuraMaterial = nullptr;
    Material* mMageBasicOrbTrailMaterial = nullptr;
    Material* mMageBasicOrbOuterTrailMaterial = nullptr;
    Material* mMageBasicOrbFlashMaterial = nullptr;
    Material* mMageBasicOrbImpactMaterial = nullptr;
    Material* mMageHealSparkleMaterial = nullptr;
    Material* mMageMeteorCircleMaterial = nullptr;
    Material* mMageMeteorFlameMaterials[4] = { nullptr, nullptr, nullptr, nullptr };
    Material* mArcherCircleMaterial = nullptr;
    Material* mArcherArrowRainDecalMaterial = nullptr;
    Material* mArcherColumnMaterial = nullptr;
    Material* mArcherWindMaterial = nullptr;
    Material* mArcherSlashMaterial = nullptr;
    Material* mArcherDustMaterial = nullptr;
    Material* mArcherArrowMaterial = nullptr;
    Material* mSummonedSwordMaterial = nullptr;
    GameObject* mArcherBuffLoopOuterObject = nullptr;
    GameObject* mArcherBuffLoopInnerObject = nullptr;
    RenderItem* mArcherBuffLoopOuterRitem = nullptr;
    RenderItem* mArcherBuffLoopInnerRitem = nullptr;
    std::vector<GameObject*> mArcherBuffLoopFlowObjects;
    std::vector<RenderItem*> mArcherBuffLoopFlowRitems;
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
    bool mLocalArcherBuffLoopActive = false;
    float mArcherHasteAuraPulseTimer = 0.0f;
    DirectX::XMFLOAT3 mLastLocalArcherBuffPosition = { 0.0f, 0.0f, 0.0f };
    float mLastLocalArcherBuffRotY = 0.0f;
    std::vector<EffectInstance> mEffects;
};
