#include "RedPortalEffect.h"

#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Material.h"
#include "RenderItem.h"
#include "ResourceManager.h"

#include <cmath>
#include <filesystem>

using namespace DirectX;

namespace
{
    constexpr float kPi = 3.1415926535f;
    constexpr float kTwoPi = 6.2831853071f;

    float Clamp01(float value)
    {
        return MathHelper::Clamp(value, 0.0f, 1.0f);
    }

    float SafeSqrt(float value)
    {
        return value > 0.0f ? std::sqrt(value) : 0.0f;
    }
}

void RedPortalEffect::LoadRequiredTextures(ResourceManager* resources, const TextureConfig& textures)
{
    if (resources == nullptr)
    {
        return;
    }

    auto tryLoadTexture =
        [resources](const std::string& textureName, const std::wstring& texturePath)
        {
            if (textureName.empty() || texturePath.empty() || resources->GetTexture(textureName) != nullptr)
            {
                return;
            }

            if (std::filesystem::exists(texturePath))
            {
                resources->LoadTexture(textureName, texturePath);
            }
        };

    tryLoadTexture(textures.CenterTextureName, textures.CenterTexturePath);
    tryLoadTexture(textures.RingTextureName, textures.RingTexturePath);
    tryLoadTexture(textures.SmokeTextureName, textures.SmokeTexturePath);
    tryLoadTexture(textures.SparkTextureName, textures.SparkTexturePath);
}

bool RedPortalEffect::Init(EclipseWalkerGame* game, const TrackOwnedCallback& trackOwned, const Settings& settings)
{
    mGame = game;
    mTrackOwned = trackOwned;
    mSettings = settings;
    mRng.seed(std::random_device{}());

    if (mGame == nullptr || mGame->GetResources() == nullptr)
    {
        return false;
    }

    EnsureResources();
    EnsurePool();
    Reset();
    mInitialized = true;
    return true;
}

void RedPortalEffect::Update(float deltaTime)
{
    if (!mInitialized)
    {
        return;
    }

    if (!mActive)
    {
        HideSprite(mCenterSprite);
        HideSprite(mRingInnerSprite);
        for (auto& particle : mSmokeParticles) DeactivateParticle(particle);
        for (auto& particle : mSparkParticles) DeactivateParticle(particle);
        return;
    }

    const XMFLOAT3 cameraPosition = mGame->GetCamera() != nullptr
        ? mGame->GetCamera()->GetPosition3f()
        : XMFLOAT3(0.0f, 0.0f, 1.0f);

    float dx = cameraPosition.x - mSettings.Position.x;
    float dz = cameraPosition.z - mSettings.Position.z;
    if (std::fabs(dx) < 0.0001f && std::fabs(dz) < 0.0001f)
    {
        dz = 1.0f;
    }

    const float cameraFacingYaw = std::atan2(dx, dz);
    const XMFLOAT3 right = { std::cos(cameraFacingYaw), 0.0f, -std::sin(cameraFacingYaw) };
    const XMFLOAT3 forward = { std::sin(cameraFacingYaw), 0.0f, std::cos(cameraFacingYaw) };

    mTime += (std::max)(deltaTime, 0.0f);
    UpdateCoreLayers(deltaTime, cameraFacingYaw);
    UpdateSmoke(deltaTime, cameraFacingYaw, right, forward);
    UpdateSparks(deltaTime, cameraFacingYaw, right, forward);
}

void RedPortalEffect::Render(ID3D12GraphicsCommandList* commandList)
{
    (void)commandList;
}

void RedPortalEffect::Reset()
{
    mTime = 0.0f;
    mSmokeSpawnAccumulator = 0.0f;
    mSparkSpawnAccumulator = 0.0f;

    HideSprite(mCenterSprite);
    HideSprite(mRingInnerSprite);
    for (auto& particle : mSmokeParticles) DeactivateParticle(particle);
    for (auto& particle : mSparkParticles) DeactivateParticle(particle);
}

void RedPortalEffect::SetPosition(const XMFLOAT3& position)
{
    mSettings.Position = position;
}

