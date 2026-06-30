#include "Archer.h"

#include "AudioManager.h"
#include "EclipseWalkerGame.h"
#include "Material.h"
#include "MeshGeometry.h"
#include "ModelLoader.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>

using namespace DirectX;

namespace
{
    constexpr int kArrowPoolSize = 100;
    constexpr float kWindImbuementDuration = 6.0f;
    constexpr float kWindImbuementAttackSpeedMultiplier = 1.45f;
    constexpr float kWindImbuementEffectIntensity = 1.18f;
    constexpr float kArrowTargetMaxDimension = 1.0f;
    constexpr float kArrowStartForwardOffset = 0.8f;
    constexpr float kArrowStartHeight = 0.7f;
    constexpr float kArrowStartRightOffset = 0.1f;
    constexpr float kArrowFireDelay = 1.1f;
    constexpr float kArrowSpeed = 20.0f;
    constexpr float kArrowMinDistance = 3.0f;
    constexpr float kArrowMaxDistance = 30.0f;
    constexpr XMFLOAT3 kArrowRotationOffset = { 0.0f, DirectX::XM_PI, 0.0f };
    const char* kArrowGeometryName = "archerBasicArrowGeo";
    const char* kArrowMaterialName = "ArcherArrowMat";
    const char* kArrowModelPath = "Models/Weapons/Arrow.fbx";
    constexpr wchar_t kArcherBowReleaseSound[] = L"Sounds\\Archer\\Archer_BowRelease.mp3";
    constexpr wchar_t kArcherDashSound[] = L"Sounds\\Archer\\Archer_Dash.mp3";
    constexpr wchar_t kArcherFootstep1Sound[] = L"Sounds\\Archer\\Archer_Footstep_01.mp3";
    constexpr wchar_t kArcherFootstep2Sound[] = L"Sounds\\Archer\\Archer_Footstep_02.mp3";
    constexpr wchar_t kArcherWindImbuementLoopSound[] = L"Sounds\\Archer\\Archer_WindImbuement_Loop.mp3";
    constexpr wchar_t kArcherArrowRainSound[] = L"Sounds\\Archer\\Archer_ArrowRain.mp3";
    constexpr float kArcherFootstepIntervalSeconds = 0.30f;
    constexpr float kArcherDashVolume = 0.10f;
    constexpr float kArcherBowReleaseVolume = 0.10f;
    constexpr float kArcherFootstepVolume = 0.08f;
    constexpr float kArcherWindImbuementVolume = 0.08f;
    constexpr float kArcherArrowRainVolume = 0.11f;

    XMFLOAT3 ForwardFromYaw(float rotY)
    {
        return { std::sin(rotY), 0.0f, std::cos(rotY) };
    }

    XMFLOAT3 RightFromYaw(float rotY)
    {
        return { std::cos(rotY), 0.0f, -std::sin(rotY) };
    }

