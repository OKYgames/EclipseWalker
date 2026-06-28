#include "Stage2BossController.h"

#include "CharacterVisualFactory.h"
#include "AudioManager.h"
#include "DamageTextRenderer.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "MapSystem.h"
#include "Material.h"
#include "Monster.h"
#include "NetworkManager.h"
#include "Player.h"
#include "Protocol.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include "SkeletalAnimationComponent.h"
#include "UIManager.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <Windows.h>

namespace
{
    constexpr bool kEnableBossAnimationDebug = true;

    constexpr const char* GetBossModelPath()
    {
        return "Models/Boss/SK_DemonLord_Idle.ufbx";
    }

    struct BossAnimationClipSpec
    {
        const char* FilePath;
        const char* ClipName;
    };

    constexpr std::array<BossAnimationClipSpec, 32> kBossAnimationClips = { {
        { "Models/Animated/Boss/DemonLord@Idle.ufbx", "SkeletonIdle" },
        { "Models/Animated/Boss/DemonLord@2HitCombo.ufbx", "Boss2HitCombo" },
        { "Models/Animated/Boss/DemonLord@2HitComboForward.ufbx", "Boss2HitComboForward" },
        { "Models/Animated/Boss/DemonLord@2HitComboForward_RM.ufbx", "Boss2HitComboForwardRM" },
        { "Models/Animated/Boss/DemonLord@3HitCombo.ufbx", "Boss3HitCombo" },
        { "Models/Animated/Boss/DemonLord@3HitComboForward.ufbx", "Boss3HitComboForward" },
        { "Models/Animated/Boss/DemonLord@3HitComboForward_RM.ufbx", "Boss3HitComboForwardRM" },
        { "Models/Animated/Boss/DemonLord@Death.ufbx", "SkeletonDeath" },
        { "Models/Animated/Boss/DemonLord@Fly.ufbx", "BossFly" },
        { "Models/Animated/Boss/DemonLord@FlyBackwards.ufbx", "BossFlyBackwards" },
        { "Models/Animated/Boss/DemonLord@FlyGetHit.ufbx", "BossFlyGetHit" },
        { "Models/Animated/Boss/DemonLord@FlyLeft.ufbx", "BossFlyLeft" },
        { "Models/Animated/Boss/DemonLord@FlyRight.ufbx", "BossFlyRight" },
        { "Models/Animated/Boss/DemonLord@GetHit1.ufbx", "SkeletonDamage" },
        { "Models/Animated/Boss/DemonLord@GetHit2.ufbx", "BossGetHit2" },
        { "Models/Animated/Boss/DemonLord@Roar.ufbx", "BossRoar" },
        { "Models/Animated/Boss/DemonLord@StrafeLeft.ufbx", "BossStrafeLeft" },
        { "Models/Animated/Boss/DemonLord@StrafeLeft_RM.ufbx", "BossStrafeLeftRM" },
        { "Models/Animated/Boss/DemonLord@StrafeRight.ufbx", "BossStrafeRight" },
        { "Models/Animated/Boss/DemonLord@StrafeRight_RM.ufbx", "BossStrafeRightRM" },
        { "Models/Animated/Boss/DemonLord@SummonSword&Whip.ufbx", "BossSummonSwordWhip" },
        { "Models/Animated/Boss/DemonLord@SwordAttack1.ufbx", "BossSwordAttack1" },
        { "Models/Animated/Boss/DemonLord@SwordAttack1Forward.ufbx", "BossSwordAttack1Forward" },
        { "Models/Animated/Boss/DemonLord@SwordAttack1Forward_RM.ufbx", "BossSwordAttack1ForwardRM" },
        { "Models/Animated/Boss/DemonLord@SwordAttack2.ufbx", "BossSwordAttack2" },
        { "Models/Animated/Boss/DemonLord@SwordAttack2Forward.ufbx", "BossSwordAttack2Forward" },
        { "Models/Animated/Boss/DemonLord@SwordAttack2Forward_RM.ufbx", "BossSwordAttack2ForwardRM" },
        { "Models/Animated/Boss/DemonLord@Walk.ufbx", "SkeletonWalk" },
        { "Models/Animated/Boss/DemonLord@Walk_RM.ufbx", "BossWalkRM" },
        { "Models/Animated/Boss/DemonLord@WhipAttack.ufbx", "BossWhipAttack" },
        { "Models/Animated/Boss/DemonLord@WhipAttackForward.ufbx", "BossWhipAttackForward" },
        { "Models/Animated/Boss/DemonLord@WhipAttackForward_RM.ufbx", "BossWhipAttackForwardRM" }
    } };

    void AddBossAnimationClips(CharacterVisualSpec& visualSpec)
    {
        for (const BossAnimationClipSpec& clip : kBossAnimationClips)
        {
            visualSpec.AdditionalAnimationClips.push_back({ clip.FilePath, clip.ClipName });
        }
    }

    const DirectX::XMFLOAT3 kStage2BossAnchorPosition = { -8.81673f, 6.01219f, 23.2462f };
    const DirectX::XMFLOAT3 kStage2BossSpawnPosition = { -8.81673f, 7.71219f, 23.2462f };
    const DirectX::XMFLOAT3 kStage2PlayerStartPosition = { -4.81673f, 6.01219f, 23.2462f };

    constexpr int kBossPattern150Layer = 150;
    constexpr float kBossPattern150Radius = 5.0f;
    constexpr float kBossPattern150Damage = 35.0f;
    constexpr float kBossPattern150DamageDelay = 2.0f;
    constexpr int kBossWipeLayer = 100;
    constexpr float kBossWipeDamageDelay = 5.0f;
    constexpr int kBossMirrorPatternLayer = 50;
    constexpr int kBossMirrorSlotCount = 3;
    constexpr int kBossMirrorCenterIndex = 1;
    constexpr int kBossPatternRadiusRingSegmentCount = 128;
    constexpr float kBossBarY = 0.84f;
    constexpr float kBossBarMaxScaleX = 0.38f;
    constexpr float kBossAreaRadius = 12.0f;
    constexpr float kBossEngageRadius = 13.5f;
    constexpr float kBossLeashRadius = 9.25f;
    constexpr float kBossTurnSpeed = 4.8f;
    constexpr float kBossMoveSpeed = 2.25f;
    constexpr float kBossStrafeSpeed = 1.65f;
    constexpr float kBossAttackStepSpeed = 1.1f;
    constexpr float kBossPreferredMinDistance = 2.8f;
    constexpr float kBossPreferredMaxDistance = 5.4f;
    constexpr float kBossAttackDistance = 3.2f;
    constexpr float kBossAttackDamage = 18.0f;
    constexpr float kBossAttackCooldown = 2.2f;
    constexpr float kBossServerAttackReplayInterval = 2.4f;
    constexpr float kBossAttackWindupDuration = 0.55f;
    constexpr float kBossAttackRecoverDuration = 0.7f;
    constexpr float kBossAttackHitBoxForwardBias = 0.0f;
    constexpr bool kShowBossAttackDebugHitBoxes = false;
    constexpr wchar_t kBossIntroRoarSound[] = L"Sounds\\Boss\\Boss_IntroRoar.mp3";
    constexpr wchar_t kBossMeleeAttackSound[] = L"Sounds\\Boss\\Boss_MeleeAttack.mp3";
    constexpr wchar_t kBossWhipAttackSound[] = L"Sounds\\Boss\\Boss_WhipAttack.mp3";
    constexpr wchar_t kBossRoar150Sound[] = L"Sounds\\Boss\\Boss_Roar150.mp3";
    constexpr wchar_t kBossDeathSound[] = L"Sounds\\Boss\\Boss_Death.mp3";
    constexpr float kBossIntroRoarVolume = 0.13f;
    constexpr float kBossMeleeAttackVolume = 0.11f;
    constexpr float kBossWhipAttackVolume = 0.12f;
    constexpr float kBossRoar150Volume = 0.08f;
    constexpr float kBossDeathVolume = 0.14f;
    constexpr float kBossAttackAnimationBlendDuration = 0.1f;
    constexpr float kBossPatternAnimationBlendDuration = 0.14f;
    constexpr float kBossWipeAttackBlendDuration = 0.18f;
    constexpr float kBossRoarFallbackDuration = 2.0f;
    constexpr float kBossSummonFallbackDuration = 1.6f;
    constexpr float kBossSwordAttackFallbackDuration = 1.4f;
    constexpr float kBossStrafeDuration = 1.1f;
    constexpr float kBossMirrorSummonDuration = 0.42f;
    constexpr float kBossMirrorDiveDuration = 0.58f;
    constexpr float kBossMirrorHiddenDuration = 0.18f;
    constexpr float kBossMirrorDiveArcHeight = 1.25f;
    constexpr float kBossMirrorWidth = 3.65f;
    constexpr float kBossMirrorHeight = 3.78f;
    constexpr float kBossMirrorDepth = 0.14f;
    constexpr float kBossMirrorPaneDepth = 0.045f;
    constexpr float kBossMirrorFrameThickness = 0.085f;
    constexpr float kBossMirrorFrameDepth = 0.15f;
    constexpr float kBossMirrorInnerInsetX = 0.03f;
    constexpr float kBossMirrorInnerInsetY = 0.04f;
    constexpr float kBossMirrorSheenWidth = 0.14f;
    constexpr float kBossMirrorSheenHeight = 0.7f;
    constexpr float kBossMirrorSheenDepth = 0.025f;
    constexpr float kBossMirrorSheenOffsetX = 0.18f;
    constexpr float kBossMirrorSheenFrontOffset = 0.03f;
    constexpr float kBossMirrorSplitDepth = 1.05f;
    constexpr float kBossMirrorHiddenDepth = 1.35f;
    constexpr DirectX::XMFLOAT4 kBossMirrorTint = { 0.20f, 0.26f, 0.34f, 1.0f };
    constexpr float kBossMirrorPaneAlpha = 0.42f;
    constexpr DirectX::XMFLOAT4 kBossMirrorDarkTint = { 0.025f, 0.03f, 0.05f, 1.0f };
    constexpr float kBossMirrorDarkAlpha = 0.92f;
    constexpr DirectX::XMFLOAT4 kBossMirrorFrameTint = { 0.34f, 0.31f, 0.22f, 1.0f };
    constexpr DirectX::XMFLOAT4 kBossMirrorFrameEdgeTint = { 0.76f, 0.66f, 0.34f, 1.0f };
    constexpr DirectX::XMFLOAT4 kBossMirrorSheenTint = { 0.96f, 0.98f, 1.0f, 0.28f };
    constexpr DirectX::XMFLOAT4 kBossMirrorFakeCloneTint = { 1.0f, 1.0f, 1.0f, 1.0f };

    const std::array<DirectX::XMFLOAT3, kBossMirrorSlotCount> kBossMirrorGroundPositions =
    {
        DirectX::XMFLOAT3{ -3.47464f, 6.01219f, 31.95f },
        DirectX::XMFLOAT3{ -7.52464f, 6.01219f, 31.95f },
        DirectX::XMFLOAT3{ -11.97464f, 6.01219f, 31.95f }
    };

    float WrapAngle(float angle)
    {
        while (angle > DirectX::XM_PI)
        {
            angle -= DirectX::XM_2PI;
        }

        while (angle < -DirectX::XM_PI)
        {
            angle += DirectX::XM_2PI;
        }

        return angle;
    }

    DirectX::XMFLOAT3 GetBossMirrorDisplayPosition(int index)
    {
        const DirectX::XMFLOAT3& ground = kBossMirrorGroundPositions[static_cast<size_t>(index)];
        return { ground.x, ground.y + (kBossMirrorHeight * 0.5f), ground.z };
    }

    DirectX::XMFLOAT3 GetBossMirrorCombinedDisplayPosition()
    {
        const DirectX::XMFLOAT3 left = GetBossMirrorDisplayPosition(0);
        const DirectX::XMFLOAT3 right = GetBossMirrorDisplayPosition(kBossMirrorSlotCount - 1);
        return
        {
            (left.x + right.x) * 0.5f,
            (left.y + right.y) * 0.5f,
            (left.z + right.z) * 0.5f
        };
    }

    float GetBossMirrorCombinedWidth()
    {
        const DirectX::XMFLOAT3 left = GetBossMirrorDisplayPosition(0);
        const DirectX::XMFLOAT3 right = GetBossMirrorDisplayPosition(kBossMirrorSlotCount - 1);
        return std::fabs(right.x - left.x) + kBossMirrorWidth;
    }

    float ComputeBossVisualYaw(float worldYaw)
    {
        return WrapAngle(worldYaw + DirectX::XM_PI);
    }

    float GetBossMirrorOutwardWorldYaw()
    {
        const DirectX::XMFLOAT3 from = GetBossMirrorDisplayPosition(kBossMirrorCenterIndex);
        return std::atan2(
            kStage2PlayerStartPosition.x - from.x,
            kStage2PlayerStartPosition.z - from.z);
    }

    float GetBossMirrorInwardWorldYaw()
    {
        return WrapAngle(GetBossMirrorOutwardWorldYaw() + DirectX::XM_PI);
    }

    float GetBossMirrorOutwardVisualYaw()
    {
        return ComputeBossVisualYaw(GetBossMirrorOutwardWorldYaw());
    }

    float GetBossMirrorInwardVisualYaw()
    {
        return ComputeBossVisualYaw(GetBossMirrorInwardWorldYaw());
    }

    DirectX::XMFLOAT3 GetBossMirrorClonePosition(int index)
    {
        const DirectX::XMFLOAT3& ground = kBossMirrorGroundPositions[static_cast<size_t>(index)];
        const float bossFloorOffset = kStage2BossSpawnPosition.y - kStage2BossAnchorPosition.y;
        const float inwardYaw = GetBossMirrorInwardWorldYaw();
        return {
            ground.x + std::sin(inwardYaw) * kBossMirrorSplitDepth,
            ground.y + bossFloorOffset,
            ground.z + std::cos(inwardYaw) * kBossMirrorSplitDepth
        };
    }