void RedPortalEffect::SetActive(bool active)
{
    mActive = active;
    if (!mActive)
    {
        Reset();
    }
}

void RedPortalEffect::EnsureResources()
{
    auto* resources = mGame != nullptr ? mGame->GetResources() : nullptr;
    if (resources == nullptr)
    {
        return;
    }

    const std::string centerTextureName =
        resources->GetTexture(mSettings.Textures.CenterTextureName) != nullptr ? mSettings.Textures.CenterTextureName : "white";
    const std::string ringTextureName =
        resources->GetTexture(mSettings.Textures.RingTextureName) != nullptr ? mSettings.Textures.RingTextureName : "white";
    const std::string smokeTextureName =
        resources->GetTexture(mSettings.Textures.SmokeTextureName) != nullptr ? mSettings.Textures.SmokeTextureName : centerTextureName;
    const std::string sparkTextureName =
        resources->GetTexture(mSettings.Textures.SparkTextureName) != nullptr ? mSettings.Textures.SparkTextureName : "white";

    mCenterMaterial = EnsureLayerMaterial("PortalFx_CenterMat", centerTextureName, mSettings.CenterColor, mSettings.CenterUseAdditive);
    mRingInnerMaterial = EnsureLayerMaterial("PortalFx_RingInnerMat", ringTextureName, mSettings.RingInnerColor, mSettings.RingUseAdditive);
    mSmokeMaterial = EnsureLayerMaterial("PortalFx_SmokeMat", smokeTextureName, mSettings.SmokeColor, mSettings.SmokeUseAdditive);
    mSparkMaterial = EnsureLayerMaterial("PortalFx_SparkMat", sparkTextureName, mSettings.SparkColor, mSettings.SparkUseAdditive);
}

void RedPortalEffect::EnsurePool()
{
    auto* resources = mGame != nullptr ? mGame->GetResources() : nullptr;
    if (resources == nullptr)
    {
        return;
    }

    auto geoIt = resources->mGeometries.find("quadGeo");
    if (geoIt == resources->mGeometries.end() || geoIt->second == nullptr)
    {
        OutputDebugStringA("[RedPortalEffect] quadGeo missing, skipping portal pool creation\n");
        return;
    }

    const auto& drawArgs = geoIt->second->DrawArgs["quad"];
    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    auto createSprite =
        [&](Material* material, SpriteNode& outSprite)
        {
            if (outSprite.Object != nullptr && outSprite.Ritem != nullptr)
            {
                outSprite.Material = material;
                outSprite.Ritem->Mat = material;
                outSprite.Ritem->NumFramesDirty = gNumFrameResources;
                return;
            }

            auto renderItem = std::make_unique<RenderItem>();
            renderItem->World = MathHelper::Identity4x4();
            renderItem->TexTransform = MathHelper::Identity4x4();
            renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
            renderItem->Geo = geoIt->second.get();
            renderItem->Mat = material;
            renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            renderItem->IndexCount = drawArgs.IndexCount;
            renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
            renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
            renderItem->Visible = false;
            renderItem->CastShadow = false;
            renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };

            auto object = std::make_unique<GameObject>();
            object->Ritem = renderItem.get();
            object->SetScale(0.0f, 0.0f, 1.0f);
            object->SetPosition(0.0f, -1000.0f, 0.0f);
            object->Update();

            outSprite.Object = object.get();
            outSprite.Ritem = renderItem.get();
            outSprite.Material = material;

            if (mTrackOwned)
            {
                mTrackOwned(object.get(), renderItem.get());
            }

            ritems.push_back(std::move(renderItem));
            objects.push_back(std::move(object));
        };

    createSprite(mCenterMaterial, mCenterSprite);
    createSprite(mRingInnerMaterial, mRingInnerSprite);

    auto ensureParticlePool =
        [&](std::vector<ParticleInstance>& particles, int targetCount, Material* material)
        {
            while (static_cast<int>(particles.size()) < targetCount)
            {
                SpriteNode sprite;
                createSprite(material, sprite);

                ParticleInstance particle;
                particle.Object = sprite.Object;
                particle.Ritem = sprite.Ritem;
                particle.Material = material;
                particles.push_back(particle);
            }

            for (auto& particle : particles)
            {
                particle.Material = material;
                if (particle.Ritem != nullptr)
                {
                    particle.Ritem->Mat = material;
                    particle.Ritem->NumFramesDirty = gNumFrameResources;
                }
            }
        };

    ensureParticlePool(mSmokeParticles, (std::max)(mSettings.SmokeMaxParticles, 1), mSmokeMaterial);
    ensureParticlePool(mSparkParticles, (std::max)(mSettings.SparkMaxParticles, 1), mSparkMaterial);
}