    std::unique_ptr<MeshGeometry> BuildArrowGeometry(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList)
    {
        if (device == nullptr || cmdList == nullptr)
        {
            return nullptr;
        }

        MapMeshData modelData;
        if (!ModelLoader::Load(kArrowModelPath, modelData) || modelData.Vertices.empty() || modelData.Indices.empty())
        {
            OutputDebugStringA("[Archer] Failed to load arrow model\n");
            return nullptr;
        }

        BoundingBox rawBounds;
        BoundingBox::CreateFromPoints(
            rawBounds,
            modelData.Vertices.size(),
            &modelData.Vertices[0].Pos,
            sizeof(Vertex));

        const float rawMaxDimension = (std::max)(
            rawBounds.Extents.x * 2.0f,
            (std::max)(rawBounds.Extents.y * 2.0f, rawBounds.Extents.z * 2.0f));
        const float normalizeScale = rawMaxDimension > 0.0001f
            ? kArrowTargetMaxDimension / rawMaxDimension
            : 1.0f;

        for (auto& vertex : modelData.Vertices)
        {
            vertex.Pos.x = (vertex.Pos.x - rawBounds.Center.x) * normalizeScale;
            vertex.Pos.y = (vertex.Pos.y - rawBounds.Center.y) * normalizeScale;
            vertex.Pos.z = (vertex.Pos.z - rawBounds.Center.z) * normalizeScale;
        }

        auto geometry = std::make_unique<MeshGeometry>();
        geometry->Name = kArrowGeometryName;

        const UINT vbByteSize = static_cast<UINT>(modelData.Vertices.size() * sizeof(Vertex));
        const UINT ibByteSize = static_cast<UINT>(modelData.Indices.size() * sizeof(std::uint32_t));

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
        CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), modelData.Vertices.data(), vbByteSize);
        ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
        CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), modelData.Indices.data(), ibByteSize);

        geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
            device,
            cmdList,
            modelData.Vertices.data(),
            vbByteSize,
            geometry->VertexBufferUploader);
        geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
            device,
            cmdList,
            modelData.Indices.data(),
            ibByteSize,
            geometry->IndexBufferUploader);

        geometry->VertexByteStride = sizeof(Vertex);
        geometry->VertexBufferByteSize = vbByteSize;
        geometry->IndexFormat = DXGI_FORMAT_R32_UINT;
        geometry->IndexBufferByteSize = ibByteSize;

        SubmeshGeometry submesh;
        submesh.IndexCount = static_cast<UINT>(modelData.Indices.size());
        submesh.StartIndexLocation = 0;
        submesh.BaseVertexLocation = 0;
        BoundingBox::CreateFromPoints(
            submesh.Bounds,
            modelData.Vertices.size(),
            &modelData.Vertices[0].Pos,
            sizeof(Vertex));
        geometry->DrawArgs["mesh"] = submesh;

        return geometry;
    }
}

Archer::Archer()
{
    maxHp = 250.0f;
    hp = 250.0f;
    maxMp = 100.0f;
    mp = 100.0f;

    mMoveSpeed = 4.0f;
    mDashDuration = 0.25f;
    mDashSpeedMultiplier = 3.0f;
    mDashCooldownDuration = 6.0f;

    UpdateMeshForTier();
}

Archer::~Archer()
{
    StopArrowRainSound();
    StopWindImbuementLoopSound();
}

bool Archer::Skill1()
{
    mWindImbuementTimer = kWindImbuementDuration;
    return true;
}
bool Archer::Skill2() { return true; }
float Archer::GetBasicAttackSpeedMultiplier() const
{
    return mWindImbuementTimer > 0.0f ? kWindImbuementAttackSpeedMultiplier : 1.0f;
}

float Archer::GetSkillEffectIntensityMultiplier() const
{
    return mWindImbuementTimer > 0.0f ? kWindImbuementEffectIntensity : 1.0f;
}

bool Archer::HasAttackSpeedBuff() const
{
    return mWindImbuementTimer > 0.0f;
}

float Archer::GetAttackSpeedBuffRemaining() const
{
    return mWindImbuementTimer > 0.0f ? mWindImbuementTimer : 0.0f;
}

void Archer::OnDashStarted()
{
    AudioManager::Get().PlayEffect(kArcherDashSound, kArcherDashVolume);
}

void Archer::OnBasicAttackStarted(int attackVariant)
{
    (void)attackVariant;
    AudioManager::Get().PlayEffect(kArcherBowReleaseSound, kArcherBowReleaseVolume);
}

void Archer::OnSkillAttackStarted(int skillIndex)
{
    if (skillIndex != 2)
    {
        return;
    }

    StopArrowRainSound();
    const float animationDuration = GetAttackAnimationRemaining();
    mArrowRainSoundTimer = ArcherAnimationTiming::DelayFromProgress(
        animationDuration,
        ArcherAnimationTiming::kSkillESoundProgress);
    mArrowRainSoundPending = mArrowRainSoundTimer > 0.0f;
    mArrowRainSoundStopTimer = ArcherAnimationTiming::DelayFromProgress(
        animationDuration,
        ArcherAnimationTiming::kSkillEHitProgress) +
        (std::max)(ArcherAnimationTiming::kSkillESoundStopDelaySeconds, 0.0f);
    mArrowRainSoundStopPending = true;
    if (!mArrowRainSoundPending)
    {
        mArrowRainSoundHandle = AudioManager::Get().PlayEffect(
            kArcherArrowRainSound,
            kArcherArrowRainVolume);
    }
}