    DirectX::XMFLOAT3 GetBossMirrorHiddenPosition(int index)
    {
        const DirectX::XMFLOAT3& ground = kBossMirrorGroundPositions[static_cast<size_t>(index)];
        const float bossFloorOffset = kStage2BossSpawnPosition.y - kStage2BossAnchorPosition.y;
        const float inwardYaw = GetBossMirrorInwardWorldYaw();
        return {
            ground.x + std::sin(inwardYaw) * kBossMirrorHiddenDepth,
            ground.y + bossFloorOffset,
            ground.z + std::cos(inwardYaw) * kBossMirrorHiddenDepth
        };
    }

    float ComputeFacingYaw(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to)
    {
        return std::atan2(to.x - from.x, to.z - from.z);
    }
}

void Stage2BossController::Initialize(
    EclipseWalkerGame* game,
    MapSystem* mapSystem,
    DamageTextRenderer* damageTextRenderer,
    const TrackOwnedCallback& trackOwned)
{
    Reset();

    mGame = game;
    mMapSystem = mapSystem;
    mDamageTextRenderer = damageTextRenderer;
    mTrackOwned = trackOwned;

    BuildBoss();
    BuildBossPatternIndicator();
    BuildBossMirrorPatternObjects();
}

void Stage2BossController::InitializeHealthText()
{
    auto* device = mGame != nullptr ? mGame->GetDevice() : nullptr;
    auto* cmdQueue = mGame != nullptr ? mGame->GetCommandQueue() : nullptr;
    if (device == nullptr || cmdQueue == nullptr)
    {
        return;
    }

    try
    {
        if (!mBossHealthTextHeap)
        {
            mBossHealthTextHeap = std::make_unique<DirectX::DescriptorHeap>(
                device,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                1);
        }

        if (!mBossHealthTextFont || !mBossHealthTextBatch)
        {
            DirectX::ResourceUploadBatch resourceUpload(device);
            resourceUpload.Begin();

            if (!mBossHealthTextFont)
            {
                mBossHealthTextFont = std::make_unique<DirectX::SpriteFont>(
                    device,
                    resourceUpload,
                    L"Textures/chat_korean.spritefont",
                    mBossHealthTextHeap->GetCpuHandle(0),
                    mBossHealthTextHeap->GetGpuHandle(0));
            }

            if (!mBossHealthTextBatch)
            {
                DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
                DirectX::SpriteBatchPipelineStateDescription pd(rtState);
                mBossHealthTextBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
            }

            auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
            uploadResourcesFinished.wait();
        }
    }
    catch (const std::exception& e)
    {
        std::string log = "[Stage2BossUI] Failed to initialize boss HP font: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());

        mBossHealthTextFont.reset();
        mBossHealthTextBatch.reset();
        mBossHealthTextHeap.reset();
    }
}

void Stage2BossController::Reset()
{
    if (mGame != nullptr)
    {
        if (auto* uiManager = mGame->GetUIManager())
        {
            uiManager->HideBossHealthBar();
            uiManager->HideMirrorCrackWarning();
        }
    }

    mBoss = nullptr;
    mBossPatternRadiusObj = nullptr;
    mBossPatternRadiusRingObj = nullptr;
    mShowBossHealthText = false;
    mBossPattern150Triggered = false;
    mBossPattern150DamagePending = false;
    mBossWipeTriggered = false;
    mBossWipeDamagePending = false;
    mBossMirrorPatternTriggered = false;
    mBossMoveState = BossMoveState::Idle;
    mBossMirrorPatternState = BossMirrorPatternState::Inactive;
    mBossBasicAttackType = BossBasicAttackType::TwoHitCombo;
    mBossScriptedAnimationState = BossScriptedAnimationState::None;
    mBossHealthTextLayer = 0;
    mLastServerState = -1;
    mBossMirrorRealIndex = kBossMirrorCenterIndex;
    mBossFacingYaw = 0.0f;
    mBossAttackCooldownTimer = 0.0f;
    mBossActionTimer = 0.0f;
    mBossAttackRecoverDuration = kBossAttackRecoverDuration;
    mBossScriptedAnimationTimer = 0.0f;
    mBossStrafeDirection = 1.0f;
    mBossMirrorPatternTimer = 0.0f;
    mBossMirrorResolveHp = 0.0f;
    mBossMirrorSplitYaw = 0.0f;
    mBossMirrorDiveStart = { 0.0f, 0.0f, 0.0f };
    mBossMirrorDiveTarget = { 0.0f, 0.0f, 0.0f };
    mBossPatternRadiusTimer = 0.0f;
    mBossPatternRadiusDuration = 0.0f;
    mBossPattern150DamageTimer = 0.0f;
    mBossWipeDamageTimer = 0.0f;
    mBossWipeDamageDuration = 0.0f;
    mBossAttackAnimationDuration = 0.0f;
    mBossAttackNextHitIndex = 0;
    StopBossPattern150Sound();
    mBossDeathSoundPlayed = false;
    mBossAttackRandomState = 0x5EED1234u;
    mBossAnimationDebugActive = false;
    mBossAnimationDebugPreviousKeyPressed = false;
    mBossAnimationDebugNextKeyPressed = false;
    mBossAnimationDebugReplayKeyPressed = false;
    mBossAnimationDebugExitKeyPressed = false;
    mBossAnimationDebugClipIndex = static_cast<std::size_t>(-1);
    mBossPattern150DamageCenter = { 0.0f, 0.0f, 0.0f };
    mBossWipeDamageCenter = { 0.0f, 0.0f, 0.0f };
    mBossMirrorObjects = {};
    mBossMirrorFrameTopObjects = {};
    mBossMirrorFrameBottomObjects = {};
    mBossMirrorFrameLeftObjects = {};
    mBossMirrorFrameRightObjects = {};
    mBossMirrorSheenObjects = {};
    mBossMirrorCloneObjects = {};
    mBossAttackDebugVisualizer.Reset();

    mBossHealthTextFont.reset();
    mBossHealthTextBatch.reset();
    mBossHealthTextHeap.reset();

    mGame = nullptr;
    mMapSystem = nullptr;
    mDamageTextRenderer = nullptr;
    mTrackOwned = nullptr;
}

void Stage2BossController::Update(const GameTimer& gt, Player* player, bool isOtherWorld)
{
    const float dt = gt.DeltaTime();

    if (!mBossDeathSoundPlayed &&
        mBoss != nullptr &&
        (mBoss->GetState() == MonsterState::DYING || mBoss->GetState() == MonsterState::DIE))
    {
        AudioManager::Get().PlayEffect(kBossDeathSound, kBossDeathVolume);
        mBossDeathSoundPlayed = true;
    }

    if (kEnableBossAnimationDebug)
    {
        UpdateBossAnimationDebugInput();
    }

    if (!mBossAnimationDebugActive)
    {
        UpdateBossPatternIndicator(dt);
        UpdateBossPattern150Damage(player, dt);
        UpdateBossWipeDamage(player, isOtherWorld, dt);
        UpdateBossMirrorPattern(player, isOtherWorld, dt);
        UpdateBossScriptedAnimation(dt);
    }
    UpdateBossWorldVisibility(isOtherWorld);

    if (!mBossAnimationDebugActive && mBoss != nullptr && mBoss->GetState() != MonsterState::DIE)
    {
        mBossAttackCooldownTimer = (std::max)(0.0f, mBossAttackCooldownTimer - dt);

        if (mBoss->UpdateAnimationState(dt))
        {
            ResetNormalBehavior();
        }
        else if (!IsBossScriptedAnimationActive() &&
            !NetworkManager::Get()->IsConnected() &&
            mBossMirrorPatternState == BossMirrorPatternState::Inactive)
        {
            UpdateNormalBehavior(player, isOtherWorld, dt);
        }
        else if (!IsBossScriptedAnimationActive() &&
            NetworkManager::Get()->IsConnected() &&
            mBossMirrorPatternState == BossMirrorPatternState::Inactive &&
            mLastServerState == 2 &&
            mBossAttackCooldownTimer <= 0.0f)
        {
            BeginBossAttack();
            mBossAttackCooldownTimer = kBossServerAttackReplayInterval;
        }

        UpdateBossAttackSequence(player, dt);
    }

    const int currentBossLayer = GetCurrentHealthLayer();
    if (!mBossAnimationDebugActive && !NetworkManager::Get()->IsConnected())
    {
        UpdateBossPatternTriggers(player, currentBossLayer);
    }
    UpdateBossHealthUi(player, currentBossLayer, isOtherWorld);
    UpdateBossAttackDebugVisualizer(isOtherWorld);
}

void Stage2BossController::Draw()
{
    DrawBossHealthText();
}

bool Stage2BossController::IsInvulnerable() const
{
    if (mBoss == nullptr || mBoss->GetState() == MonsterState::DIE || mBoss->GetState() == MonsterState::DYING)
    {
        return false;
    }

    return mBossPatternRadiusTimer > 0.0f ||
        mBossPattern150DamagePending ||
        mBossWipeDamagePending ||
        IsBossScriptedAnimationActive() ||
        mBossMirrorPatternState == BossMirrorPatternState::Summon ||
        mBossMirrorPatternState == BossMirrorPatternState::Dive ||
        mBossMirrorPatternState == BossMirrorPatternState::Hidden;
}

int Stage2BossController::GetCurrentHealthLayer() const
{
    if (mBoss == nullptr || mBoss->GetState() == MonsterState::DIE)
    {
        return 0;
    }

    return CalculateBossHealthLayer(mBoss->GetHP(), mBoss->GetMaxHP());
}

void Stage2BossController::ApplyServerSync(int state, float x, float y, float z, float rotY)
{
    if (mBoss == nullptr)
    {
        return;
    }

    if (state == 3)
    {
        StopBossPattern150Sound();
        mBossScriptedAnimationState = BossScriptedAnimationState::None;
        mBossScriptedAnimationTimer = 0.0f;
        mBoss->ApplyServerHit(0, true);
        return;
    }

    if (mBoss->GetState() == MonsterState::DIE || mBoss->GetState() == MonsterState::DYING)
    {
        return;
    }

    const bool normalAnimationAllowed =
        !IsBossScriptedAnimationActive() &&
        mBossMirrorPatternState == BossMirrorPatternState::Inactive;
    if (normalAnimationAllowed && state != mLastServerState)
    {
        if (auto* animation = mBoss->GetSkeletalAnimation())
        {
            if (state == 1)
            {
                animation->Play("SkeletonWalk", 0.12f);
            }
            else if (state == 2)
            {
                BeginBossAttack();
                mBossAttackCooldownTimer = kBossServerAttackReplayInterval;
            }
            else
            {
                animation->Play("SkeletonIdle", 0.12f);
                mBossAttackCooldownTimer = 0.0f;
            }
        }
        mLastServerState = state;
    }

    mBoss->SetPosition(x, y, z);
    mBossFacingYaw = ComputeBossVisualYaw(rotY * (3.14159265f / 180.0f));
    mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
    mBoss->GameObject::Update();
}

void Stage2BossController::ApplyServerHit(int remainHp, bool isDead)
{
    if (mBoss == nullptr)
    {
        return;
    }

    mBoss->ApplyServerHit(remainHp, isDead);

    if (isDead)
    {
        StopBossPattern150Sound();
        mBossScriptedAnimationState = BossScriptedAnimationState::None;
        mBossScriptedAnimationTimer = 0.0f;
    }

    if (mBossMirrorPatternState == BossMirrorPatternState::Split &&
        (isDead || mBoss->GetHP() < mBossMirrorResolveHp - 0.01f))
    {
        EndBossMirrorPattern();
    }
}

void Stage2BossController::ApplyServerPattern(int patternType, float x, float y, float z, float radius, float delay, int damage, int patternData)
{
    if (patternType == BOSS_PATTERN_STAGE2_SHOCKWAVE)
    {
        const float indicatorRadius = radius > 0.0f ? radius : kBossPattern150Radius;
        const float indicatorDelay = delay > 0.0f ? delay : kBossPattern150DamageDelay;
        mBossPattern150Triggered = true;
        mBossPattern150DamagePending = false;
        mBossPattern150DamageTimer = 0.0f;
        ShowBossPatternRadiusIndicator({ x, y, z }, indicatorRadius, indicatorDelay);
        PlayBossScriptedAnimation(
            BossScriptedAnimationState::Pattern150Roar,
            "BossRoar",
            kBossRoarFallbackDuration,
            kBossPatternAnimationBlendDuration);
        OutputDebugStringA("[Stage2Boss][Pattern] Server shockwave triggered\n");
        return;
    }

    if (patternType == BOSS_PATTERN_STAGE2_WIPE)
    {
        mBossWipeTriggered = true;
        mBossWipeDamagePending = true;
        mBossWipeDamageTimer = delay > 0.0f ? delay : kBossWipeDamageDelay;
        mBossWipeDamageDuration = mBossWipeDamageTimer;
        mBossWipeDamageCenter = { x, y, z };
        PlayBossScriptedAnimation(
            BossScriptedAnimationState::WipeSummon,
            "BossSummonSwordWhip",
            kBossSummonFallbackDuration,
            kBossPatternAnimationBlendDuration);
        if (auto* uiManager = mGame != nullptr ? mGame->GetUIManager() : nullptr)
        {
            uiManager->ShowMirrorCrackWarning(0.0f);
        }
        OutputDebugStringA("[Stage2Boss][Pattern] Server lantern wipe triggered\n");
        return;
    }

    if (patternType == BOSS_PATTERN_STAGE2_MIRROR)
    {
        mBossMirrorPatternTriggered = true;
        TriggerBossMirrorPattern(nullptr, patternData);
        OutputDebugStringA("[Stage2Boss][Pattern] Server mirror pattern triggered\n");
    }
}