Material* RedPortalEffect::EnsureLayerMaterial(
    const std::string& materialName,
    const std::string& textureName,
    const XMFLOAT4& diffuseAlbedo,
    bool additive)
{
    auto* resources = mGame != nullptr ? mGame->GetResources() : nullptr;
    if (resources == nullptr)
    {
        return nullptr;
    }

    if (resources->GetMaterial(materialName) == nullptr)
    {
        resources->CreateMaterial(
            materialName,
            static_cast<int>(resources->mMaterials.size()),
            textureName,
            "",
            "",
            "",
            diffuseAlbedo,
            XMFLOAT3(0.04f, 0.04f, 0.04f),
            0.02f);
    }

    Material* material = resources->GetMaterial(materialName);
    if (material != nullptr)
    {
        material->DiffuseMapName = textureName;
        material->DiffuseAlbedo = diffuseAlbedo;
        material->FresnelR0 = { 0.04f, 0.04f, 0.04f };
        material->Roughness = 0.02f;
        material->MetallicFactor = 0.0f;
        // Portal layers should stay in the regular transparent/effect pass.
        // The fog-volume pass softens them too much and makes the portal look washed out.
        material->IsTransparent = 1;
        material->IsToon = 0;
        material->OutlineThickness = 0.0f;
        material->OutlineColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        material->NumFramesDirty = gNumFrameResources;
    }

    return material;
}

void RedPortalEffect::UpdateCoreLayers(float dt, float cameraFacingYaw)
{
    (void)dt;

    const float baseHalfWidth = mSettings.PortalWidth * 0.5f;
    const float baseHalfHeight = mSettings.PortalHeight * 0.5f;
    HideSprite(mCenterSprite);

    XMFLOAT4 innerColor = mSettings.RingInnerColor;
    ApplySprite(
        mRingInnerSprite,
        mSettings.Position,
        cameraFacingYaw,
        baseHalfWidth * 1.00f,
        baseHalfHeight * 1.00f,
        0.0f,
        innerColor,
        XMMatrixIdentity());
}

void RedPortalEffect::UpdateSmoke(float dt, float cameraFacingYaw, const XMFLOAT3& right, const XMFLOAT3& forward)
{
    if (mSmokeMaterial == nullptr)
    {
        return;
    }

    mSmokeSpawnAccumulator += dt * (std::max)(mSettings.SmokeSpawnRate, 0.0f);
    while (mSmokeSpawnAccumulator >= 1.0f)
    {
        SpawnSmokeParticle();
        mSmokeSpawnAccumulator -= 1.0f;
    }

    for (auto& particle : mSmokeParticles)
    {
        if (!particle.Active || particle.Object == nullptr || particle.Ritem == nullptr)
        {
            continue;
        }

        particle.Age += dt;
        if (particle.Age >= particle.LifeTime)
        {
            DeactivateParticle(particle);
            continue;
        }

        particle.LocalOffset.x += particle.LocalVelocity.x * dt;
        particle.LocalOffset.y += particle.LocalVelocity.y * dt;
        particle.DepthOffset += particle.DepthVelocity * dt;
        particle.Roll += particle.RollSpeed * dt;
        particle.UvRotation += particle.UvRotationRate * dt;

        const float t = Clamp01(particle.Age / particle.LifeTime);
        const float fade = ComputeFade(t, 0.08f, 0.92f);
        const float width = MathHelper::Lerp(particle.WidthStart, particle.WidthEnd, t);
        const float height = MathHelper::Lerp(particle.HeightStart, particle.HeightEnd, t);
        XMFLOAT4 color = LerpColor(particle.StartColor, particle.EndColor, t);
        color.w *= fade;

        ApplySprite(
            { particle.Object, particle.Ritem, particle.Material },
            MakeWorldPosition(particle.LocalOffset, particle.DepthOffset, right, forward),
            cameraFacingYaw,
            width,
            height,
            particle.Roll,
            color,
            BuildUvTransform(particle.UvRotation, 0.0f, -t * 0.10f, 1.0f, 1.0f));
    }
}

