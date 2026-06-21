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
        const XMFLOAT4 currentColor = Lerp4(effect.StartColor, effect.EndColor, t);
        const XMFLOAT3 currentPosition =
        {
            effect.BasePosition.x + effect.Velocity.x * effect.Age,
            effect.BasePosition.y + effect.Velocity.y * effect.Age,
            effect.BasePosition.z + effect.Velocity.z * effect.Age
        };

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
        else if (skillIndex == 2)
        {
            const XMFLOAT3 front = AddScaled(origin, forward, 1.35f);
            SpawnGroundDecal({ front.x, origin.y + 0.05f, front.z }, rotY, 0.42f, 1.56f, 0.38f, skillColor, fadeColor);
            SpawnBurst({ front.x, origin.y + 1.05f, front.z }, 0.34f, 1.12f, 0.28f, 0.95f, skillColor, fadeColor);
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
        renderItem->Mat = (style == EffectStyle::BillboardBurst) ? mBurstMaterial : mDecalMaterial;
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
        if (!object->mIsBillboard)
        {
            object->SetRotation(XM_PIDIV2, 0.0f, 0.0f);
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

    if (effect.Object != nullptr)
    {
        effect.Object->mIsAnimated = false;
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