DirectX::XMFLOAT3 Stage2BossController::GetBossAnchorPosition()
{
    return kStage2BossAnchorPosition;
}

DirectX::XMFLOAT3 Stage2BossController::GetBossSpawnPosition()
{
    return kStage2BossSpawnPosition;
}

DirectX::XMFLOAT3 Stage2BossController::GetPlayerStartPosition()
{
    return kStage2PlayerStartPosition;
}

void Stage2BossController::SetPatternObjectVisible(
    GameObject* object,
    bool visible,
    const DirectX::XMFLOAT4& color) const
{
    if (object == nullptr || object->Ritem == nullptr)
    {
        return;
    }

    object->Ritem->Visible = visible;
    object->Ritem->ColorMultiplier = color;
    object->Ritem->NumFramesDirty = gNumFrameResources;
}

void Stage2BossController::BuildBossMirrorPatternObjects()
{
    auto* res = mGame != nullptr ? mGame->GetResources() : nullptr;
    if (res == nullptr || mBoss == nullptr)
    {
        return;
    }

    mBossMirrorObjects = {};
    mBossMirrorFrameTopObjects = {};
    mBossMirrorFrameBottomObjects = {};
    mBossMirrorFrameLeftObjects = {};
    mBossMirrorFrameRightObjects = {};
    mBossMirrorSheenObjects = {};

    auto geoIt = res->mGeometries.find("boxGeo");
    if (geoIt == res->mGeometries.end() || geoIt->second == nullptr)
    {
        return;
    }

    constexpr const char* kMirrorMaterialName = "Stage2BossMirrorPaneMat";
    constexpr const char* kMirrorFrameMaterialName = "Stage2BossMirrorFrameMat";
    constexpr const char* kMirrorSheenMaterialName = "Stage2BossMirrorSheenMat";
    constexpr const char* kMirrorCloneMaterialName = "Stage2BossMirrorCloneMat";

    if (res->GetMaterial(kMirrorMaterialName) == nullptr)
    {
        const DirectX::XMFLOAT4 mirrorMaterialTint =
        {
            kBossMirrorTint.x,
            kBossMirrorTint.y,
            kBossMirrorTint.z,
            1.0f
        };

        res->CreateMaterial(
            kMirrorMaterialName,
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            mirrorMaterialTint,
            { 0.08f, 0.12f, 0.18f },
            0.12f);
    }

    if (Material* mirrorMaterial = res->GetMaterial(kMirrorMaterialName))
    {
        mirrorMaterial->DiffuseAlbedo =
        {
            kBossMirrorTint.x,
            kBossMirrorTint.y,
            kBossMirrorTint.z,
            1.0f
        };
        mirrorMaterial->FresnelR0 = { 0.08f, 0.12f, 0.18f };
        mirrorMaterial->Roughness = 0.12f;
        mirrorMaterial->IsTransparent = 2;
        mirrorMaterial->IsToon = 0;
        mirrorMaterial->NumFramesDirty = gNumFrameResources;
    }

    if (res->GetMaterial(kMirrorFrameMaterialName) == nullptr)
    {
        res->CreateMaterial(
            kMirrorFrameMaterialName,
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            kBossMirrorFrameTint,
            { 0.22f, 0.18f, 0.08f },
            0.28f);
    }

    if (Material* frameMaterial = res->GetMaterial(kMirrorFrameMaterialName))
    {
        frameMaterial->DiffuseAlbedo = kBossMirrorFrameTint;
        frameMaterial->FresnelR0 = { 0.22f, 0.18f, 0.08f };
        frameMaterial->Roughness = 0.28f;
        frameMaterial->IsTransparent = 0;
        frameMaterial->IsToon = 0;
        frameMaterial->NumFramesDirty = gNumFrameResources;
    }

    if (res->GetMaterial(kMirrorSheenMaterialName) == nullptr)
    {
        res->CreateMaterial(
            kMirrorSheenMaterialName,
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            kBossMirrorSheenTint,
            { 0.12f, 0.12f, 0.14f },
            0.08f);
    }

    if (Material* sheenMaterial = res->GetMaterial(kMirrorSheenMaterialName))
    {
        sheenMaterial->DiffuseAlbedo = kBossMirrorSheenTint;
        sheenMaterial->FresnelR0 = { 0.12f, 0.12f, 0.14f };
        sheenMaterial->Roughness = 0.08f;
        sheenMaterial->IsTransparent = 1;
        sheenMaterial->IsToon = 0;
        sheenMaterial->NumFramesDirty = gNumFrameResources;
    }

    const auto& boxArgs = geoIt->second->DrawArgs["box"];
    auto CreateMirrorBox = [&](const char* materialName,
        const DirectX::XMFLOAT3& scale,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& color,
        bool castShadow,
        float rotZ = 0.0f)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());
        ritem->Geo = geoIt->second.get();
        ritem->Mat = res->GetMaterial(materialName);
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = boxArgs.IndexCount;
        ritem->StartIndexLocation = boxArgs.StartIndexLocation;
        ritem->BaseVertexLocation = boxArgs.BaseVertexLocation;
        ritem->Visible = false;
        ritem->CastShadow = castShadow;
        ritem->ColorMultiplier = color;

        auto object = std::make_unique<GameObject>();
        object->Ritem = ritem.get();
        // boxGeo is built from -1 to +1, so these values are treated as half-extents.
        object->SetScale(scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f);
        object->SetPosition(position.x, position.y, position.z);
        if (rotZ != 0.0f)
        {
            object->SetRotation(0.0f, 0.0f, rotZ);
        }
        object->Update();

        GameObject* rawObject = object.get();
        TrackOwned(rawObject, ritem.get());
        mGame->GetRitems().push_back(std::move(ritem));
        mGame->GetGameObjects().push_back(std::move(object));
        return rawObject;
    };

    {
        const DirectX::XMFLOAT3 mirrorPos = GetBossMirrorCombinedDisplayPosition();
        const float combinedWidth = GetBossMirrorCombinedWidth();
        const float frameHalfWidth = combinedWidth * 0.5f;
        const float frameHalfHeight = kBossMirrorHeight * 0.5f;
        const float sideX = frameHalfWidth - (kBossMirrorFrameThickness * 0.5f);
        const float topY = frameHalfHeight - (kBossMirrorFrameThickness * 0.5f);
        const float paneWidth = (std::max)(0.1f, combinedWidth - (kBossMirrorFrameThickness * 2.0f) - (kBossMirrorInnerInsetX * 2.0f));
        const float paneHeight = (std::max)(0.1f, kBossMirrorHeight - (kBossMirrorFrameThickness * 2.0f) - (kBossMirrorInnerInsetY * 2.0f));
        const size_t mirrorIndex = static_cast<size_t>(kBossMirrorCenterIndex);

        mBossMirrorObjects[mirrorIndex] = CreateMirrorBox(
            kMirrorMaterialName,
            { paneWidth, paneHeight, kBossMirrorPaneDepth },
            { mirrorPos.x, mirrorPos.y, mirrorPos.z + 0.005f },
            kBossMirrorTint,
            false);

        mBossMirrorFrameTopObjects[mirrorIndex] = CreateMirrorBox(
            kMirrorFrameMaterialName,
            { combinedWidth, kBossMirrorFrameThickness, kBossMirrorFrameDepth },
            { mirrorPos.x, mirrorPos.y + topY, mirrorPos.z },
            kBossMirrorFrameEdgeTint,
            false);

        mBossMirrorFrameBottomObjects[mirrorIndex] = CreateMirrorBox(
            kMirrorFrameMaterialName,
            { combinedWidth, kBossMirrorFrameThickness, kBossMirrorFrameDepth },
            { mirrorPos.x, mirrorPos.y - topY, mirrorPos.z },
            kBossMirrorFrameTint,
            false);

        mBossMirrorFrameLeftObjects[mirrorIndex] = CreateMirrorBox(
            kMirrorFrameMaterialName,
            { kBossMirrorFrameThickness, kBossMirrorHeight, kBossMirrorFrameDepth },
            { mirrorPos.x - sideX, mirrorPos.y, mirrorPos.z },
            kBossMirrorFrameEdgeTint,
            false);

        mBossMirrorFrameRightObjects[mirrorIndex] = CreateMirrorBox(
            kMirrorFrameMaterialName,
            { kBossMirrorFrameThickness, kBossMirrorHeight, kBossMirrorFrameDepth },
            { mirrorPos.x + sideX, mirrorPos.y, mirrorPos.z },
            kBossMirrorFrameTint,
            false);

        mBossMirrorSheenObjects[mirrorIndex] = CreateMirrorBox(
            kMirrorSheenMaterialName,
            { kBossMirrorSheenWidth, paneHeight * kBossMirrorSheenHeight, kBossMirrorSheenDepth },
            { mirrorPos.x + kBossMirrorSheenOffsetX, mirrorPos.y + 0.08f, mirrorPos.z - kBossMirrorSheenFrontOffset },
            kBossMirrorSheenTint,
            false,
            -0.18f);
    }

    const float bossHalfHeight = mBoss->GetColliderHalfHeight();
    for (int i = 0; i < kBossMirrorSlotCount; ++i)
    {
        auto cloneRitem = std::make_unique<RenderItem>();
        cloneRitem->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());
        cloneRitem->Visible = false;
        cloneRitem->CastShadow = false;
        cloneRitem->ColorMultiplier = kBossMirrorFakeCloneTint;

        auto cloneObj = std::make_unique<GameObject>();

        CharacterVisualSpec visualSpec;
        visualSpec.UseSkinned = true;
        visualSpec.ModelPath = GetBossModelPath();
        visualSpec.DefaultClipName = "SkeletonIdle";
        visualSpec.LoadModelAnimations = false;
        visualSpec.AdditionalAnimationClips.push_back({
            "Models/Animated/Boss/DemonLord@Idle.ufbx",
            "SkeletonIdle"
        });
        visualSpec.GeometryName = "stage2BossDemonLordGeo";
        visualSpec.MaterialName = kMirrorCloneMaterialName;
        visualSpec.DiffuseTextureName = "Stage2BossDemonLordBaseColor";
        visualSpec.DiffuseTexturePath = L"Textures/T_DemonLordBody_BaseColor.dds";
        visualSpec.EmissiveTextureName = "Stage2BossDemonLordEmissive";
        visualSpec.EmissiveTexturePath = L"Textures/T_DemonLordBody_Emissive.dds";
        visualSpec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        visualSpec.FresnelR0 = { 0.08f, 0.04f, 0.03f };
        visualSpec.Roughness = 0.62f;
        visualSpec.IsToon = false;
        visualSpec.OutlineThickness = 0.0f;
        visualSpec.OutlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        visualSpec.FallbackMaterialName = "MonsterRed";
        visualSpec.FallbackScale = { 0.72f, 1.85f, 0.72f };
        visualSpec.SpawnPosition = GetBossMirrorClonePosition(i);
        visualSpec.UseActorOrigin = true;
        visualSpec.CenterBoundsXZ = false;
        visualSpec.OriginToFloor = bossHalfHeight + 0.6f;
        visualSpec.RotationOffset = { 0.0f, DirectX::XM_PI, 0.0f };
        visualSpec.TargetHeight = bossHalfHeight * 1.75f;

        if (!CharacterVisualFactory::ApplyVisual(
            cloneObj.get(),
            cloneRitem.get(),
            mGame->GetDevice(),
            mGame->GetCommandList(),
            res,
            visualSpec))
        {
            continue;
        }

        if (auto* animation = cloneObj->GetSkeletalAnimation())
        {
            animation->Play("SkeletonIdle");
        }

        cloneObj->Ritem->Visible = false;
        cloneObj->Ritem->CastShadow = false;
        cloneObj->Ritem->ColorMultiplier = kBossMirrorFakeCloneTint;
        cloneObj->Ritem->NumFramesDirty = gNumFrameResources;

        mBossMirrorCloneObjects[static_cast<size_t>(i)] = cloneObj.get();
        TrackOwned(cloneObj.get(), cloneRitem.get());
        mGame->GetRitems().push_back(std::move(cloneRitem));
        mGame->GetGameObjects().push_back(std::move(cloneObj));
    }
}

void Stage2BossController::TriggerBossMirrorPattern(Player* player, int mirrorRealIndex)
{
    if (mBoss == nullptr || mBoss->GetState() == MonsterState::DIE)
    {
        return;
    }

    mBossMirrorPatternState = BossMirrorPatternState::Summon;
    mBossMirrorPatternTimer = kBossMirrorSummonDuration;
    mBossMirrorResolveHp = mBoss->GetHP();
    mBossMirrorRealIndex =
        mirrorRealIndex >= 0 && mirrorRealIndex < kBossMirrorSlotCount
        ? mirrorRealIndex
        : std::rand() % kBossMirrorSlotCount;
    mBossMirrorDiveStart = mBoss->GetPosition();
    mBossMirrorDiveTarget = GetBossMirrorDisplayPosition(kBossMirrorCenterIndex);
    mBossScriptedAnimationState = BossScriptedAnimationState::None;
    mBossScriptedAnimationTimer = 0.0f;
    ResetNormalBehavior();
    SetBossLocomotionState(false);

    for (int i = 0; i < kBossMirrorSlotCount; ++i)
    {
        SetPatternObjectVisible(
            mBossMirrorObjects[static_cast<size_t>(i)],
            true,
            { kBossMirrorDarkTint.x, kBossMirrorDarkTint.y, kBossMirrorDarkTint.z, kBossMirrorDarkAlpha });
        SetPatternObjectVisible(mBossMirrorSheenObjects[static_cast<size_t>(i)], false, kBossMirrorSheenTint);
        SetPatternObjectVisible(mBossMirrorCloneObjects[static_cast<size_t>(i)], false, kBossMirrorFakeCloneTint);
    }

    mBossFacingYaw = ComputeBossVisualYaw(
        ComputeFacingYaw(mBossMirrorDiveStart, mBossMirrorDiveTarget));
    mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
    mBoss->GameObject::Update();

    OutputDebugStringA("[Stage2Boss][Pattern] Mirror summon started\n");
}