void RedPortalEffect::UpdateSparks(float dt, float cameraFacingYaw, const XMFLOAT3& right, const XMFLOAT3& forward)
{
    if (mSparkMaterial == nullptr)
    {
        return;
    }

    mSparkSpawnAccumulator += dt * (std::max)(mSettings.SparkSpawnRate, 0.0f);
    while (mSparkSpawnAccumulator >= 1.0f)
    {
        SpawnSparkParticle();
        mSparkSpawnAccumulator -= 1.0f;
    }

    for (auto& particle : mSparkParticles)
    {
        if (!particle.Active || particle.Object == nullptr || particle.Ritem == nullptr)
        {
            continue;
        }

        particle.Age += dt;
        if (particle.Age >= particle.LifeTime)
        {
            DeactivateParticle(particle);
            continue;
        }

        if (particle.Mode == ParticleMode::Orbit)
        {
            particle.Angle += particle.AngularVelocity * dt;
            particle.LocalOffset = MakeEllipsePoint(particle.Angle, particle.OrbitScale.x, particle.OrbitScale.y);
            const XMFLOAT2 tangent =
            {
                -std::sin(particle.Angle) * mSettings.PortalWidth * 0.5f * particle.OrbitScale.x,
                std::cos(particle.Angle) * mSettings.PortalHeight * 0.5f * particle.OrbitScale.y
            };
            particle.Roll = std::atan2(tangent.y, tangent.x) + 0.45f * std::sin(mTime * 6.0f + particle.Angle);
        }
        else
        {
            particle.LocalOffset.x += particle.LocalVelocity.x * dt;
            particle.LocalOffset.y += particle.LocalVelocity.y * dt;
            particle.DepthOffset += particle.DepthVelocity * dt;
            particle.Roll += particle.RollSpeed * dt;
        }

        particle.UvRotation += particle.UvRotationRate * dt;

        const float t = Clamp01(particle.Age / particle.LifeTime);
        const float fade = ComputeFade(t, 0.06f, 0.42f);
        const float width = MathHelper::Lerp(particle.WidthStart, particle.WidthEnd, t);
        const float height = MathHelper::Lerp(particle.HeightStart, particle.HeightEnd, t);
        XMFLOAT4 color = LerpColor(particle.StartColor, particle.EndColor, t);
        color.w *= fade;

        ApplySprite(
            { particle.Object, particle.Ritem, particle.Material },
            MakeWorldPosition(particle.LocalOffset, particle.DepthOffset, right, forward),
            cameraFacingYaw,
            width,
            height,
            particle.Roll,
            color,
            BuildUvTransform(particle.UvRotation, 0.0f, 0.0f, 1.0f, 1.0f));
    }
}

