#pragma once

#include "d3dUtil.h"
#include <DirectXMath.h>
#include <functional>
#include <random>
#include <string>
#include <vector>

class EclipseWalkerGame;
class GameObject;
class ResourceManager;
struct Material;
struct RenderItem;

class RedPortalEffect
{
public:
    using TrackOwnedCallback = std::function<void(GameObject*, RenderItem*)>;

    struct TextureConfig
    {
        std::string CenterTextureName = "Effect_Portal_Center";
        std::wstring CenterTexturePath = L"Textures/Effect/portal_spark_point_01.dds";
        std::string RingTextureName = "Effect_Portal_Ring";
        std::wstring RingTexturePath = L"Textures/Effect/portal_ring_circle_01.dds";
        std::string SmokeTextureName = "Effect_Portal_Smoke";
        std::wstring SmokeTexturePath = L"Textures/Effect/portal_smoke_01.dds";
        std::string SparkTextureName = "Effect_Portal_Spark";
        std::wstring SparkTexturePath = L"Textures/Effect/portal_spark_point_01.dds";
    };

    struct Settings
    {
        TextureConfig Textures;
        DirectX::XMFLOAT3 Position = { 0.0f, 1.35f, 0.0f };

        float PortalWidth = 1.50f;
        float PortalHeight = 2.18f;
        float PortalYaw = 0.0f;

        DirectX::XMFLOAT4 CenterColor = { 0.72f, 0.72f, 0.74f, 0.60f };
        DirectX::XMFLOAT4 RingInnerColor = { 1.86f, 0.18f, 0.14f, 0.82f };
        DirectX::XMFLOAT4 SmokeColor = { 1.16f, 0.14f, 0.14f, 0.44f };
        DirectX::XMFLOAT4 SparkColor = { 1.95f, 1.95f, 1.95f, 0.92f };

        float CenterPulseSpeed = 0.95f;
        float CenterPulseAmount = 0.035f;

        float RingInnerPulseSpeed = 1.10f;
        float RingInnerPulseAmount = 0.015f;

        bool CenterUseAdditive = false;
        bool RingUseAdditive = true;
        bool SmokeUseAdditive = false;
        bool SparkUseAdditive = true;

        int SmokeMaxParticles = 72;
        float SmokeSpawnRate = 46.0f;
        float SmokeLifetimeMin = 1.15f;
        float SmokeLifetimeMax = 2.05f;
        float SmokeStartScaleMin = 0.40f;
        float SmokeStartScaleMax = 0.62f;
        float SmokeEndScaleMin = 0.72f;
        float SmokeEndScaleMax = 1.05f;
        float SmokeDriftMin = 0.015f;
        float SmokeDriftMax = 0.05f;
        float SmokeTangentialDrift = 0.07f;
        float SmokeEdgeJitter = 0.06f;
        float SmokeInwardBias = 0.02f;
        float SmokeBandInnerScale = 0.82f;
        float SmokeBandOuterScale = 1.08f;
        float SmokeBandVerticalJitter = 0.18f;
        float SmokeBandTangentialJitter = 0.22f;
        float SmokeAlphaStartMin = 0.20f;
        float SmokeAlphaStartMax = 0.34f;

        int SparkMaxParticles = 18;
        float SparkSpawnRate = 10.0f;
        float SparkLifetimeMin = 0.20f;
        float SparkLifetimeMax = 0.48f;
        float SparkLengthMin = 0.08f;
        float SparkLengthMax = 0.20f;
        float SparkWidthMin = 0.015f;
        float SparkWidthMax = 0.032f;
        float SparkBurstSpeedMin = 0.18f;
        float SparkBurstSpeedMax = 0.42f;
        float SparkOrbitSpeedMin = 1.10f;
        float SparkOrbitSpeedMax = 2.30f;
        float SparkOrbitRatio = 0.88f;
    };

public:
    static void LoadRequiredTextures(ResourceManager* resources, const TextureConfig& textures = TextureConfig{});

    bool Init(EclipseWalkerGame* game, const TrackOwnedCallback& trackOwned, const Settings& settings = Settings{});
    void Update(float deltaTime);
    void Render(ID3D12GraphicsCommandList* commandList);
    void Reset();
    void SetPosition(const DirectX::XMFLOAT3& position);
    void SetActive(bool active);