void Stage2BossController::UpdateBossMirrorPattern(Player* player, bool isOtherWorld, float dt)
{
    if (mBoss == nullptr || mBossMirrorPatternState == BossMirrorPatternState::Inactive)
    {
        return;
    }

    if (mBoss->GetState() == MonsterState::DIE || mBoss->GetState() == MonsterState::DYING)
    {
        EndBossMirrorPattern();
        return;
    }

    auto ApplyMirrorPresentationRotation = [&](GameObject* object)
    {
        if (object == nullptr)
        {
            return;
        }

        object->SetRotation(0.0f, mBossMirrorSplitYaw, 0.0f);
        object->Update();
    };

    const bool revealMirrorClones = (mBossMirrorPatternState == BossMirrorPatternState::Split);
    const float pulse = 0.85f + std::sin(static_cast<float>(GetTickCount64() % 100000ULL) * 0.01f) * 0.15f;
    const DirectX::XMFLOAT4 mirrorPulseTint =
    {
        kBossMirrorTint.x * pulse,
        kBossMirrorTint.y,
        kBossMirrorTint.z,
        kBossMirrorPaneAlpha
    };
    const DirectX::XMFLOAT4 mirrorDarkTint =
    {
        kBossMirrorDarkTint.x,
        kBossMirrorDarkTint.y,
        kBossMirrorDarkTint.z,
        kBossMirrorDarkAlpha
    };
    const DirectX::XMFLOAT4 activeMirrorTint = revealMirrorClones ? mirrorPulseTint : mirrorDarkTint;
    for (int i = 0; i < kBossMirrorSlotCount; ++i)
    {
        const DirectX::XMFLOAT3 mirrorPos =
            (i == kBossMirrorCenterIndex)
            ? GetBossMirrorCombinedDisplayPosition()
            : GetBossMirrorDisplayPosition(i);
        const float sheenPhase = static_cast<float>(GetTickCount64() % 100000ULL) * 0.0038f + static_cast<float>(i) * 0.55f;
        const float sheenOffset = std::sin(sheenPhase) * 0.22f;
        const float sheenAlpha = 0.18f + (std::sin(sheenPhase * 1.2f) * 0.5f + 0.5f) * 0.14f;

        SetPatternObjectVisible(
            mBossMirrorObjects[static_cast<size_t>(i)],
            !isOtherWorld,
            activeMirrorTint);

        SetPatternObjectVisible(
            mBossMirrorFrameTopObjects[static_cast<size_t>(i)],
            !isOtherWorld,
            kBossMirrorFrameEdgeTint);
        SetPatternObjectVisible(
            mBossMirrorFrameBottomObjects[static_cast<size_t>(i)],
            !isOtherWorld,
            kBossMirrorFrameTint);
        SetPatternObjectVisible(
            mBossMirrorFrameLeftObjects[static_cast<size_t>(i)],
            !isOtherWorld,
            kBossMirrorFrameEdgeTint);
        SetPatternObjectVisible(
            mBossMirrorFrameRightObjects[static_cast<size_t>(i)],
            !isOtherWorld,
            kBossMirrorFrameTint);

        if (GameObject* sheenObj = mBossMirrorSheenObjects[static_cast<size_t>(i)])
        {
            const float paneHeight = (std::max)(0.1f, kBossMirrorHeight - (kBossMirrorFrameThickness * 2.0f) - (kBossMirrorInnerInsetY * 2.0f));
            sheenObj->SetPosition(
                mirrorPos.x + kBossMirrorSheenOffsetX + sheenOffset,
                mirrorPos.y + 0.08f,
                mirrorPos.z - kBossMirrorSheenFrontOffset);
            sheenObj->SetScale(
                kBossMirrorSheenWidth * 0.5f,
                paneHeight * kBossMirrorSheenHeight * 0.5f,
                kBossMirrorSheenDepth * 0.5f);
            sheenObj->SetRotation(0.0f, 0.0f, -0.18f);
            sheenObj->Update();
        }
        SetPatternObjectVisible(
            mBossMirrorSheenObjects[static_cast<size_t>(i)],
            !isOtherWorld && revealMirrorClones,
            { 0.96f, 0.98f, 1.0f, sheenAlpha });
    }

    if (mBossMirrorPatternState == BossMirrorPatternState::Summon)
    {
        mBossMirrorPatternTimer -= dt;

        SetBossLocomotionState(false);
        mBoss->GameObject::Update();

        if (mBossMirrorPatternTimer <= 0.0f)
        {
            mBossMirrorPatternState = BossMirrorPatternState::Dive;
            mBossMirrorPatternTimer = kBossMirrorDiveDuration;
            if (auto* animation = mBoss->GetSkeletalAnimation())
            {
                animation->Play("BossFly", kBossPatternAnimationBlendDuration, 1.0f, false);
            }
            mLastServerState = -1;
            OutputDebugStringA("[Stage2Boss][Pattern] Boss diving into center mirror\n");
        }

        return;
    }

    if (mBossMirrorPatternState == BossMirrorPatternState::Dive)
    {
        mBossMirrorPatternTimer -= dt;
        const float progress = 1.0f - (std::clamp)(mBossMirrorPatternTimer / kBossMirrorDiveDuration, 0.0f, 1.0f);
        const float posX = std::lerp(mBossMirrorDiveStart.x, mBossMirrorDiveTarget.x, progress);
        const float posZ = std::lerp(mBossMirrorDiveStart.z, mBossMirrorDiveTarget.z, progress);
        const float baseY = std::lerp(mBossMirrorDiveStart.y, mBossMirrorDiveTarget.y, progress);
        const float posY = baseY + std::sin(progress * DirectX::XM_PI) * kBossMirrorDiveArcHeight;

        mBoss->SetPosition(posX, posY, posZ);
        mBossFacingYaw = ComputeBossVisualYaw(
            ComputeFacingYaw({ posX, posY, posZ }, mBossMirrorDiveTarget));
        mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
        SetBossLocomotionState(false);
        mBoss->GameObject::Update();

        if (mBossMirrorPatternTimer <= 0.0f)
        {
            const DirectX::XMFLOAT3 hiddenPos = GetBossMirrorHiddenPosition(kBossMirrorCenterIndex);
            mBossMirrorSplitYaw = GetBossMirrorOutwardVisualYaw();

            mBoss->SetPosition(hiddenPos.x, hiddenPos.y, hiddenPos.z);
            mBossFacingYaw = mBossMirrorSplitYaw;
            mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
            mBoss->GameObject::Update();
            mBossMirrorPatternState = BossMirrorPatternState::Hidden;
            mBossMirrorPatternTimer = kBossMirrorHiddenDuration;
        }

        return;
    }

    if (mBossMirrorPatternState == BossMirrorPatternState::Hidden)
    {
        const DirectX::XMFLOAT3 hiddenPos = GetBossMirrorHiddenPosition(kBossMirrorCenterIndex);
        mBoss->SetPosition(hiddenPos.x, hiddenPos.y, hiddenPos.z);
        mBossFacingYaw = mBossMirrorSplitYaw;
        mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
        mBoss->GameObject::Update();

        mBossMirrorPatternTimer -= dt;
        if (mBossMirrorPatternTimer <= 0.0f)
        {
            mBossMirrorPatternState = BossMirrorPatternState::Split;
            mBossMirrorPatternTimer = 0.0f;
            mBossMirrorResolveHp = mBoss->GetHP();

            for (int i = 0; i < kBossMirrorSlotCount; ++i)
            {
                SetPatternObjectVisible(
                    mBossMirrorObjects[static_cast<size_t>(i)],
                    !isOtherWorld,
                    mirrorPulseTint);
                SetPatternObjectVisible(
                    mBossMirrorSheenObjects[static_cast<size_t>(i)],
                    !isOtherWorld,
                    kBossMirrorSheenTint);
            }

            for (int i = 0; i < kBossMirrorSlotCount; ++i)
            {
                SetPatternObjectVisible(
                    mBossMirrorCloneObjects[static_cast<size_t>(i)],
                    !isOtherWorld && i != mBossMirrorRealIndex,
                    kBossMirrorFakeCloneTint);
                ApplyMirrorPresentationRotation(mBossMirrorCloneObjects[static_cast<size_t>(i)]);
            }

            const DirectX::XMFLOAT3 realClonePos = GetBossMirrorClonePosition(mBossMirrorRealIndex);
            mBoss->SetPosition(realClonePos.x, realClonePos.y, realClonePos.z);
            mBossFacingYaw = mBossMirrorSplitYaw;
            mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
            mBoss->GameObject::Update();

            if (auto* animation = mBoss->GetSkeletalAnimation())
            {
                animation->Play("SkeletonIdle", kBossPatternAnimationBlendDuration);
            }

            OutputDebugStringA("[Stage2Boss][Pattern] Mirror clones spawned\n");
        }

        return;
    }

    if (mBossMirrorPatternState == BossMirrorPatternState::Split)
    {
        if (mBoss->GetHP() < mBossMirrorResolveHp - 0.01f)
        {
            EndBossMirrorPattern();
            return;
        }

        const DirectX::XMFLOAT3 realClonePos = GetBossMirrorClonePosition(mBossMirrorRealIndex);
        mBoss->SetPosition(realClonePos.x, realClonePos.y, realClonePos.z);
        mBossFacingYaw = mBossMirrorSplitYaw;
        mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
        SetBossLocomotionState(false);
        mBoss->GameObject::Update();

        for (int i = 0; i < kBossMirrorSlotCount; ++i)
        {
            ApplyMirrorPresentationRotation(mBossMirrorCloneObjects[static_cast<size_t>(i)]);
            SetPatternObjectVisible(
                mBossMirrorCloneObjects[static_cast<size_t>(i)],
                !isOtherWorld && i != mBossMirrorRealIndex,
                kBossMirrorFakeCloneTint);
        }
    }
}

void Stage2BossController::EndBossMirrorPattern()
{
    mBossMirrorPatternState = BossMirrorPatternState::Inactive;
    mBossMirrorPatternTimer = 0.0f;
    mBossMirrorResolveHp = 0.0f;
    mBossMirrorDiveStart = { 0.0f, 0.0f, 0.0f };
    mBossMirrorDiveTarget = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < kBossMirrorSlotCount; ++i)
    {
        SetPatternObjectVisible(mBossMirrorObjects[static_cast<size_t>(i)], false, kBossMirrorTint);
        SetPatternObjectVisible(mBossMirrorFrameTopObjects[static_cast<size_t>(i)], false, kBossMirrorFrameEdgeTint);
        SetPatternObjectVisible(mBossMirrorFrameBottomObjects[static_cast<size_t>(i)], false, kBossMirrorFrameTint);
        SetPatternObjectVisible(mBossMirrorFrameLeftObjects[static_cast<size_t>(i)], false, kBossMirrorFrameEdgeTint);
        SetPatternObjectVisible(mBossMirrorFrameRightObjects[static_cast<size_t>(i)], false, kBossMirrorFrameTint);
        SetPatternObjectVisible(mBossMirrorSheenObjects[static_cast<size_t>(i)], false, kBossMirrorSheenTint);
        SetPatternObjectVisible(mBossMirrorCloneObjects[static_cast<size_t>(i)], false, kBossMirrorFakeCloneTint);
    }

    if (mBoss != nullptr)
    {
        mBossMirrorSplitYaw = 0.0f;
        mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
        mBoss->ForceAnimationState(MonsterState::IDLE);
        mBoss->GameObject::Update();
    }

    mLastServerState = -1;

    ResetNormalBehavior();

    OutputDebugStringA("[Stage2Boss][Pattern] Mirror clone pattern resolved\n");
}

void Stage2BossController::StopBossPattern150Sound()
{
    if (mBossPattern150SoundHandle == AudioManager::InvalidClipHandle)
    {
        return;
    }

    AudioManager::Get().StopEffect(mBossPattern150SoundHandle);
    mBossPattern150SoundHandle = AudioManager::InvalidClipHandle;
}

void Stage2BossController::ResetNormalBehavior()
{
    mBossMoveState = BossMoveState::Idle;
    mBossActionTimer = 0.0f;
    mBossAttackRecoverDuration = kBossAttackRecoverDuration;
    mBossAttackAnimationDuration = 0.0f;
    mBossAttackNextHitIndex = 0;
}

void Stage2BossController::BeginBossAttack()
{
    SelectBossBasicAttack();
    if (!PlaySelectedBossBasicAttack())
    {
        ResetNormalBehavior();
        return;
    }

    mBossMoveState = BossMoveState::AttackWindup;
    mBossActionTimer = (std::max)(mBossAttackAnimationDuration, kBossAttackWindupDuration);
    mBossAttackNextHitIndex = 0;
}

void Stage2BossController::SelectBossBasicAttack()
{
    mBossAttackRandomState = mBossAttackRandomState * 1664525u + 1013904223u;
    const std::uint32_t roll = (mBossAttackRandomState >> 16u) % 100u;

    if (roll < 30u)
    {
        mBossBasicAttackType = BossBasicAttackType::TwoHitCombo;
    }
    else if (roll < 45u)
    {
        mBossBasicAttackType = BossBasicAttackType::ThreeHitCombo;
    }
    else if (roll < 75u)
    {
        mBossBasicAttackType = BossBasicAttackType::SwordAttack2;
    }
    else
    {
        mBossBasicAttackType = BossBasicAttackType::WhipAttack;
    }
}