void RedPortalEffect::SpawnSmokeParticle()
{
    ParticleInstance* particle = AcquireInactiveParticle(mSmokeParticles);
    if (particle == nullptr)
    {
        return;
    }

    const float angle = RandomRange(0.0f, kTwoPi);
    const float radiusScaleX = RandomRange(mSettings.SmokeBandInnerScale, mSettings.SmokeBandOuterScale);
    const float radiusScaleY = RandomRange(mSettings.SmokeBandInnerScale, mSettings.SmokeBandOuterScale);
    const XMFLOAT2 edgePoint = MakeEllipsePoint(angle, radiusScaleX, radiusScaleY);
    const XMFLOAT2 normal = MakeEllipseNormal(angle);
    const XMFLOAT2 tangent = { -normal.y, normal.x };
    const float drift = RandomRange(mSettings.SmokeDriftMin, mSettings.SmokeDriftMax);
    const float tangentialDrift = RandomRange(-mSettings.SmokeTangentialDrift, mSettings.SmokeTangentialDrift);
    const float edgeOffset = RandomRange(-mSettings.SmokeEdgeJitter, mSettings.SmokeEdgeJitter) - mSettings.SmokeInwardBias;
    const float verticalJitter = RandomRange(-mSettings.SmokeBandVerticalJitter, mSettings.SmokeBandVerticalJitter);
    const float tangentialSpawnOffset = RandomRange(-mSettings.SmokeBandTangentialJitter, mSettings.SmokeBandTangentialJitter);
    const float startAlpha = RandomRange(mSettings.SmokeAlphaStartMin, mSettings.SmokeAlphaStartMax);

    particle->Active = true;
    particle->Mode = ParticleMode::Drift;
    particle->Age = 0.0f;
    particle->LifeTime = RandomRange(mSettings.SmokeLifetimeMin, mSettings.SmokeLifetimeMax);
    particle->LocalOffset =
    {
        edgePoint.x + normal.x * edgeOffset + tangent.x * tangentialSpawnOffset,
        edgePoint.y + normal.y * edgeOffset + tangent.y * tangentialSpawnOffset + verticalJitter
    };
    particle->LocalVelocity =
    {
        normal.x * drift * 0.35f + tangent.x * tangentialDrift,
        normal.y * drift * 0.18f + tangent.y * tangentialDrift * 0.32f
    };
    particle->DepthOffset = RandomRange(-0.015f, 0.015f);
    particle->DepthVelocity = RandomRange(-0.008f, 0.008f);
    particle->Roll = RandomRange(-0.45f, 0.45f);
    particle->RollSpeed = RandomRange(-0.12f, 0.12f);
    particle->WidthStart = RandomRange(mSettings.SmokeStartScaleMin, mSettings.SmokeStartScaleMax);
    particle->WidthEnd = RandomRange(mSettings.SmokeEndScaleMin, mSettings.SmokeEndScaleMax);
    particle->HeightStart = particle->WidthStart * RandomRange(0.90f, 1.20f);
    particle->HeightEnd = particle->WidthEnd * RandomRange(0.95f, 1.28f);
    particle->StartColor = { mSettings.SmokeColor.x, mSettings.SmokeColor.y, mSettings.SmokeColor.z, startAlpha };
    particle->EndColor = { mSettings.SmokeColor.x * 0.72f, mSettings.SmokeColor.y * 0.52f, mSettings.SmokeColor.z * 0.52f, startAlpha * 0.28f };
    particle->UvRotation = RandomRange(-kPi, kPi);
    particle->UvRotationRate = RandomRange(-0.12f, 0.12f);
    particle->Ritem->Mat = mSmokeMaterial;
}