    const Settings& GetSettings() const { return mSettings; }
    Settings& GetMutableSettings() { return mSettings; }
    bool IsActive() const { return mActive; }

private:
    enum class ParticleMode
    {
        Drift,
        Orbit
    };

    struct SpriteNode
    {
        GameObject* Object = nullptr;
        RenderItem* Ritem = nullptr;
        Material* Material = nullptr;
    };

    struct ParticleInstance
    {
        GameObject* Object = nullptr;
        RenderItem* Ritem = nullptr;
        Material* Material = nullptr;
        bool Active = false;
        ParticleMode Mode = ParticleMode::Drift;
        float Age = 0.0f;
        float LifeTime = 0.0f;
        float Angle = 0.0f;
        float AngularVelocity = 0.0f;
        float Roll = 0.0f;
        float RollSpeed = 0.0f;
        float WidthStart = 0.0f;
        float WidthEnd = 0.0f;
        float HeightStart = 0.0f;
        float HeightEnd = 0.0f;
        float DepthOffset = 0.0f;
        float DepthVelocity = 0.0f;
        float UvRotation = 0.0f;
        float UvRotationRate = 0.0f;
        DirectX::XMFLOAT2 LocalOffset = { 0.0f, 0.0f };
        DirectX::XMFLOAT2 LocalVelocity = { 0.0f, 0.0f };
        DirectX::XMFLOAT2 OrbitScale = { 1.0f, 1.0f };
        DirectX::XMFLOAT4 StartColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 EndColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    };

private:
    void EnsureResources();
    void EnsurePool();
    Material* EnsureLayerMaterial(
        const std::string& materialName,
        const std::string& textureName,
        const DirectX::XMFLOAT4& diffuseAlbedo,
        bool additive);

    void UpdateCoreLayers(float dt, float cameraFacingYaw);
    void UpdateSmoke(float dt, float cameraFacingYaw, const DirectX::XMFLOAT3& right, const DirectX::XMFLOAT3& forward);
    void UpdateSparks(float dt, float cameraFacingYaw, const DirectX::XMFLOAT3& right, const DirectX::XMFLOAT3& forward);
    void SpawnSmokeParticle();
    void SpawnSparkParticle();
    ParticleInstance* AcquireInactiveParticle(std::vector<ParticleInstance>& particles);
    void DeactivateParticle(ParticleInstance& particle);
    void HideSprite(const SpriteNode& sprite);
    void ApplyBillboard(
        GameObject* object,
        const DirectX::XMFLOAT3& position,
        float cameraFacingYaw,
        float width,
        float height,
        float roll);
    void ApplySprite(
        const SpriteNode& sprite,
        const DirectX::XMFLOAT3& position,
        float cameraFacingYaw,
        float width,
        float height,
        float roll,
        const DirectX::XMFLOAT4& color,
        DirectX::CXMMATRIX texTransform);
    DirectX::XMFLOAT2 MakeEllipsePoint(float angle, float radiusScaleX, float radiusScaleY) const;
    DirectX::XMFLOAT2 MakeEllipseNormal(float angle) const;
    DirectX::XMFLOAT4 LerpColor(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t) const;
    DirectX::XMFLOAT3 MakeWorldPosition(
        const DirectX::XMFLOAT2& localOffset,
        float depthOffset,
        const DirectX::XMFLOAT3& right,
        const DirectX::XMFLOAT3& forward) const;
    DirectX::XMMATRIX BuildUvTransform(float rotation, float scrollX, float scrollY, float scaleU = 1.0f, float scaleV = 1.0f) const;
    float RandomRange(float minValue, float maxValue);
    float ComputeFade(float normalizedAge, float fadeInEnd, float fadeOutStart) const;

private:
    EclipseWalkerGame* mGame = nullptr;
    TrackOwnedCallback mTrackOwned;
    Settings mSettings;

    SpriteNode mCenterSprite;
    SpriteNode mRingInnerSprite;
    std::vector<ParticleInstance> mSmokeParticles;
    std::vector<ParticleInstance> mSparkParticles;

    Material* mCenterMaterial = nullptr;
    Material* mRingInnerMaterial = nullptr;
    Material* mSmokeMaterial = nullptr;
    Material* mSparkMaterial = nullptr;

    bool mInitialized = false;
    bool mActive = true;
    float mTime = 0.0f;
    float mSmokeSpawnAccumulator = 0.0f;
    float mSparkSpawnAccumulator = 0.0f;

    std::mt19937 mRng;
};