bool Stage2BossController::PlaySelectedBossBasicAttack()
{
    if (mBoss == nullptr)
    {
        return false;
    }

    auto* animation = mBoss->GetSkeletalAnimation();
    if (animation == nullptr)
    {
        return false;
    }

    const BossAttackProfile& profile = GetSelectedBossAttackProfile();

    SetBossLocomotionState(false);
    if (!animation->Play(profile.ClipName, kBossAttackAnimationBlendDuration, 1.0f, false))
    {
        OutputDebugStringA(("[Stage2Boss][Attack] Failed to play " + std::string(profile.ClipName) + "\n").c_str());
        mBossAttackAnimationDuration = 0.0f;
        mBossAttackRecoverDuration = kBossAttackRecoverDuration;
        return false;
    }

    const float clipDuration = animation->GetClipDurationSeconds(profile.ClipName);
    mBossAttackAnimationDuration = clipDuration > 0.05f ? clipDuration : profile.FallbackDuration;
    mBossAttackRecoverDuration = kBossAttackRecoverDuration;

    std::ostringstream log;
    log << "[Stage2Boss][Attack] Selected " << profile.AttackLabel
        << " duration=" << mBossAttackAnimationDuration
        << " hitCount=" << profile.HitCount << "\n";
    OutputDebugStringA(log.str().c_str());

    switch (mBossBasicAttackType)
    {
    case BossBasicAttackType::WhipAttack:
        AudioManager::Get().PlayEffect(kBossWhipAttackSound, kBossWhipAttackVolume);
        break;

    case BossBasicAttackType::ThreeHitCombo:
    case BossBasicAttackType::SwordAttack2:
    case BossBasicAttackType::TwoHitCombo:
    default:
        AudioManager::Get().PlayEffect(kBossMeleeAttackSound, kBossMeleeAttackVolume);
        break;
    }

    return true;
}

const Stage2BossController::BossAttackProfile& Stage2BossController::GetSelectedBossAttackProfile() const
{
    static const BossAttackProfile kTwoHitComboProfile =
    {
        "Boss2HitCombo",
        "2HitCombo",
        1.35f,
        0.42f,
        2,
        {
            BossAttackHitBox{ 0.30f, { 0.00f, -0.90f, 2.10f }, { 1.15f, 1.00f, 1.55f } },
            BossAttackHitBox{ 0.58f, { 0.00f, -0.88f, 2.65f }, { 1.35f, 1.00f, 1.75f } },
            BossAttackHitBox{}
        }
    };
    static const BossAttackProfile kThreeHitComboProfile =
    {
        "Boss3HitCombo",
        "3HitCombo",
        1.75f,
        0.48f,
        3,
        {
            BossAttackHitBox{ 0.28f, { -0.15f, -0.92f, 2.00f }, { 1.10f, 1.00f, 1.45f } },
            BossAttackHitBox{ 0.50f, {  0.18f, -0.88f, 2.45f }, { 1.25f, 1.00f, 1.65f } },
            BossAttackHitBox{ 0.76f, {  0.00f, -0.82f, 3.40f }, { 2.40f, 1.05f, 2.10f } }
        }
    };
    static const BossAttackProfile kSwordAttack2Profile =
    {
        "BossSwordAttack2",
        "SwordAttack2",
        1.25f,
        0.36f,
        1,
        {
            BossAttackHitBox{ 0.56f, { 0.00f, -0.85f, 2.85f }, { 1.10f, 1.05f, 2.10f } },
            BossAttackHitBox{},
            BossAttackHitBox{}
        }
    };
    static const BossAttackProfile kWhipAttackProfile =
    {
        "BossWhipAttack",
        "WhipAttack",
        1.50f,
        0.34f,
        1,
        {
            BossAttackHitBox{ 0.62f, { 0.00f, -0.82f, 3.65f }, { 2.85f, 1.05f, 2.35f } },
            BossAttackHitBox{},
            BossAttackHitBox{}
        }
    };

    switch (mBossBasicAttackType)
    {
    case BossBasicAttackType::ThreeHitCombo:
        return kThreeHitComboProfile;
    case BossBasicAttackType::SwordAttack2:
        return kSwordAttack2Profile;
    case BossBasicAttackType::WhipAttack:
        return kWhipAttackProfile;
    case BossBasicAttackType::TwoHitCombo:
    default:
        return kTwoHitComboProfile;
    }
}

DirectX::XMFLOAT3 Stage2BossController::RotateBossAttackLocalOffset(const DirectX::XMFLOAT3& localOffset) const
{
    const float attackYaw = WrapAngle(mBossFacingYaw - DirectX::XM_PI);
    const float sinYaw = std::sin(attackYaw);
    const float cosYaw = std::cos(attackYaw);
    return
    {
        (localOffset.x * cosYaw) + (localOffset.z * sinYaw),
        localOffset.y,
        (localOffset.z * cosYaw) - (localOffset.x * sinYaw)
    };
}

bool Stage2BossController::DoesPlayerOverlapBossAttackHitBox(Player* player, const BossAttackHitBox& hitBox) const
{
    if (mBoss == nullptr || player == nullptr)
    {
        return false;
    }

    const DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();
    const DirectX::XMFLOAT3 playerPos = player->GetPosition();
    const DirectX::XMFLOAT3 delta =
    {
        playerPos.x - bossPos.x,
        playerPos.y - bossPos.y,
        playerPos.z - bossPos.z
    };

    const float attackYaw = WrapAngle(mBossFacingYaw - DirectX::XM_PI);
    const float sinYaw = std::sin(attackYaw);
    const float cosYaw = std::cos(attackYaw);
    const DirectX::XMFLOAT3 biasedLocalCenter =
    {
        hitBox.LocalCenter.x,
        hitBox.LocalCenter.y,
        hitBox.LocalCenter.z + kBossAttackHitBoxForwardBias
    };
    const DirectX::XMFLOAT3 localPlayer =
    {
        (delta.x * cosYaw) - (delta.z * sinYaw),
        delta.y,
        (delta.x * sinYaw) + (delta.z * cosYaw)
    };

    constexpr float kPlayerHalfWidth = Player::DefaultColliderHalfWidth;
    constexpr float kPlayerHalfHeight = Player::DefaultColliderHalfHeight;

    return
        std::fabs(localPlayer.x - biasedLocalCenter.x) <= (hitBox.Extents.x + kPlayerHalfWidth) &&
        std::fabs(localPlayer.y - biasedLocalCenter.y) <= (hitBox.Extents.y + kPlayerHalfHeight) &&
        std::fabs(localPlayer.z - biasedLocalCenter.z) <= (hitBox.Extents.z + kPlayerHalfWidth);
}

void Stage2BossController::UpdateBossAttackSequence(Player* player, float dt)
{
    if (mBoss == nullptr)
    {
        return;
    }

    const bool isPatternBlockingAttack =
        mBossMirrorPatternState != BossMirrorPatternState::Inactive ||
        IsBossScriptedAnimationActive() ||
        mBossPattern150DamagePending ||
        mBossWipeDamagePending ||
        mBossPatternRadiusTimer > 0.0f;
    if (isPatternBlockingAttack)
    {
        ResetNormalBehavior();
        return;
    }

    if (mBossMoveState == BossMoveState::AttackRecover)
    {
        mBossActionTimer -= dt;
        SetBossLocomotionState(false);
        if (mBossActionTimer <= 0.0f)
        {
            mBossMoveState = BossMoveState::Idle;
            mBossActionTimer = 0.0f;
        }
        return;
    }

    if (mBossMoveState != BossMoveState::AttackWindup)
    {
        return;
    }

    const BossAttackProfile& profile = GetSelectedBossAttackProfile();
    auto* animation = mBoss->GetSkeletalAnimation();
    const float animationProgress =
        animation != nullptr ? animation->GetCurrentAnimationProgress() : 0.0f;

    if (player != nullptr && animationProgress < profile.StepMoveEndProgress)
    {
        const DirectX::XMFLOAT3 playerPos = player->GetPosition();
        const DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();
        MoveBoss(
            {
                playerPos.x - bossPos.x,
                0.0f,
                playerPos.z - bossPos.z
            },
            kBossAttackStepSpeed,
            dt);
    }

    while (mBossAttackNextHitIndex < profile.HitCount &&
        animationProgress >= profile.HitBoxes[mBossAttackNextHitIndex].TriggerProgress)
    {
        if (!NetworkManager::Get()->IsConnected() &&
            DoesPlayerOverlapBossAttackHitBox(player, profile.HitBoxes[mBossAttackNextHitIndex]))
        {
            player->OnDamaged(kBossAttackDamage);
        }

        ++mBossAttackNextHitIndex;
    }

    mBossActionTimer -= dt;
    if (animationProgress >= 0.995f || mBossActionTimer <= 0.0f)
    {
        mBossMoveState = BossMoveState::AttackRecover;
        mBossActionTimer = mBossAttackRecoverDuration;
        mBossAttackCooldownTimer = kBossAttackCooldown;
    }

    SetBossLocomotionState(false);
}

void Stage2BossController::UpdateBossAttackDebugVisualizer(bool isOtherWorld)
{
    std::vector<DebugColliderVisualizer::Target> targets;

    if (!kShowBossAttackDebugHitBoxes)
    {
        mBossAttackDebugVisualizer.Update(mGame, targets, mTrackOwned);
        return;
    }

    const bool shouldShow =
        !isOtherWorld &&
        mBoss != nullptr &&
        mBossMoveState == BossMoveState::AttackWindup &&
        mBoss->GetState() != MonsterState::DIE &&
        mBoss->GetState() != MonsterState::DYING;
    if (!shouldShow)
    {
        mBossAttackDebugVisualizer.Update(mGame, targets, mTrackOwned);
        return;
    }

    const BossAttackProfile& profile = GetSelectedBossAttackProfile();
    const DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();
    targets.reserve(profile.HitCount);

    for (std::size_t i = 0; i < profile.HitCount; ++i)
    {
        const BossAttackHitBox& hitBox = profile.HitBoxes[i];
        const DirectX::XMFLOAT3 biasedLocalCenter =
        {
            hitBox.LocalCenter.x,
            hitBox.LocalCenter.y,
            hitBox.LocalCenter.z + kBossAttackHitBoxForwardBias
        };
        const DirectX::XMFLOAT3 worldOffset = RotateBossAttackLocalOffset(biasedLocalCenter);
        DebugColliderVisualizer::Target target;
        target.Center =
        {
            bossPos.x + worldOffset.x,
            bossPos.y + worldOffset.y,
            bossPos.z + worldOffset.z
        };
        target.Extents = hitBox.Extents;
        target.Rotation = { 0.0f, WrapAngle(mBossFacingYaw - DirectX::XM_PI), 0.0f };
        target.MaterialName =
            (i < mBossAttackNextHitIndex) ? "Stage2BossAttackSpentMat" :
            (i == mBossAttackNextHitIndex) ? "Stage2BossAttackActiveMat" :
            "Stage2BossAttackPendingMat";
        target.Color =
            (i < mBossAttackNextHitIndex) ? DirectX::XMFLOAT4{ 0.55f, 0.18f, 0.18f, 0.12f } :
            (i == mBossAttackNextHitIndex) ? DirectX::XMFLOAT4{ 1.0f, 0.08f, 0.08f, 0.34f } :
            DirectX::XMFLOAT4{ 0.95f, 0.45f, 0.12f, 0.20f };
        target.Visible = true;
        targets.push_back(target);
    }

    mBossAttackDebugVisualizer.Update(mGame, targets, mTrackOwned);
}

bool Stage2BossController::PlayBossScriptedAnimation(
    BossScriptedAnimationState state,
    const char* clipName,
    float fallbackDuration,
    float blendDuration)
{
    if (mBoss == nullptr || clipName == nullptr || state == BossScriptedAnimationState::None)
    {
        return false;
    }

    auto* animation = mBoss->GetSkeletalAnimation();
    if (animation == nullptr)
    {
        return false;
    }

    if (state != BossScriptedAnimationState::Pattern150Roar)
    {
        StopBossPattern150Sound();
    }

    ResetNormalBehavior();
    SetBossLocomotionState(false);
    if (!animation->Play(clipName, blendDuration, 1.0f, false))
    {
        OutputDebugStringA(("[Stage2Boss][Pattern] Failed to play " + std::string(clipName) + "\n").c_str());
        return false;
    }

    const float clipDuration = animation->GetClipDurationSeconds(clipName);
    mBossScriptedAnimationState = state;
    mBossScriptedAnimationTimer = clipDuration > 0.05f ? clipDuration : fallbackDuration;
    mLastServerState = -1;

    switch (state)
    {
    case BossScriptedAnimationState::SpawnSummon:
        AudioManager::Get().PlayEffect(kBossIntroRoarSound, kBossIntroRoarVolume);
        break;

    case BossScriptedAnimationState::Pattern150Roar:
        StopBossPattern150Sound();
        mBossPattern150SoundHandle = AudioManager::Get().PlayEffect(kBossRoar150Sound, kBossRoar150Volume);
        break;

    case BossScriptedAnimationState::WipeSwordAttack:
        AudioManager::Get().PlayEffect(kBossMeleeAttackSound, kBossMeleeAttackVolume);
        break;

    case BossScriptedAnimationState::WipeSummon:
    case BossScriptedAnimationState::None:
    default:
        break;
    }

    return true;
}

void Stage2BossController::UpdateBossScriptedAnimation(float dt)
{
    if (!IsBossScriptedAnimationActive())
    {
        return;
    }

    if (mBoss == nullptr ||
        mBoss->GetState() == MonsterState::DYING ||
        mBoss->GetState() == MonsterState::DIE)
    {
        StopBossPattern150Sound();
        mBossScriptedAnimationState = BossScriptedAnimationState::None;
        mBossScriptedAnimationTimer = 0.0f;
        return;
    }

    mBossScriptedAnimationTimer -= dt;
    if (mBossScriptedAnimationTimer > 0.0f)
    {
        return;
    }

    if (mBossScriptedAnimationState == BossScriptedAnimationState::WipeSummon)
    {
        PlayBossScriptedAnimation(
            BossScriptedAnimationState::WipeSwordAttack,
            "BossSwordAttack1",
            kBossSwordAttackFallbackDuration,
            kBossWipeAttackBlendDuration);
        return;
    }

    if (mBossScriptedAnimationState == BossScriptedAnimationState::Pattern150Roar)
    {
        StopBossPattern150Sound();
    }

    mBossScriptedAnimationState = BossScriptedAnimationState::None;
    mBossScriptedAnimationTimer = 0.0f;
    mLastServerState = -1;
    mBoss->ForceAnimationState(MonsterState::IDLE);
}