void RedPortalEffect::SpawnSparkParticle()
{
    ParticleInstance* particle = AcquireInactiveParticle(mSparkParticles);
    if (particle == nullptr)
    {
        return;
    }

    const float angle = RandomRange(0.0f, kTwoPi);
    const bool useOrbit = RandomRange(0.0f, 1.0f) < mSettings.SparkOrbitRatio;

    particle->Active = true;
    particle->Age = 0.0f;
    particle->LifeTime = RandomRange(mSettings.SparkLifetimeMin, mSettings.SparkLifetimeMax);
    particle->WidthStart = RandomRange(mSettings.SparkWidthMin, mSettings.SparkWidthMax);
    particle->WidthEnd = particle->WidthStart * 0.35f;
    particle->HeightStart = RandomRange(mSettings.SparkLengthMin, mSettings.SparkLengthMax);
    particle->HeightEnd = particle->HeightStart * 0.55f;
    particle->StartColor = mSettings.SparkColor;
    particle->EndColor = { mSettings.SparkColor.x, mSettings.SparkColor.y, mSettings.SparkColor.z, 0.0f };
    particle->DepthOffset = RandomRange(-0.010f, 0.024f);
    particle->DepthVelocity = RandomRange(-0.05f, 0.05f);
    particle->UvRotation = RandomRange(-0.35f, 0.35f);
    particle->UvRotationRate = RandomRange(-2.2f, 2.2f);
    particle->Ritem->Mat = mSparkMaterial;

    if (useOrbit)
    {
        particle->Mode = ParticleMode::Orbit;
        particle->Angle = angle;
        particle->AngularVelocity = RandomRange(mSettings.SparkOrbitSpeedMin, mSettings.SparkOrbitSpeedMax) * (RandomRange(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
        particle->OrbitScale = { RandomRange(1.00f, 1.18f), RandomRange(0.88f, 1.12f) };
        particle->LocalOffset = MakeEllipsePoint(angle, particle->OrbitScale.x, particle->OrbitScale.y);
        particle->LocalVelocity = { 0.0f, 0.0f };
        particle->Roll = RandomRange(-0.24f, 0.24f);
        particle->RollSpeed = 0.0f;
    }
    else
    {
        particle->Mode = ParticleMode::Drift;
        particle->Angle = angle;
        particle->AngularVelocity = 0.0f;
        particle->OrbitScale = { 1.0f, 1.0f };
        particle->LocalOffset = MakeEllipsePoint(angle, RandomRange(0.96f, 1.08f), RandomRange(0.96f, 1.08f));

        const XMFLOAT2 normal = MakeEllipseNormal(angle);
        const XMFLOAT2 tangent = { -normal.y, normal.x };
        const float burstSpeed = RandomRange(mSettings.SparkBurstSpeedMin, mSettings.SparkBurstSpeedMax);
        const float tangentScale = RandomRange(-0.22f, 0.22f);
        particle->LocalVelocity =
        {
            normal.x * burstSpeed + tangent.x * tangentScale,
            normal.y * burstSpeed + tangent.y * tangentScale
        };
        particle->Roll = std::atan2(particle->LocalVelocity.y, particle->LocalVelocity.x);
        particle->RollSpeed = RandomRange(-1.2f, 1.2f);
    }
}

RedPortalEffect::ParticleInstance* RedPortalEffect::AcquireInactiveParticle(std::vector<ParticleInstance>& particles)
{
    for (auto& particle : particles)
    {
        if (!particle.Active)
        {
            return &particle;
        }
    }

    return nullptr;
}

void RedPortalEffect::DeactivateParticle(ParticleInstance& particle)
{
    particle.Active = false;
    particle.Age = 0.0f;
    particle.LifeTime = 0.0f;

    if (particle.Object != nullptr)
    {
        particle.Object->SetScale(0.0f, 0.0f, 1.0f);
        particle.Object->SetPosition(0.0f, -1000.0f, 0.0f);
        particle.Object->Update();
    }

    if (particle.Ritem != nullptr)
    {
        particle.Ritem->Visible = false;
        particle.Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
        particle.Ritem->TexTransform = MathHelper::Identity4x4();
        particle.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void RedPortalEffect::HideSprite(const SpriteNode& sprite)
{
    if (sprite.Object != nullptr)
    {
        sprite.Object->SetScale(0.0f, 0.0f, 1.0f);
        sprite.Object->SetPosition(0.0f, -1000.0f, 0.0f);
        sprite.Object->Update();
    }

    if (sprite.Ritem != nullptr)
    {
        sprite.Ritem->Visible = false;
        sprite.Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 0.0f };
        sprite.Ritem->TexTransform = MathHelper::Identity4x4();
        sprite.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void RedPortalEffect::ApplyBillboard(
    GameObject* object,
    const XMFLOAT3& position,
    float cameraFacingYaw,
    float width,
    float height,
    float roll)
{
    if (object == nullptr)
    {
        return;
    }

    object->SetPosition(position.x, position.y, position.z);
    object->SetRotation(0.0f, cameraFacingYaw, roll);
    object->SetScale(width, height, 1.0f);
    object->Update();
}

void RedPortalEffect::ApplySprite(
    const SpriteNode& sprite,
    const XMFLOAT3& position,
    float cameraFacingYaw,
    float width,
    float height,
    float roll,
    const XMFLOAT4& color,
    CXMMATRIX texTransform)
{
    if (sprite.Object == nullptr || sprite.Ritem == nullptr)
    {
        return;
    }

    ApplyBillboard(sprite.Object, position, cameraFacingYaw, width, height, roll);
    sprite.Ritem->Mat = sprite.Material;
    sprite.Ritem->ColorMultiplier = color;
    XMStoreFloat4x4(&sprite.Ritem->TexTransform, texTransform);
    sprite.Ritem->Visible = color.w > 0.001f;
    sprite.Ritem->NumFramesDirty = gNumFrameResources;
}

XMFLOAT2 RedPortalEffect::MakeEllipsePoint(float angle, float radiusScaleX, float radiusScaleY) const
{
    const float halfWidth = mSettings.PortalWidth * 0.5f * radiusScaleX;
    const float halfHeight = mSettings.PortalHeight * 0.5f * radiusScaleY;
    return { std::cos(angle) * halfWidth, std::sin(angle) * halfHeight };
}

XMFLOAT2 RedPortalEffect::MakeEllipseNormal(float angle) const
{
    const float halfWidth = (std::max)(mSettings.PortalWidth * 0.5f, 0.001f);
    const float halfHeight = (std::max)(mSettings.PortalHeight * 0.5f, 0.001f);

    float nx = std::cos(angle) / halfWidth;
    float ny = std::sin(angle) / halfHeight;
    const float length = SafeSqrt(nx * nx + ny * ny);
    if (length > 0.0001f)
    {
        nx /= length;
        ny /= length;
    }
    else
    {
        nx = 1.0f;
        ny = 0.0f;
    }

    return { nx, ny };
}

XMFLOAT4 RedPortalEffect::LerpColor(const XMFLOAT4& a, const XMFLOAT4& b, float t) const
{
    return
    {
        MathHelper::Lerp(a.x, b.x, t),
        MathHelper::Lerp(a.y, b.y, t),
        MathHelper::Lerp(a.z, b.z, t),
        MathHelper::Lerp(a.w, b.w, t)
    };
}

XMFLOAT3 RedPortalEffect::MakeWorldPosition(
    const XMFLOAT2& localOffset,
    float depthOffset,
    const XMFLOAT3& right,
    const XMFLOAT3& forward) const
{
    return
    {
        mSettings.Position.x + right.x * localOffset.x + forward.x * depthOffset,
        mSettings.Position.y + localOffset.y,
        mSettings.Position.z + right.z * localOffset.x + forward.z * depthOffset
    };
}

XMMATRIX RedPortalEffect::BuildUvTransform(float rotation, float scrollX, float scrollY, float scaleU, float scaleV) const
{
    return XMMatrixTranslation(-0.5f, -0.5f, 0.0f) *
        XMMatrixRotationZ(rotation) *
        XMMatrixScaling(scaleU, scaleV, 1.0f) *
        XMMatrixTranslation(0.5f + scrollX, 0.5f + scrollY, 0.0f);
}

float RedPortalEffect::RandomRange(float minValue, float maxValue)
{
    if (maxValue <= minValue)
    {
        return minValue;
    }

    std::uniform_real_distribution<float> distribution(minValue, maxValue);
    return distribution(mRng);
}

float RedPortalEffect::ComputeFade(float normalizedAge, float fadeInEnd, float fadeOutStart) const
{
    const float t = Clamp01(normalizedAge);
    const float fadeIn = fadeInEnd > 0.0001f ? Clamp01(t / fadeInEnd) : 1.0f;
    const float fadeOut = fadeOutStart < 0.9999f ? Clamp01((1.0f - t) / (1.0f - fadeOutStart)) : 1.0f;
    return fadeIn * fadeOut;
}