void Archer::SetArrowTrailType(ArrowProjectile& projectile, ArrowTrailType trailType)
{
    projectile.TrailType = trailType;

    // TODO: Route TrailType into a dedicated projectile trail renderer when arrow trail assets are added.
    if (projectile.Ritem != nullptr)
    {
        projectile.Ritem->ColorMultiplier = GetArrowTrailColorMultiplier(trailType);
        projectile.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

XMFLOAT4 Archer::GetArrowTrailColorMultiplier(ArrowTrailType trailType) const
{
    switch (trailType)
    {
    case ArrowTrailType::BuffedArrowTrail:
        return { 0.72f, 1.18f, 0.86f, 1.0f };

    case ArrowTrailType::NormalArrowTrail:
    default:
        return { 1.0f, 1.0f, 1.0f, 1.0f };
    }
}

void Archer::UpdateMeshForTier() {}

bool Archer::EnsureArrowResources(EclipseWalkerGame* game)
{
    if (game == nullptr)
    {
        return false;
    }

    auto* resources = game->GetResources();
    if (resources == nullptr)
    {
        return false;
    }

    if (!std::filesystem::exists(kArrowModelPath))
    {
        OutputDebugStringA("[Archer] Missing Models/Weapons/Arrow.fbx\n");
        return false;
    }

    if (resources->mGeometries.find(kArrowGeometryName) == resources->mGeometries.end())
    {
        auto geometry = BuildArrowGeometry(game->GetDevice(), game->GetCommandList());
        if (geometry == nullptr)
        {
            return false;
        }

        resources->mGeometries[geometry->Name] = std::move(geometry);
    }

    if (resources->GetMaterial(kArrowMaterialName) == nullptr)
    {
        resources->CreateMaterial(
            kArrowMaterialName,
            static_cast<int>(resources->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(1.35f, 1.08f, 0.62f, 1.0f),
            XMFLOAT3(0.05f, 0.05f, 0.05f),
            0.55f);
    }

    if (!mArrowProjectiles.empty())
    {
        return true;
    }

    auto geoIt = resources->mGeometries.find(kArrowGeometryName);
    Material* material = resources->GetMaterial(kArrowMaterialName);
    if (geoIt == resources->mGeometries.end() || material == nullptr)
    {
        return false;
    }

    auto* geometry = geoIt->second.get();
    auto submeshIt = geometry->DrawArgs.find("mesh");
    if (submeshIt == geometry->DrawArgs.end())
    {
        return false;
    }

    auto& ritems = game->GetRitems();
    auto& objects = game->GetGameObjects();

    mArrowProjectiles.reserve(kArrowPoolSize);
    for (int i = 0; i < kArrowPoolSize; ++i)
    {
        auto item = std::make_unique<RenderItem>();
        item->World = MathHelper::Identity4x4();
        item->TexTransform = MathHelper::Identity4x4();
        item->ObjCBIndex = static_cast<UINT>(ritems.size());
        item->Geo = geometry;
        item->Mat = material;
        item->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        item->IndexCount = submeshIt->second.IndexCount;
        item->StartIndexLocation = submeshIt->second.StartIndexLocation;
        item->BaseVertexLocation = submeshIt->second.BaseVertexLocation;
        item->Visible = false;
        item->CastShadow = false;

        auto object = std::make_unique<GameObject>();
        object->Ritem = item.get();
        object->SetScale(1.0f, 1.0f, 1.0f);
        object->Update();

        ArrowProjectile projectile;
        projectile.Object = object.get();
        projectile.Ritem = item.get();
        mArrowProjectiles.push_back(projectile);

        ritems.push_back(std::move(item));
        objects.push_back(std::move(object));
    }

    return true;
}

void Archer::FireBasicArrow(EclipseWalkerGame* game, const XMFLOAT3& origin, float rotY, float travelDistance)
{
    if (!EnsureArrowResources(game))
    {
        return;
    }

    auto projectileIt = std::find_if(
        mArrowProjectiles.begin(),
        mArrowProjectiles.end(),
        [](const ArrowProjectile& projectile)
        {
            return !projectile.Active;
        });

    if (projectileIt == mArrowProjectiles.end())
    {
        projectileIt = mArrowProjectiles.begin();
    }

    const XMFLOAT3 forward = ForwardFromYaw(rotY);
    const XMFLOAT3 right = RightFromYaw(rotY);
    const float clampedDistance = std::clamp(travelDistance, kArrowMinDistance, kArrowMaxDistance);
    const float basicAttackSpeedMultiplier = (std::max)(GetBasicAttackSpeedMultiplier(), 1.0f);
    const bool buffedShot = HasAttackSpeedBuff();

    ArrowProjectile& projectile = *projectileIt;
    projectile.StartPosition =
    {
        origin.x + forward.x * kArrowStartForwardOffset + right.x * kArrowStartRightOffset,
        origin.y + kArrowStartHeight,
        origin.z + forward.z * kArrowStartForwardOffset + right.z * kArrowStartRightOffset
    };
    projectile.PreviousPosition = projectile.StartPosition;
    projectile.Direction = forward;
    projectile.RotY = rotY;
    projectile.Age = 0.0f;
    projectile.Delay = kArrowFireDelay / basicAttackSpeedMultiplier;
    projectile.TravelDistance = clampedDistance;
    projectile.LifeTime = (std::max)(clampedDistance / kArrowSpeed, 0.12f);
    projectile.Buffed = buffedShot;
    projectile.Active = true;
    SetArrowTrailType(
        projectile,
        buffedShot ? ArrowTrailType::BuffedArrowTrail : ArrowTrailType::NormalArrowTrail);

    OutputDebugStringA("[Archer] Basic arrow fired\n");

    if (projectile.Ritem != nullptr)
    {
        projectile.Ritem->Visible = false;
        projectile.Ritem->ColorMultiplier = GetArrowTrailColorMultiplier(projectile.TrailType);
        projectile.Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void Archer::UpdateArrows(float dt, const ArrowCollisionCallback& collisionCallback)
{
    for (auto& projectile : mArrowProjectiles)
    {
        if (!projectile.Active || projectile.Object == nullptr || projectile.Ritem == nullptr)
        {
            continue;
        }

        projectile.Age += dt;
        if (projectile.Age < projectile.Delay)
        {
            projectile.Ritem->Visible = false;
            projectile.Ritem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        const float moveAge = projectile.Age - projectile.Delay;
        const float t = projectile.LifeTime > 0.0f
            ? std::clamp(moveAge / projectile.LifeTime, 0.0f, 1.0f)
            : 1.0f;

        if (t >= 1.0f)
        {
            projectile.Active = false;
            projectile.Ritem->Visible = false;
            projectile.Ritem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        const XMFLOAT3 currentPosition =
        {
            projectile.StartPosition.x + projectile.Direction.x * projectile.TravelDistance * t,
            projectile.StartPosition.y,
            projectile.StartPosition.z + projectile.Direction.z * projectile.TravelDistance * t
        };
        const XMFLOAT3 previousPosition = projectile.PreviousPosition;

        projectile.Object->SetPosition(currentPosition.x, currentPosition.y, currentPosition.z);
        projectile.Object->SetRotation(
            kArrowRotationOffset.x,
            projectile.RotY + kArrowRotationOffset.y,
            kArrowRotationOffset.z);
        projectile.Object->Update();

        projectile.Ritem->Visible = true;
        projectile.Ritem->ColorMultiplier = GetArrowTrailColorMultiplier(projectile.TrailType);
        projectile.Ritem->NumFramesDirty = gNumFrameResources;

        if (collisionCallback && collisionCallback(previousPosition, currentPosition, projectile.RotY))
        {
            projectile.Active = false;
            projectile.Ritem->Visible = false;
            projectile.Ritem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        projectile.PreviousPosition = currentPosition;
    }
}

void Archer::HideArrows()
{
    for (auto& projectile : mArrowProjectiles)
    {
        projectile.Active = false;
        if (projectile.Ritem != nullptr)
        {
            projectile.Ritem->Visible = false;
            projectile.Ritem->NumFramesDirty = gNumFrameResources;
        }
    }
}

void Archer::UpdateClassState(float dt)
{
    if (IsDead())
    {
        mWindImbuementTimer = 0.0f;
        mFootstepTimer = 0.0f;
        mWasWalkingOnGround = false;
        mArrowRainSoundTimer = 0.0f;
        mArrowRainSoundPending = false;
        mArrowRainSoundStopTimer = 0.0f;
        mArrowRainSoundStopPending = false;
        StopArrowRainSound();
        StopWindImbuementLoopSound();
        return;
    }

    if (mArrowRainSoundPending)
    {
        mArrowRainSoundTimer -= dt;
        if (mArrowRainSoundTimer <= 0.0f)
        {
            mArrowRainSoundHandle = AudioManager::Get().PlayEffect(
                kArcherArrowRainSound,
                kArcherArrowRainVolume);
            mArrowRainSoundTimer = 0.0f;
            mArrowRainSoundPending = false;
        }
    }

    if (mArrowRainSoundStopPending)
    {
        mArrowRainSoundStopTimer -= dt;
        if (mArrowRainSoundStopTimer <= 0.0f)
        {
            StopArrowRainSound();
        }
    }

    if (mWindImbuementTimer > 0.0f)
    {
        mWindImbuementTimer -= dt;
        if (mWindImbuementTimer < 0.0f)
        {
            mWindImbuementTimer = 0.0f;
        }
    }

    if (mWindImbuementTimer > 0.0f)
    {
        if (mWindImbuementLoopHandle == AudioManager::InvalidClipHandle)
        {
            mWindImbuementLoopHandle = AudioManager::Get().PlayLoop(
                kArcherWindImbuementLoopSound,
                kArcherWindImbuementVolume);
        }
    }
    else
    {
        StopWindImbuementLoopSound();
    }

    const bool isWalkingOnGround =
        mIsGrounded &&
        !mIsDashing &&
        !mIsDead &&
        !mIsSkillLeaping &&
        mAttackAnimationTimer <= 0.0f &&
        (std::fabs(mMoveDir.x) > 0.01f || std::fabs(mMoveDir.z) > 0.01f);

    if (!isWalkingOnGround)
    {
        mFootstepTimer = 0.0f;
        mWasWalkingOnGround = false;
        return;
    }

    if (!mWasWalkingOnGround)
    {
        AudioManager::Get().PlayEffect(
            mNextFootstepVariant == 1 ? kArcherFootstep1Sound : kArcherFootstep2Sound,
            kArcherFootstepVolume);
        mNextFootstepVariant = (mNextFootstepVariant == 1) ? 2 : 1;
        mFootstepTimer = kArcherFootstepIntervalSeconds;
        mWasWalkingOnGround = true;
        return;
    }

    mFootstepTimer -= dt;
    if (mFootstepTimer <= 0.0f)
    {
        AudioManager::Get().PlayEffect(
            mNextFootstepVariant == 1 ? kArcherFootstep1Sound : kArcherFootstep2Sound,
            kArcherFootstepVolume);
        mNextFootstepVariant = (mNextFootstepVariant == 1) ? 2 : 1;
        mFootstepTimer = kArcherFootstepIntervalSeconds;
    }
}

float Archer::GetSkillAttackLockDuration(int skillIndex) const
{
    if (skillIndex == 1)
    {
        return 0.62f;
    }

    return Player::GetSkillAttackLockDuration(skillIndex);
}

void Archer::StopArrowRainSound()
{
    if (mArrowRainSoundHandle != AudioManager::InvalidClipHandle)
    {
        AudioManager::Get().StopEffect(mArrowRainSoundHandle);
        mArrowRainSoundHandle = AudioManager::InvalidClipHandle;
    }

    mArrowRainSoundTimer = 0.0f;
    mArrowRainSoundPending = false;
    mArrowRainSoundStopTimer = 0.0f;
    mArrowRainSoundStopPending = false;
}

void Archer::StopWindImbuementLoopSound()
{
    if (mWindImbuementLoopHandle == AudioManager::InvalidClipHandle)
    {
        return;
    }

    AudioManager::Get().StopEffect(mWindImbuementLoopHandle);
    mWindImbuementLoopHandle = AudioManager::InvalidClipHandle;
}