bool Stage2BossController::IsBossScriptedAnimationActive() const
{
    return mBossScriptedAnimationState != BossScriptedAnimationState::None;
}

void Stage2BossController::SetBossLocomotionState(bool isMoving)
{
    if (mBoss == nullptr)
    {
        return;
    }

    const MonsterState desiredState = isMoving ? MonsterState::TRACE : MonsterState::IDLE;
    if (mBoss->GetState() == desiredState)
    {
        return;
    }

    mBoss->ForceAnimationState(desiredState);

    if (isMoving)
    {
        if (auto* animation = mBoss->GetSkeletalAnimation())
        {
            if (!animation->Play("SkeletonWalk", 0.14f, 1.0f))
            {
                animation->Play("SkeletonIdle", 0.14f, 1.08f);
            }
        }
    }
}

void Stage2BossController::UpdateBossAnimationDebugInput()
{
    if (mGame == nullptr || mBoss == nullptr || GetForegroundWindow() != mGame->GetMainWindowHandle())
    {
        return;
    }

    const bool previousDown = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    const bool nextDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    const bool replayDown = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    const bool exitDown = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;

    if (previousDown && !mBossAnimationDebugPreviousKeyPressed)
    {
        if (mBossAnimationDebugClipIndex >= kBossAnimationClips.size())
        {
            mBossAnimationDebugClipIndex = kBossAnimationClips.size() - 1;
        }
        else
        {
            mBossAnimationDebugClipIndex =
                (mBossAnimationDebugClipIndex + kBossAnimationClips.size() - 1) % kBossAnimationClips.size();
        }

        PlayBossDebugAnimation(mBossAnimationDebugClipIndex);
    }

    if (nextDown && !mBossAnimationDebugNextKeyPressed)
    {
        if (mBossAnimationDebugClipIndex >= kBossAnimationClips.size())
        {
            mBossAnimationDebugClipIndex = 0;
        }
        else
        {
            mBossAnimationDebugClipIndex =
                (mBossAnimationDebugClipIndex + 1) % kBossAnimationClips.size();
        }

        PlayBossDebugAnimation(mBossAnimationDebugClipIndex);
    }

    if (replayDown && !mBossAnimationDebugReplayKeyPressed)
    {
        const std::size_t replayIndex = mBossAnimationDebugClipIndex < kBossAnimationClips.size()
            ? mBossAnimationDebugClipIndex
            : 0;
        PlayBossDebugAnimation(replayIndex);
    }

    if (exitDown && !mBossAnimationDebugExitKeyPressed)
    {
        StopBossAnimationDebug();
    }

    mBossAnimationDebugPreviousKeyPressed = previousDown;
    mBossAnimationDebugNextKeyPressed = nextDown;
    mBossAnimationDebugReplayKeyPressed = replayDown;
    mBossAnimationDebugExitKeyPressed = exitDown;
}

bool Stage2BossController::PlayBossDebugAnimation(std::size_t clipIndex)
{
    if (mBoss == nullptr || clipIndex >= kBossAnimationClips.size())
    {
        return false;
    }

    auto* animation = mBoss->GetSkeletalAnimation();
    if (animation == nullptr)
    {
        return false;
    }

    mBoss->ForceAnimationState(MonsterState::IDLE);
    const BossAnimationClipSpec& clip = kBossAnimationClips[clipIndex];
    if (!animation->Play(clip.ClipName, 0.08f, 1.0f))
    {
        OutputDebugStringA(("[Stage2Boss][AnimationDebug] Failed to play " +
            std::string(clip.ClipName) + "\n").c_str());
        return false;
    }

    mBossAnimationDebugActive = true;
    mBossAnimationDebugClipIndex = clipIndex;
    ResetNormalBehavior();

    std::ostringstream log;
    log << "[Stage2Boss][AnimationDebug] "
        << (clipIndex + 1) << "/" << kBossAnimationClips.size()
        << " clip=" << clip.ClipName
        << " file=" << clip.FilePath << "\n";
    OutputDebugStringA(log.str().c_str());
    return true;
}

void Stage2BossController::StopBossAnimationDebug()
{
    if (!mBossAnimationDebugActive)
    {
        return;
    }

    mBossAnimationDebugActive = false;
    mBossAnimationDebugClipIndex = static_cast<std::size_t>(-1);
    ResetNormalBehavior();
    if (mBoss != nullptr)
    {
        mBoss->ForceAnimationState(MonsterState::IDLE);
    }
    OutputDebugStringA("[Stage2Boss][AnimationDebug] stopped; restored SkeletonIdle\n");
}

void Stage2BossController::FaceTowards(const DirectX::XMFLOAT3& targetPosition, float dt)
{
    if (mBoss == nullptr)
    {
        return;
    }

    const DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();
    const float dx = targetPosition.x - bossPos.x;
    const float dz = targetPosition.z - bossPos.z;
    if ((dx * dx + dz * dz) <= 0.0001f)
    {
        return;
    }

    const float targetYaw = ComputeBossVisualYaw(std::atan2(dx, dz));
    const float deltaYaw = WrapAngle(targetYaw - mBossFacingYaw);
    const float maxStep = kBossTurnSpeed * dt;
    const float clampedStep = (std::clamp)(deltaYaw, -maxStep, maxStep);
    mBossFacingYaw = WrapAngle(mBossFacingYaw + clampedStep);
    mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
}

bool Stage2BossController::MoveBoss(const DirectX::XMFLOAT3& moveDirection, float speed, float dt)
{
    if (mBoss == nullptr || speed <= 0.0f || dt <= 0.0f)
    {
        return false;
    }

    const float lenSq = moveDirection.x * moveDirection.x + moveDirection.z * moveDirection.z;
    if (lenSq <= 0.0001f)
    {
        return false;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    const float dirX = moveDirection.x * invLen;
    const float dirZ = moveDirection.z * invLen;

    DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();
    float moveX = dirX * speed * dt;
    float moveZ = dirZ * speed * dt;

    if (mMapSystem != nullptr)
    {
        const float feetPos = bossPos.y - mBoss->GetColliderHalfHeight();
        if (mMapSystem->CheckWall(bossPos.x, bossPos.z, feetPos, dirX, 0.0f))
        {
            moveX = 0.0f;
        }

        if (mMapSystem->CheckWall(bossPos.x, bossPos.z, feetPos, 0.0f, dirZ))
        {
            moveZ = 0.0f;
        }
    }

    if (moveX == 0.0f && moveZ == 0.0f)
    {
        return false;
    }

    DirectX::XMFLOAT3 nextPos = bossPos;
    nextPos.x += moveX;
    nextPos.z += moveZ;

    const float leashDx = nextPos.x - kStage2BossAnchorPosition.x;
    const float leashDz = nextPos.z - kStage2BossAnchorPosition.z;
    const float leashDistanceSq = leashDx * leashDx + leashDz * leashDz;
    if (leashDistanceSq > (kBossLeashRadius * kBossLeashRadius))
    {
        const float leashDistance = std::sqrt(leashDistanceSq);
        const float scale = kBossLeashRadius / leashDistance;
        nextPos.x = kStage2BossAnchorPosition.x + leashDx * scale;
        nextPos.z = kStage2BossAnchorPosition.z + leashDz * scale;
    }

    if (mMapSystem != nullptr)
    {
        const float halfHeight = mBoss->GetColliderHalfHeight();
        const float nextFeetPos = nextPos.y - halfHeight;
        const float rayStartY = nextFeetPos + 1.0f;
        const float floorY = mMapSystem->GetFloorHeight(nextPos.x, nextPos.z, rayStartY, 1000.0f);

        if (floorY < -8000.0f)
        {
            return false;
        }

        if (nextFeetPos < floorY || (nextFeetPos - floorY) <= 0.5f)
        {
            nextPos.y = floorY + halfHeight;
        }
        else
        {
            return false;
        }
    }

    mBoss->SetPosition(nextPos.x, nextPos.y, nextPos.z);
    return true;
}

void Stage2BossController::UpdateNormalBehavior(Player* player, bool isOtherWorld, float dt)
{
    if (mBoss == nullptr)
    {
        return;
    }

    if (player == nullptr || player->IsDead() || isOtherWorld || IsInvulnerable())
    {
        ResetNormalBehavior();
        SetBossLocomotionState(false);
        return;
    }

    const DirectX::XMFLOAT3 playerPos = player->GetPosition();
    const DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();

    const float playerAnchorDx = playerPos.x - kStage2BossAnchorPosition.x;
    const float playerAnchorDz = playerPos.z - kStage2BossAnchorPosition.z;
    const float playerAnchorDistanceSq = playerAnchorDx * playerAnchorDx + playerAnchorDz * playerAnchorDz;
    if (playerAnchorDistanceSq > (kBossEngageRadius * kBossEngageRadius))
    {
        FaceTowards(kStage2BossAnchorPosition, dt);
        const bool movedToAnchor = MoveBoss(
            {
                kStage2BossAnchorPosition.x - bossPos.x,
                0.0f,
                kStage2BossAnchorPosition.z - bossPos.z
            },
            kBossMoveSpeed * 0.85f,
            dt);
        mBossMoveState = movedToAnchor ? BossMoveState::Chase : BossMoveState::Idle;
        SetBossLocomotionState(movedToAnchor);
        return;
    }

    const float dx = playerPos.x - bossPos.x;
    const float dz = playerPos.z - bossPos.z;
    const float distanceSq = dx * dx + dz * dz;
    const float distance = std::sqrt((std::max)(distanceSq, 0.0001f));

    if (mBossMoveState == BossMoveState::AttackWindup)
    {
        SetBossLocomotionState(false);
        return;
    }

    if (mBossMoveState == BossMoveState::AttackRecover)
    {
        mBossActionTimer -= dt;
        SetBossLocomotionState(false);
        if (mBossActionTimer <= 0.0f)
        {
            mBossMoveState = BossMoveState::Idle;
            mBossActionTimer = 0.0f;
        }
        return;
    }

    FaceTowards(playerPos, dt);

    if (distance <= kBossAttackDistance && mBossAttackCooldownTimer <= 0.0f)
    {
        BeginBossAttack();
        return;
    }

    if (distance > kBossPreferredMaxDistance)
    {
        mBossMoveState = BossMoveState::Chase;
        const bool moved = MoveBoss({ dx, 0.0f, dz }, kBossMoveSpeed, dt);
        SetBossLocomotionState(moved);
        return;
    }

    if (mBossMoveState != BossMoveState::Strafe || mBossActionTimer <= 0.0f)
    {
        mBossMoveState = BossMoveState::Strafe;
        mBossActionTimer = kBossStrafeDuration;
        mBossStrafeDirection = (mBossStrafeDirection >= 0.0f) ? -1.0f : 1.0f;
    }

    mBossActionTimer -= dt;
    const float retreatWeight = distance < kBossPreferredMinDistance ? 0.4f : 0.0f;
    const DirectX::XMFLOAT3 strafeMove =
    {
        (-dz * mBossStrafeDirection) - (dx * retreatWeight),
        0.0f,
        (dx * mBossStrafeDirection) - (dz * retreatWeight)
    };
    const bool moved = MoveBoss(strafeMove, kBossStrafeSpeed, dt);
    SetBossLocomotionState(moved);
}

void Stage2BossController::BuildBoss()
{
    auto* res = mGame != nullptr ? mGame->GetResources() : nullptr;
    auto* device = mGame != nullptr ? mGame->GetDevice() : nullptr;
    auto* cmdList = mGame != nullptr ? mGame->GetCommandList() : nullptr;
    if (res == nullptr || device == nullptr || cmdList == nullptr)
    {
        return;
    }

    auto bossRitem = std::make_unique<RenderItem>();
    bossRitem->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());

    auto boss = std::make_unique<Monster>(MonsterType::STAGE2_BOSS);
    boss->SetNetworkId(STAGE2_BOSS_MONSTER_ID);
    boss->Initialize(bossRitem.get(), kStage2BossSpawnPosition);

    CharacterVisualSpec visualSpec;
    visualSpec.UseSkinned = true;
    visualSpec.ModelPath = GetBossModelPath();
    visualSpec.DefaultClipName = "SkeletonIdle";
    visualSpec.LoadModelAnimations = false;
    AddBossAnimationClips(visualSpec);
    visualSpec.GeometryName = "stage2BossDemonLordGeo";
    visualSpec.MaterialName = "Stage2BossMat";
    visualSpec.DiffuseTextureName = "Stage2BossDemonLordBaseColor";
    visualSpec.DiffuseTexturePath = L"Textures/T_DemonLordBody_BaseColor.dds";
    visualSpec.EmissiveTextureName = "Stage2BossDemonLordEmissive";
    visualSpec.EmissiveTexturePath = L"Textures/T_DemonLordBody_Emissive.dds";
    visualSpec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    visualSpec.FresnelR0 = { 0.08f, 0.04f, 0.03f };
    visualSpec.Roughness = 0.62f;
    visualSpec.IsToon = false;
    visualSpec.OutlineThickness = 0.0f;
    visualSpec.OutlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    visualSpec.FallbackMaterialName = "MonsterRed";
    visualSpec.FallbackScale = { 0.72f, 1.85f, 0.72f };
    visualSpec.SpawnPosition = kStage2BossSpawnPosition;
    visualSpec.UseActorOrigin = true;
    visualSpec.CenterBoundsXZ = false;
    visualSpec.OriginToFloor = boss->GetColliderHalfHeight() + 0.6f;
    visualSpec.RotationOffset = { 0.0f, DirectX::XM_PI, 0.0f };
    visualSpec.TargetHeight = boss->GetColliderHalfHeight() * 1.75f;

    if (!CharacterVisualFactory::ApplyVisual(
        boss.get(),
        bossRitem.get(),
        device,
        cmdList,
        res,
        visualSpec))
    {
        OutputDebugStringA("[Stage2Boss] Failed to build boss visual\n");
        return;
    }

    const float dx = kStage2PlayerStartPosition.x - kStage2BossAnchorPosition.x;
    const float dz = kStage2PlayerStartPosition.z - kStage2BossAnchorPosition.z;
    mBossFacingYaw = ComputeBossVisualYaw(std::atan2(dx, dz));
    boss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
    boss->GameObject::Update();

    mBoss = boss.get();
    PlayBossScriptedAnimation(
        BossScriptedAnimationState::SpawnSummon,
        "BossSummonSwordWhip",
        kBossSummonFallbackDuration,
        0.0f);
    TrackOwned(mBoss, bossRitem.get());
    mGame->GetRitems().push_back(std::move(bossRitem));
    mGame->GetGameObjects().push_back(std::move(boss));

    OutputDebugStringA("[Stage2Boss] Demon Lord boss spawned with summon animation\n");
}

void Stage2BossController::BuildBossPatternIndicator()
{
    auto* res = mGame != nullptr ? mGame->GetResources() : nullptr;
    auto* device = mGame != nullptr ? mGame->GetDevice() : nullptr;
    auto* cmdList = mGame != nullptr ? mGame->GetCommandList() : nullptr;
    if (res == nullptr || device == nullptr || cmdList == nullptr)
    {
        return;
    }

    constexpr const char* kIndicatorGeoName = "stage2BossPatternDiskGeo";
    constexpr const char* kIndicatorSubmeshName = "disk";
    constexpr const char* kIndicatorRingGeoName = "stage2BossPatternRingGeo";
    constexpr const char* kIndicatorRingSubmeshName = "ring";
    constexpr const char* kIndicatorMatName = "Stage2BossPatternRadiusMat";
    constexpr float kIndicatorRingInnerRadius = 0.994f;

    if (res->mGeometries.find(kIndicatorGeoName) == res->mGeometries.end())
    {
        constexpr int segmentCount = 128;
        std::vector<Vertex> vertices;
        std::vector<std::uint16_t> indices;
        vertices.reserve(segmentCount + 2);
        indices.reserve(segmentCount * 6);

        vertices.push_back(Vertex({ DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.5f, 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
        for (int i = 0; i <= segmentCount; ++i)
        {
            const float angle = DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(segmentCount);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            vertices.push_back(Vertex({ DirectX::XMFLOAT3(c, 0.0f, s), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
        }

        for (int i = 1; i <= segmentCount; ++i)
        {
            indices.insert(indices.end(),
                {
                    0,
                    static_cast<std::uint16_t>(i + 1),
                    static_cast<std::uint16_t>(i),
                    0,
                    static_cast<std::uint16_t>(i),
                    static_cast<std::uint16_t>(i + 1)
                });
        }

        const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

        auto geometry = std::make_unique<MeshGeometry>();
        geometry->Name = kIndicatorGeoName;

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
        CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
        ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
        CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

        geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, vertices.data(), vbByteSize, geometry->VertexBufferUploader);
        geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, indices.data(), ibByteSize, geometry->IndexBufferUploader);
        geometry->VertexByteStride = sizeof(Vertex);
        geometry->VertexBufferByteSize = vbByteSize;
        geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
        geometry->IndexBufferByteSize = ibByteSize;

        SubmeshGeometry submesh;
        submesh.IndexCount = static_cast<UINT>(indices.size());
        submesh.StartIndexLocation = 0;
        submesh.BaseVertexLocation = 0;
        geometry->DrawArgs[kIndicatorSubmeshName] = submesh;

        res->mGeometries[kIndicatorGeoName] = std::move(geometry);
    }

    if (res->mGeometries.find(kIndicatorRingGeoName) == res->mGeometries.end())
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint16_t> indices;
        vertices.reserve((kBossPatternRadiusRingSegmentCount + 1) * 2);
        indices.reserve(kBossPatternRadiusRingSegmentCount * 12);

        for (int i = 0; i <= kBossPatternRadiusRingSegmentCount; ++i)
        {
            const float angle = DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(kBossPatternRadiusRingSegmentCount);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            vertices.push_back(Vertex({ DirectX::XMFLOAT3(c, 0.0f, s), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
            vertices.push_back(Vertex({ DirectX::XMFLOAT3(c * kIndicatorRingInnerRadius, 0.0f, s * kIndicatorRingInnerRadius), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.5f + c * 0.5f * kIndicatorRingInnerRadius, 0.5f - s * 0.5f * kIndicatorRingInnerRadius), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
        }

        for (int i = 0; i < kBossPatternRadiusRingSegmentCount; ++i)
        {
            const std::uint16_t outer0 = static_cast<std::uint16_t>(i * 2);
            const std::uint16_t inner0 = static_cast<std::uint16_t>(outer0 + 1);
            const std::uint16_t outer1 = static_cast<std::uint16_t>(outer0 + 2);
            const std::uint16_t inner1 = static_cast<std::uint16_t>(outer0 + 3);

            indices.insert(indices.end(),
                {
                    outer0, outer1, inner0,
                    inner0, outer1, inner1,
                    outer0, inner0, outer1,
                    inner0, inner1, outer1
                });
        }

        const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

        auto geometry = std::make_unique<MeshGeometry>();
        geometry->Name = kIndicatorRingGeoName;

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
        CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
        ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
        CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

        geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, vertices.data(), vbByteSize, geometry->VertexBufferUploader);
        geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, indices.data(), ibByteSize, geometry->IndexBufferUploader);
        geometry->VertexByteStride = sizeof(Vertex);
        geometry->VertexBufferByteSize = vbByteSize;
        geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
        geometry->IndexBufferByteSize = ibByteSize;

        SubmeshGeometry submesh;
        submesh.IndexCount = static_cast<UINT>(indices.size());
        submesh.StartIndexLocation = 0;
        submesh.BaseVertexLocation = 0;
        geometry->DrawArgs[kIndicatorRingSubmeshName] = submesh;

        res->mGeometries[kIndicatorRingGeoName] = std::move(geometry);
    }

    if (res->GetMaterial(kIndicatorMatName) == nullptr)
    {
        res->CreateMaterial(
            kIndicatorMatName,
            static_cast<int>(res->mMaterials.size()),
            "white",
            "",
            "",
            "",
            { 1.0f, 0.0f, 0.0f, 0.95f },
            { 0.12f, 0.02f, 0.02f },
            0.18f);
    }

    if (Material* material = res->GetMaterial(kIndicatorMatName))
    {
        material->DiffuseAlbedo = { 1.0f, 0.0f, 0.0f, 0.95f };
        material->FresnelR0 = { 0.12f, 0.02f, 0.02f };
        material->Roughness = 0.18f;
        material->IsTransparent = 1;
        material->IsToon = 0;
        material->NumFramesDirty = gNumFrameResources;
    }

    auto renderItem = std::make_unique<RenderItem>();
    renderItem->World = MathHelper::Identity4x4();
    renderItem->TexTransform = MathHelper::Identity4x4();
    renderItem->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());
    renderItem->Geo = res->mGeometries[kIndicatorGeoName].get();
    renderItem->Mat = res->GetMaterial(kIndicatorMatName);
    renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->IndexCount = renderItem->Geo->DrawArgs[kIndicatorSubmeshName].IndexCount;
    renderItem->StartIndexLocation = renderItem->Geo->DrawArgs[kIndicatorSubmeshName].StartIndexLocation;
    renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs[kIndicatorSubmeshName].BaseVertexLocation;
    renderItem->Visible = false;
    renderItem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };

    auto object = std::make_unique<GameObject>();
    object->Ritem = renderItem.get();
    object->SetScale(kBossPattern150Radius, 1.0f, kBossPattern150Radius);
    object->SetPosition(kStage2BossAnchorPosition.x, kStage2BossAnchorPosition.y + 0.06f, kStage2BossAnchorPosition.z);
    object->Update();

    mBossPatternRadiusObj = object.get();
    TrackOwned(object.get(), renderItem.get());
    mGame->GetRitems().push_back(std::move(renderItem));
    mGame->GetGameObjects().push_back(std::move(object));

    auto ringRitem = std::make_unique<RenderItem>();
    ringRitem->World = MathHelper::Identity4x4();
    ringRitem->TexTransform = MathHelper::Identity4x4();
    ringRitem->ObjCBIndex = static_cast<UINT>(mGame->GetRitems().size());
    ringRitem->Geo = res->mGeometries[kIndicatorRingGeoName].get();
    ringRitem->Mat = res->GetMaterial(kIndicatorMatName);
    ringRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ringRitem->IndexCount = ringRitem->Geo->DrawArgs[kIndicatorRingSubmeshName].IndexCount;
    ringRitem->StartIndexLocation = ringRitem->Geo->DrawArgs[kIndicatorRingSubmeshName].StartIndexLocation;
    ringRitem->BaseVertexLocation = ringRitem->Geo->DrawArgs[kIndicatorRingSubmeshName].BaseVertexLocation;
    ringRitem->Visible = false;
    ringRitem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };

    auto ringObj = std::make_unique<GameObject>();
    ringObj->Ritem = ringRitem.get();
    ringObj->SetScale(kBossPattern150Radius * 0.985f, 1.0f, kBossPattern150Radius * 0.985f);
    ringObj->SetPosition(kStage2BossAnchorPosition.x, kStage2BossAnchorPosition.y + 0.22f, kStage2BossAnchorPosition.z);
    ringObj->Update();

    mBossPatternRadiusRingObj = ringObj.get();
    TrackOwned(ringObj.get(), ringRitem.get());
    mGame->GetRitems().push_back(std::move(ringRitem));
    mGame->GetGameObjects().push_back(std::move(ringObj));
}

void Stage2BossController::ShowBossPatternRadiusIndicator(const DirectX::XMFLOAT3& center, float radius, float duration)
{
    if (mBossPatternRadiusObj == nullptr || mBossPatternRadiusObj->Ritem == nullptr)
    {
        return;
    }

    float floorY = kStage2BossAnchorPosition.y;
    if (mMapSystem != nullptr)
    {
        const float sampledFloorY = mMapSystem->GetFloorHeight(center.x, center.z, center.y + 5.0f, 20.0f);
        if (sampledFloorY > -9000.0f)
        {
            floorY = sampledFloorY;
        }
    }

    mBossPatternRadiusTimer = duration;
    mBossPatternRadiusDuration = duration;
    mBossPatternRadiusObj->SetScale(radius, 1.0f, radius);
    mBossPatternRadiusObj->SetPosition(center.x, floorY + 0.14f, center.z);
    mBossPatternRadiusObj->Update();
    mBossPatternRadiusObj->Ritem->Visible = true;
    mBossPatternRadiusObj->Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
    mBossPatternRadiusObj->Ritem->NumFramesDirty = gNumFrameResources;

    if (mBossPatternRadiusRingObj != nullptr && mBossPatternRadiusRingObj->Ritem != nullptr)
    {
        const float ringScale = radius * 0.985f;
        mBossPatternRadiusRingObj->SetScale(ringScale, 1.0f, ringScale);
        mBossPatternRadiusRingObj->SetRotation(0.0f, 0.0f, 0.0f);
        mBossPatternRadiusRingObj->SetPosition(center.x, floorY + 0.23f, center.z);
        mBossPatternRadiusRingObj->Update();
        mBossPatternRadiusRingObj->Ritem->Visible = true;
        mBossPatternRadiusRingObj->Ritem->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
        mBossPatternRadiusRingObj->Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void Stage2BossController::UpdateBossPatternIndicator(float dt)
{
    if (mBossPatternRadiusObj == nullptr || mBossPatternRadiusObj->Ritem == nullptr)
    {
        return;
    }

    if (mBossPatternRadiusTimer <= 0.0f)
    {
        mBossPatternRadiusObj->Ritem->Visible = false;
        if (mBossPatternRadiusRingObj != nullptr && mBossPatternRadiusRingObj->Ritem != nullptr)
        {
            mBossPatternRadiusRingObj->Ritem->Visible = false;
        }
        return;
    }

    mBossPatternRadiusTimer -= dt;
    if (mBossPatternRadiusTimer <= 0.0f)
    {
        mBossPatternRadiusTimer = 0.0f;
        mBossPatternRadiusObj->Ritem->Visible = false;
        mBossPatternRadiusObj->Ritem->NumFramesDirty = gNumFrameResources;
        if (mBossPatternRadiusRingObj != nullptr && mBossPatternRadiusRingObj->Ritem != nullptr)
        {
            mBossPatternRadiusRingObj->Ritem->Visible = false;
            mBossPatternRadiusRingObj->Ritem->NumFramesDirty = gNumFrameResources;
        }
        return;
    }

    const float normalizedTime =
        mBossPatternRadiusDuration > 0.0f ? mBossPatternRadiusTimer / mBossPatternRadiusDuration : 0.0f;
    const float pulse = 0.82f + std::sin((1.0f - normalizedTime) * DirectX::XM_2PI * 3.0f) * 0.18f;
    const float alpha = 1.0f * (std::clamp)(normalizedTime + 0.18f, 0.0f, 1.0f);
    mBossPatternRadiusObj->Ritem->Visible = true;
    mBossPatternRadiusObj->Ritem->ColorMultiplier = { pulse, 1.0f, 1.0f, alpha };
    mBossPatternRadiusObj->Ritem->NumFramesDirty = gNumFrameResources;

    if (mBossPatternRadiusRingObj != nullptr && mBossPatternRadiusRingObj->Ritem != nullptr)
    {
        mBossPatternRadiusRingObj->Ritem->Visible = true;
        mBossPatternRadiusRingObj->Ritem->ColorMultiplier = { 1.0f, 0.82f + pulse * 0.18f, 0.82f + pulse * 0.18f, 1.0f };
        mBossPatternRadiusRingObj->Ritem->NumFramesDirty = gNumFrameResources;
    }
}

void Stage2BossController::UpdateBossPattern150Damage(Player* player, float dt)
{
    if (!mBossPattern150DamagePending)
    {
        return;
    }

    mBossPattern150DamageTimer -= dt;
    if (mBossPattern150DamageTimer > 0.0f)
    {
        return;
    }

    mBossPattern150DamagePending = false;
    mBossPattern150DamageTimer = 0.0f;
    ApplyBossPattern150Damage(player);
}

void Stage2BossController::ApplyBossPattern150Damage(Player* player)
{
    OutputDebugStringA("[Stage2Boss][Pattern] 150-layer shockwave damage applied\n");

    if (mBoss != nullptr && mBoss->GetState() != MonsterState::DIE)
    {
        mBoss->ForceAnimationState(MonsterState::DAMAGED);
    }

    (void)player;
}

void Stage2BossController::UpdateBossWipeDamage(Player* player, bool isOtherWorld, float dt)
{
    if (!mBossWipeDamagePending)
    {
        return;
    }

    if (auto* uiManager = mGame != nullptr ? mGame->GetUIManager() : nullptr)
    {
        if (isOtherWorld)
        {
            uiManager->HideMirrorCrackWarning();
        }
        else
        {
            const float progress = mBossWipeDamageDuration > 0.0f
                ? 1.0f - (std::clamp)(mBossWipeDamageTimer / mBossWipeDamageDuration, 0.0f, 1.0f)
                : 1.0f;
            uiManager->ShowMirrorCrackWarning(progress);
        }
    }

    mBossWipeDamageTimer -= dt;
    if (mBossWipeDamageTimer > 0.0f)
    {
        return;
    }

    mBossWipeDamagePending = false;
    mBossWipeDamageTimer = 0.0f;
    ApplyBossWipeDamage(player, isOtherWorld);
}

void Stage2BossController::ApplyBossWipeDamage(Player* player, bool isOtherWorld)
{
    if (auto* uiManager = mGame != nullptr ? mGame->GetUIManager() : nullptr)
    {
        uiManager->HideMirrorCrackWarning();
    }

    if (isOtherWorld)
    {
        OutputDebugStringA("[Stage2Boss][Wipe] Lantern dimension evasion succeeded\n");
        return;
    }

    OutputDebugStringA("[Stage2Boss][Wipe] Lantern dimension evasion failed. Player killed\n");

    if (mBoss != nullptr && mBoss->GetState() != MonsterState::DIE)
    {
        mBoss->ForceAnimationState(MonsterState::DAMAGED);
    }

    if (NetworkManager::Get()->IsConnected())
    {
        OutputDebugStringA("[Stage2Boss][Wipe] Waiting for server player hit result\n");
        return;
    }

    if (player == nullptr)
    {
        return;
    }

    DirectX::XMFLOAT3 damageTextPosition = player->GetPosition();
    damageTextPosition.y += Player::DefaultColliderHalfHeight * 1.4f;
    if (player->IsDamageInvulnerable())
    {
        if (mDamageTextRenderer != nullptr)
        {
            mDamageTextRenderer->SpawnImmune(damageTextPosition);
        }
        return;
    }

    if (mDamageTextRenderer != nullptr)
    {
        mDamageTextRenderer->SpawnIncoming(damageTextPosition, player->GetMaxHP());
    }

    player->ApplyServerHit(0, true);
}

void Stage2BossController::UpdateBossPatternTriggers(Player* player, int currentBossLayer)
{
    if (mBoss == nullptr || mBoss->GetState() == MonsterState::DIE || currentBossLayer <= 0)
    {
        return;
    }

    if (!mBossPattern150Triggered && currentBossLayer <= kBossPattern150Layer)
    {
        mBossPattern150Triggered = true;
        TriggerBossPattern150(player);
    }

    if (!mBossWipeTriggered && currentBossLayer <= kBossWipeLayer)
    {
        mBossWipeTriggered = true;
        TriggerBossWipePattern(player);
    }

    if (!mBossMirrorPatternTriggered && currentBossLayer <= kBossMirrorPatternLayer)
    {
        mBossMirrorPatternTriggered = true;
        TriggerBossMirrorPattern(player);
    }
}

void Stage2BossController::TriggerBossPattern150(Player* player)
{
    if (mBoss == nullptr || mBoss->GetState() == MonsterState::DIE)
    {
        return;
    }

    OutputDebugStringA("[Stage2Boss][Pattern] 150-layer shockwave triggered\n");

    const DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();
    ShowBossPatternRadiusIndicator(bossPos, kBossPattern150Radius, kBossPattern150DamageDelay);
    mBossPattern150DamagePending = true;
    mBossPattern150DamageTimer = kBossPattern150DamageDelay;
    mBossPattern150DamageCenter = bossPos;
    PlayBossScriptedAnimation(
        BossScriptedAnimationState::Pattern150Roar,
        "BossRoar",
        kBossRoarFallbackDuration,
        kBossPatternAnimationBlendDuration);

    if (player != nullptr)
    {
        const DirectX::XMFLOAT3 playerPos = player->GetPosition();
        const float dx = playerPos.x - bossPos.x;
        const float dz = playerPos.z - bossPos.z;
        mBossFacingYaw = ComputeBossVisualYaw(std::atan2(dx, dz));
        mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
        mBoss->GameObject::Update();
    }
}

void Stage2BossController::TriggerBossWipePattern(Player* player)
{
    if (mBoss == nullptr || mBoss->GetState() == MonsterState::DIE)
    {
        return;
    }

    OutputDebugStringA("[Stage2Boss][Wipe] 100-layer lantern dimension wipe triggered\n");

    const DirectX::XMFLOAT3 bossPos = mBoss->GetPosition();
    mBossWipeDamagePending = true;
    mBossWipeDamageTimer = kBossWipeDamageDelay;
    mBossWipeDamageDuration = kBossWipeDamageDelay;
    mBossWipeDamageCenter = bossPos;
    PlayBossScriptedAnimation(
        BossScriptedAnimationState::WipeSummon,
        "BossSummonSwordWhip",
        kBossSummonFallbackDuration,
        kBossPatternAnimationBlendDuration);
    if (auto* uiManager = mGame != nullptr ? mGame->GetUIManager() : nullptr)
    {
        uiManager->ShowMirrorCrackWarning(0.0f);
    }

    if (player != nullptr)
    {
        const DirectX::XMFLOAT3 playerPos = player->GetPosition();
        const float dx = playerPos.x - bossPos.x;
        const float dz = playerPos.z - bossPos.z;
        mBossFacingYaw = ComputeBossVisualYaw(std::atan2(dx, dz));
        mBoss->SetRotation(0.0f, mBossFacingYaw, 0.0f);
        mBoss->GameObject::Update();
    }
}

void Stage2BossController::UpdateBossHealthUi(Player* player, int currentBossLayer, bool isOtherWorld)
{
    const bool shouldShowBossHealth = !isOtherWorld && ShouldShowBossHealth(player);
    mShowBossHealthText = shouldShowBossHealth;
    mBossHealthTextLayer = shouldShowBossHealth ? currentBossLayer : 0;

    if (auto* uiManager = mGame != nullptr ? mGame->GetUIManager() : nullptr)
    {
        if (shouldShowBossHealth && mBoss != nullptr)
        {
            uiManager->UpdateBossHealthBar(mBoss->GetHP(), mBoss->GetMaxHP());
        }
        else
        {
            uiManager->HideBossHealthBar();
        }
    }
}

void Stage2BossController::UpdateBossWorldVisibility(bool isOtherWorld)
{
    if (mBoss != nullptr && mBoss->Ritem != nullptr)
    {
        bool shouldShowBoss = !isOtherWorld && mBoss->GetState() != MonsterState::DIE;
        if (mBoss->Ritem->Visible != shouldShowBoss)
        {
            mBoss->Ritem->Visible = shouldShowBoss;
            mBoss->Ritem->NumFramesDirty = gNumFrameResources;
        }
    }

    for (int i = 0; i < kBossMirrorSlotCount; ++i)
    {
        if (mBossMirrorObjects[static_cast<size_t>(i)] != nullptr &&
            mBossMirrorObjects[static_cast<size_t>(i)]->Ritem != nullptr)
        {
            const bool shouldShowMirror =
                !isOtherWorld &&
                mBossMirrorPatternState != BossMirrorPatternState::Inactive;
            if (mBossMirrorObjects[static_cast<size_t>(i)]->Ritem->Visible != shouldShowMirror)
            {
                mBossMirrorObjects[static_cast<size_t>(i)]->Ritem->Visible = shouldShowMirror;
                mBossMirrorObjects[static_cast<size_t>(i)]->Ritem->NumFramesDirty = gNumFrameResources;
            }
        }

        if (mBossMirrorCloneObjects[static_cast<size_t>(i)] != nullptr &&
            mBossMirrorCloneObjects[static_cast<size_t>(i)]->Ritem != nullptr)
        {
            const bool shouldShowClone =
                !isOtherWorld &&
                mBossMirrorPatternState == BossMirrorPatternState::Split &&
                i != mBossMirrorRealIndex;
            if (mBossMirrorCloneObjects[static_cast<size_t>(i)]->Ritem->Visible != shouldShowClone)
            {
                mBossMirrorCloneObjects[static_cast<size_t>(i)]->Ritem->Visible = shouldShowClone;
                mBossMirrorCloneObjects[static_cast<size_t>(i)]->Ritem->NumFramesDirty = gNumFrameResources;
            }
        }
    }

    if (!isOtherWorld)
    {
        return;
    }

    if (mBossPatternRadiusObj != nullptr && mBossPatternRadiusObj->Ritem != nullptr)
    {
        if (mBossPatternRadiusObj->Ritem->Visible)
        {
            mBossPatternRadiusObj->Ritem->Visible = false;
            mBossPatternRadiusObj->Ritem->NumFramesDirty = gNumFrameResources;
        }
    }

    if (mBossPatternRadiusRingObj != nullptr && mBossPatternRadiusRingObj->Ritem != nullptr)
    {
        if (mBossPatternRadiusRingObj->Ritem->Visible)
        {
            mBossPatternRadiusRingObj->Ritem->Visible = false;
            mBossPatternRadiusRingObj->Ritem->NumFramesDirty = gNumFrameResources;
        }
    }
}

void Stage2BossController::DrawBossHealthText()
{
    if (!mShowBossHealthText ||
        mBossHealthTextLayer <= 0 ||
        !mBossHealthTextFont ||
        !mBossHealthTextBatch ||
        !mBossHealthTextHeap)
    {
        return;
    }

    auto* cmdList = mGame != nullptr ? mGame->GetCommandList() : nullptr;
    if (cmdList == nullptr)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    if (viewport.Width <= 0.0f || viewport.Height <= 0.0f)
    {
        return;
    }

    try
    {
        ID3D12DescriptorHeap* heaps[] = { mBossHealthTextHeap->Heap() };
        cmdList->SetDescriptorHeaps(1, heaps);

        mBossHealthTextBatch->SetViewport(viewport);
        mBossHealthTextBatch->Begin(cmdList);

        const std::wstring label = L"x" + std::to_wstring(mBossHealthTextLayer);
        constexpr float textScale = 0.42f;
        constexpr float rightPadding = 30.0f;

        const DirectX::XMVECTOR textSize = mBossHealthTextFont->MeasureString(label.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * textScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * textScale;
        const float barRightPixel = (kBossBarMaxScaleX + 1.0f) * 0.5f * viewport.Width;
        const float barCenterYPixel = (1.0f - kBossBarY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            barRightPixel - textWidth - rightPadding,
            barCenterYPixel - textHeight * 0.5f - 1.0f);

        const DirectX::XMVECTORF32 shadowColor = { 0.0f, 0.0f, 0.0f, 0.72f };
        const DirectX::XMVECTORF32 textColor = { 1.0f, 0.92f, 0.48f, 1.0f };

        mBossHealthTextFont->DrawString(
            mBossHealthTextBatch.get(),
            label.c_str(),
            DirectX::XMFLOAT2(textPos.x + 1.0f, textPos.y + 1.0f),
            shadowColor,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            textScale);
        mBossHealthTextFont->DrawString(
            mBossHealthTextBatch.get(),
            label.c_str(),
            textPos,
            textColor,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            textScale);

        mBossHealthTextBatch->End();
    }
    catch (const std::exception& e)
    {
        std::string log = "[Stage2BossUI] Failed to draw boss HP font: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());
        mShowBossHealthText = false;
    }
}

int Stage2BossController::CalculateBossHealthLayer(float currentHp, float maxHp) const
{
    if (maxHp <= 0.0f || currentHp <= 0.0f)
    {
        return 0;
    }

    const float clampedHp = (std::clamp)(currentHp, 0.0f, maxHp);
    const float hpPerLayer = maxHp / static_cast<float>(BossHpLayerCount);
    return (std::clamp)(
        static_cast<int>(std::ceil(clampedHp / hpPerLayer)),
        1,
        BossHpLayerCount);
}

bool Stage2BossController::ShouldShowBossHealth(Player* player) const
{
    if (player == nullptr || mBoss == nullptr || mBoss->GetState() == MonsterState::DIE)
    {
        return false;
    }

    const DirectX::XMFLOAT3 playerPos = player->GetPosition();
    const float dx = playerPos.x - kStage2BossAnchorPosition.x;
    const float dz = playerPos.z - kStage2BossAnchorPosition.z;
    return (dx * dx + dz * dz) <= (kBossAreaRadius * kBossAreaRadius);
}

void Stage2BossController::TrackOwned(GameObject* object, RenderItem* renderItem) const
{
    if (mTrackOwned)
    {
        mTrackOwned(object, renderItem);
    }
}
