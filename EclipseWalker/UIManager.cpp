#include "UIManager.h"
#include "EclipseWalkerGame.h"
#include "Vertices.h"
#include "AudioManager.h"
#include "Protocol.h"
#include "Scene.h"
#include <Windows.h>
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
    constexpr float kUiBaseWidth = 1280.0f;
    constexpr float kUiBaseHeight = 720.0f;
    constexpr wchar_t kStageClearSound[] = L"Sounds\\StageClear.mp3";
    constexpr float kStageClearSoundVolume = 0.14f;
    constexpr float kHudCenterX = -0.715f;
    constexpr float kHudCenterY = 0.77f;
    constexpr float kHudFrameScaleX = 0.285f;
    constexpr float kHudFrameScaleY = 0.185f;
    constexpr float kHudBarLeftOffsetX = -0.11f;
    constexpr float kHudBarMaxScaleX = 0.170f;
    constexpr float kHudHpY = 0.818f;
    constexpr float kHudMpY = 0.731f;
    constexpr float kHudExpY = 0.676f;
    constexpr float kHudHpScaleY = 0.034f;
    constexpr float kHudMpScaleY = 0.034f;
    constexpr float kHudExpScaleY = 0.010f;
    constexpr float kHudClassEmblemScaleY = 0.104f;
    constexpr bool kDebugAutoDrainHudBars = false;
    constexpr float kDebugHudDrainCycleSeconds = 4.0f;
    constexpr float kLowHealthWarningThreshold = 0.30f;
    constexpr float kLowHealthPulseSpeed = 2.4f;
    constexpr float kLowHealthMinAlpha = 0.10f;
    constexpr float kLowHealthMaxAlpha = 0.44f;
    constexpr float kLanternFrameRadius = 0.170f;
    constexpr float kLanternRingRadius = 0.132f;
    constexpr float kLanternRingOffsetY = -0.004f;
    constexpr float kLanternCoreRadius = 0.078f;
    constexpr float kSkillBarAspect = 2.0f;
    constexpr float kSkillBarScaleY = 0.135f;
    constexpr float kSkillBarMarginX = 0.025f;
    constexpr float kSkillBarMarginY = 0.035f;
    constexpr float kSkillIconScaleY = 0.071f;
    constexpr float kSkillIconOffsetXFactor = 0.38f;
    constexpr float kSkillIconOffsetY = 0.0f;
    constexpr float kPotionSlotGapX = 0.006f;
    constexpr float kPotionSlotSpacingFactor = 1.86f;
    constexpr float kPotionSlotIconScaleY = 0.048f;
    constexpr float kDashCooldownRadius = 0.061f;
    constexpr float kDashCooldownFillRadius = 0.050f;
    constexpr float kDashCooldownIconScaleY = 0.058f;
    constexpr float kDashCooldownFrameScaleY = 0.071f;
    constexpr float kDashCooldownTextScale = 0.56f;
    constexpr float kKeyHintTextScale = 0.36f;
    constexpr float kSkillKeyHintOffsetY = -0.052f;
    constexpr float kPotionKeyHintOffsetY = -0.034f;
    constexpr float kRespawnOverlayScaleX = 1.0f;
    constexpr float kRespawnOverlayScaleY = 1.0f;
    constexpr float kRespawnButtonCenterX = 0.0f;
    constexpr float kRespawnButtonCenterY = -0.08f;
    constexpr float kRespawnButtonScaleX = 0.17f;
    constexpr float kRespawnButtonScaleY = 0.055f;
    constexpr float kRespawnButtonFrameScaleX = 0.176f;
    constexpr float kRespawnButtonFrameScaleY = 0.061f;
    constexpr float kRespawnTitleY = -0.31f;
    constexpr float kRespawnCountdownY = -0.23f;
    constexpr float kRespawnTitleScale = 1.12f;
    constexpr float kRespawnCountdownScale = 0.82f;
    constexpr float kRespawnButtonTextScale = 0.80f;
    constexpr float kReturnConfirmOverlayScaleX = 1.0f;
    constexpr float kReturnConfirmOverlayScaleY = 1.0f;
    constexpr float kReturnConfirmPanelCenterX = 0.0f;
    constexpr float kReturnConfirmPanelCenterY = 0.03f;
    constexpr float kReturnConfirmPanelScaleX = 0.34f;
    constexpr float kReturnConfirmPanelScaleY = 0.20f;
    constexpr float kReturnConfirmPanelFrameScaleX = 0.35f;
    constexpr float kReturnConfirmPanelFrameScaleY = 0.21f;
    constexpr float kReturnConfirmTitleY = 0.125f;
    constexpr float kReturnConfirmSubtitleY = 0.045f;
    constexpr float kReturnConfirmButtonY = -0.080f;
    constexpr float kReturnConfirmYesButtonX = -0.105f;
    constexpr float kReturnConfirmNoButtonX = 0.105f;
    constexpr float kReturnConfirmButtonScaleX = 0.080f;
    constexpr float kReturnConfirmButtonScaleY = 0.045f;
    constexpr float kReturnConfirmButtonFrameScaleX = 0.086f;
    constexpr float kReturnConfirmButtonFrameScaleY = 0.051f;
    constexpr float kReturnConfirmTitleScale = 0.74f;
    constexpr float kReturnConfirmSubtitleScale = 0.42f;
    constexpr float kReturnConfirmButtonTextScale = 0.58f;
    constexpr float kStageClearOverlayScaleX = 1.0f;
    constexpr float kStageClearOverlayScaleY = 1.0f;
    constexpr float kStageClearPanelCenterX = 0.0f;
    constexpr float kStageClearPanelCenterY = 0.02f;
    constexpr float kStageClearPanelScaleX = 0.62f;
    constexpr float kStageClearPanelScaleY = 0.58f;
    constexpr float kStageClearPanelFrameScaleX = 0.634f;
    constexpr float kStageClearPanelFrameScaleY = 0.598f;
    constexpr float kStageClearBannerScaleX = 0.33f;
    constexpr float kStageClearBannerScaleY = 0.072f;
    constexpr float kStageClearBannerCenterY = 0.34f;
    constexpr float kStageClearTimeAboveTitleY = 0.455f;
    constexpr float kStageClearTitleY = 0.345f;
    constexpr float kStageClearSubtitleY = 0.245f;
    constexpr float kStageClearHeaderY = 0.095f;
    constexpr float kStageClearFirstRowY = 0.025f;
    constexpr float kStageClearRowStepY = 0.075f;
    constexpr float kStageClearTitleScale = 1.18f;
    constexpr float kStageClearSubtitleScale = 0.62f;
    constexpr float kStageClearTimeScale = 0.76f;
    constexpr float kStageClearHeaderScale = 0.56f;
    constexpr float kStageClearRowScale = 0.58f;
    constexpr float kStageClearRecordHeaderY = 0.145f;
    constexpr float kStageClearRecordFirstRowY = 0.085f;
    constexpr float kStageClearRecordRowStepY = 0.045f;
    constexpr float kStageClearRecordHeaderScale = 0.43f;
    constexpr float kStageClearRecordRowScale = 0.38f;
    constexpr float kStageClearButtonCenterX = 0.455f;
    constexpr float kStageClearButtonCenterY = -0.465f;
    constexpr float kStageClearButtonScaleX = 0.155f;
    constexpr float kStageClearButtonScaleY = 0.052f;
    constexpr float kStageClearButtonFrameScaleX = 0.162f;
    constexpr float kStageClearButtonFrameScaleY = 0.059f;
    constexpr float kStageClearButtonTextScale = 0.48f;
    constexpr float kBossBarCenterX = 0.0f;
    constexpr float kBossBarY = 0.84f;
    constexpr float kBossBarFrameAspect = 11.40f;
    constexpr float kBossBarFrameScaleY = 0.085f;
    constexpr float kBossBarFillMaxScaleX = 0.360f;
    constexpr float kBossBarFillScaleY = 0.018f;
    constexpr float kBossBarGlossScaleY = 0.005f;
    constexpr float kEclipseTimerPanelCenterX = 0.765f;
    constexpr float kEclipseTimerPanelCenterY = 0.813f;
    constexpr float kEclipseTimerPanelScaleX = 0.175f;
    constexpr float kEclipseTimerPanelScaleY = 0.094f;
    constexpr float kEclipseTimerProgressCenterX = 0.765f;
    constexpr float kEclipseTimerProgressMaxScaleX = 0.095f;
    constexpr float kEclipseTimerProgressScaleY = 0.0085f;
    constexpr float kEclipseTimerProgressY = 0.756f;
    constexpr float kEclipseTimerLabelY = 0.860f;
    constexpr float kEclipseTimerTimeY = 0.806f;
    constexpr float kEclipseTimerLabelScale = 0.36f;
    constexpr float kEclipseTimerTimeScale = 0.72f;
    constexpr float kGoldTextRightMargin = 28.0f;
    constexpr float kGoldTextTopMargin = 6.0f;
    constexpr float kGoldTextTopMarginWithTimer = 6.0f;
    constexpr float kGoldTextScale = 0.54f;
    constexpr float kStatsPanelCenterX = 0.715f;
    constexpr float kStatsPanelCenterY = 0.045f;
    constexpr float kStatsPanelScaleX = 0.275f;
    constexpr float kStatsPanelScaleY = 0.560f;
    constexpr float kStatsPanelTextLeftMargin = 34.0f;
    constexpr float kStatsPanelValueRightMargin = 54.0f;
    constexpr float kStatsPanelTextTopMargin = 74.0f;
    constexpr float kStatsPanelRowHeight = 30.0f;
    constexpr float kStatsPanelSectionGap = 8.0f;
    constexpr float kStatsPanelTitleScale = 0.62f;
    constexpr float kStatsPanelTextScale = 0.40f;

    DirectX::XMFLOAT4 GetLevelUpFlashColor(PlayerClass playerClass, int newLevel)
    {
        const float intensity = newLevel >= 3 ? 1.15f : 1.0f;
        switch (playerClass)
        {
        case PlayerClass::Warrior:
            return { 1.0f, 0.76f * intensity, 0.38f * intensity, 0.0f };
        case PlayerClass::Mage:
            return { 0.62f * intensity, 0.88f * intensity, 1.0f, 0.0f };
        case PlayerClass::Archer:
            return { 0.58f * intensity, 1.0f, 0.62f * intensity, 0.0f };
        case PlayerClass::None:
        default:
            return { 1.0f, 1.0f, 1.0f, 0.0f };
        }
    }

    void SetTexScale(RenderItem* ritem, float scaleU, float scaleV = 1.0f)
    {
        if (ritem == nullptr)
        {
            return;
        }

        DirectX::XMStoreFloat4x4(
            &ritem->TexTransform,
            DirectX::XMMatrixScaling(scaleU, scaleV, 1.0f));
        ritem->NumFramesDirty = gNumFrameResources;
    }

    std::wstring FormatClearTimeLabel(float clearTimeSeconds)
    {
        const float clampedSeconds = (std::max)(0.0f, clearTimeSeconds);
        const int totalSeconds = static_cast<int>(clampedSeconds);
        const int minutes = totalSeconds / 60;
        const int seconds = totalSeconds % 60;
        const int tenths = static_cast<int>(std::floor((clampedSeconds - static_cast<float>(totalSeconds)) * 10.0f + 0.5f));

        std::wostringstream oss;
        oss << L"클리어 시간  "
            << std::setfill(L'0') << std::setw(2) << minutes
            << L":"
            << std::setfill(L'0') << std::setw(2) << seconds
            << L"."
            << (std::clamp)(tenths, 0, 9);
        return oss.str();
    }

    std::wstring FormatClearTimeShort(float clearTimeSeconds)
    {
        const float clampedSeconds = (std::max)(0.0f, clearTimeSeconds);
        const int totalSeconds = static_cast<int>(clampedSeconds);
        const int minutes = totalSeconds / 60;
        const int seconds = totalSeconds % 60;
        const int tenths = static_cast<int>(std::floor((clampedSeconds - static_cast<float>(totalSeconds)) * 10.0f + 0.5f));

        std::wostringstream oss;
        oss << std::setfill(L'0') << std::setw(2) << minutes
            << L":"
            << std::setfill(L'0') << std::setw(2) << seconds
            << L"."
            << (std::clamp)(tenths, 0, 9);
        return oss.str();
    }

    std::wstring FormatEclipseCountdownTime(float remainingSeconds)
    {
        const int totalSeconds = (std::max)(0, static_cast<int>(std::ceil(remainingSeconds)));
        const int minutes = totalSeconds / 60;
        const int seconds = totalSeconds % 60;

        std::wostringstream oss;
        oss << std::setfill(L'0') << std::setw(2) << minutes
            << L":"
            << std::setfill(L'0') << std::setw(2) << seconds;
        return oss.str();
    }

    std::wstring FormatGoldAmount(int gold)
    {
        std::wstring digits = std::to_wstring((std::max)(gold, 0));
        for (int insertIndex = static_cast<int>(digits.size()) - 3; insertIndex > 0; insertIndex -= 3)
        {
            digits.insert(static_cast<size_t>(insertIndex), 1, L',');
        }

        return digits;
    }

    std::wstring FormatStatNumber(float value)
    {
        return std::to_wstring(static_cast<int>(std::lround(value)));
    }

    std::wstring GetPlayerClassName(PlayerClass playerClass)
    {
        switch (playerClass)
        {
        case PlayerClass::Warrior:
            return L"전사";
        case PlayerClass::Mage:
            return L"마법사";
        case PlayerClass::Archer:
            return L"궁수";
        case PlayerClass::None:
        default:
            return L"없음";
        }
    }

    float GetResponsiveTextScale(const D3D12_VIEWPORT& viewport, float minScale = 0.85f, float maxScale = 1.65f)
    {
        const float widthScale = (std::max)(1.0f, viewport.Width) / kUiBaseWidth;
        const float heightScale = (std::max)(1.0f, viewport.Height) / kUiBaseHeight;
        return std::clamp((std::min)(widthScale, heightScale), minScale, maxScale);
    }

    float GetGoldTextUiScale(const D3D12_VIEWPORT& viewport)
    {
        return GetResponsiveTextScale(viewport, 0.82f, 1.45f);
    }

    float GetEclipseTimerPanelCenterXForViewport(const D3D12_VIEWPORT& viewport)
    {
        if (viewport.Width <= 0.0f)
        {
            return kEclipseTimerPanelCenterX;
        }

        const float rightMarginPixels = kGoldTextRightMargin * GetGoldTextUiScale(viewport);
        const float rightEdgePixels = (std::max)(0.0f, viewport.Width - rightMarginPixels);
        const float rightEdgeNdc = (rightEdgePixels / viewport.Width) * 2.0f - 1.0f;
        return std::clamp(
            rightEdgeNdc - kEclipseTimerPanelScaleX,
            -1.0f + kEclipseTimerPanelScaleX,
            1.0f - kEclipseTimerPanelScaleX);
    }

    float GetEclipseTimerProgressCenterXForViewport(const D3D12_VIEWPORT& viewport)
    {
        return GetEclipseTimerPanelCenterXForViewport(viewport);
    }
}

UIManager::UIManager(EclipseWalkerGame* game) : mGame(game)
{
}

UIManager::~UIManager()
{
}

void UIManager::BuildInGameUI()
{
    auto& ritems = mGame->GetRitems();
    auto res = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* cmdList = mGame->GetCommandList();
    auto* cmdQueue = mGame->GetCommandQueue();

    auto createUIMaterial = [&](const std::string& name, const DirectX::XMFLOAT4& color)
        {
            res->CreateMaterial(name, static_cast<int>(res->mMaterials.size()), "white", "", "", "",
                color, DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
            if (auto mat = res->GetMaterial(name))
            {
                mat->IsTransparent = 1;
                mat->NumFramesDirty = 3;
            }
        };

    auto createUITextureMaterial = [&](const std::string& name, const std::string& textureName, const DirectX::XMFLOAT4& color)
        {
            res->CreateMaterial(name, static_cast<int>(res->mMaterials.size()), textureName, "", "", "",
                color, DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
            if (auto mat = res->GetMaterial(name))
            {
                mat->DiffuseMapName = textureName;
                mat->DiffuseAlbedo = color;
                mat->IsTransparent = 1;
                mat->NumFramesDirty = gNumFrameResources;
            }
        };

    createUIMaterial("UI_HudShadowMat", DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.52f));
    createUIMaterial("UI_HudPanelMat", DirectX::XMFLOAT4(0.025f, 0.026f, 0.032f, 0.96f));
    createUIMaterial("UI_HudFrameMat", DirectX::XMFLOAT4(0.62f, 0.66f, 0.70f, 1.0f));
    createUIMaterial("UI_HudInnerFrameMat", DirectX::XMFLOAT4(0.12f, 0.10f, 0.08f, 1.0f));
    createUITextureMaterial("UI_HPMPFrameMat", "UI_HPMP_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_HPFillTexMat", "UI_HP_Fill", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_HPDelayTexMat", "UI_HP_Fill", DirectX::XMFLOAT4(1.0f, 0.78f, 0.42f, 0.88f));
    createUITextureMaterial("UI_MPFillTexMat", "UI_MP_Fill", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_MPDelayTexMat", "UI_MP_Fill", DirectX::XMFLOAT4(0.58f, 1.0f, 1.0f, 0.82f));
    createUITextureMaterial("UI_HPMPGlossMat", "UI_HPMP_Gloss", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.55f));
    createUITextureMaterial("UI_ClassEmblemWarriorTexMat", "UI_ClassEmblem_Warrior", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_ClassEmblemMageTexMat", "UI_ClassEmblem_Mage", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_ClassEmblemArcherTexMat", "UI_ClassEmblem_Archer", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_LanternFrameTexMat", "UI_Lantern_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_LanternRingFillTexMat", "UI_Lantern_Ring_Fill", DirectX::XMFLOAT4(0.72f, 1.0f, 0.78f, 1.0f));
    createUITextureMaterial("UI_LanternCoreGlowTexMat", "UI_Lantern_Core_Glow", DirectX::XMFLOAT4(0.85f, 1.0f, 0.86f, 0.92f));
    createUITextureMaterial("UI_SkillBarTwoSlotsTexMat", "UI_SkillBar_TwoSlots", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_PotionHpSmallTexMat", "UI_Shop_Potion_HpSmall", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_PotionHpMediumTexMat", "UI_Shop_Potion_HpMedium", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_PotionMpSmallTexMat", "UI_Shop_Potion_MpSmall", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_PotionMpMediumTexMat", "UI_Shop_Potion_MpMedium", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_PotionBattleElixirTexMat", "UI_Shop_Potion_BattleElixir", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillWarriorEarthquakeSlamTexMat", "UI_Skill_Warrior_EarthquakeSlam", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillWarriorGreatswordSummonTexMat", "UI_Skill_Warrior_GreatswordSummon", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillMageHealingLightTexMat", "UI_Skill_Mage_HealingLight", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillMageMeteorTexMat", "UI_Skill_Mage_Meteor", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillArcherWindImbuementTexMat", "UI_Skill_Archer_WindImbuement", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_SkillArcherArrowRainTexMat", "UI_Skill_Archer_ArrowRain", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_BossHpFrameTexMat", "UI_BossHp_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_MirrorCrackMat", "UI_MirrorCrackOverlay", DirectX::XMFLOAT4(0.82f, 0.96f, 1.0f, 0.0f));
    if (res->GetTexture("UI_StatsPanel") != nullptr)
    {
        createUITextureMaterial("UI_StatsPanelMat", "UI_StatsPanel", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.96f));
    }
    else
    {
        createUIMaterial("UI_StatsPanelMat", DirectX::XMFLOAT4(0.026f, 0.030f, 0.038f, 0.94f));
    }
    createUIMaterial("UI_HpBackMat", DirectX::XMFLOAT4(0.13f, 0.025f, 0.03f, 1.0f));
    createUIMaterial("UI_HpDelayMat", DirectX::XMFLOAT4(0.95f, 0.48f, 0.22f, 1.0f));
    createUIMaterial("UI_HpMat", DirectX::XMFLOAT4(0.86f, 0.04f, 0.06f, 1.0f));
    createUIMaterial("UI_HpGlossMat", DirectX::XMFLOAT4(1.0f, 0.48f, 0.42f, 0.42f));
    createUIMaterial("UI_ExpBackMat", DirectX::XMFLOAT4(0.018f, 0.055f, 0.022f, 0.96f));
    createUIMaterial("UI_ExpMat", DirectX::XMFLOAT4(0.12f, 0.92f, 0.24f, 1.0f));
    createUIMaterial("UI_BossHpFrameMat", DirectX::XMFLOAT4(0.015f, 0.012f, 0.013f, 0.92f));
    createUIMaterial("UI_BossHpBackMat", DirectX::XMFLOAT4(0.12f, 0.018f, 0.024f, 0.94f));
    createUIMaterial("UI_BossHpDelayMat", DirectX::XMFLOAT4(0.95f, 0.52f, 0.18f, 0.95f));
    createUIMaterial("UI_BossHpFillMat", DirectX::XMFLOAT4(0.78f, 0.025f, 0.04f, 1.0f));
    createUIMaterial("UI_BossHpGlossMat", DirectX::XMFLOAT4(1.0f, 0.36f, 0.32f, 0.36f));
    createUIMaterial("UI_BossHpCapMat", DirectX::XMFLOAT4(0.70f, 0.60f, 0.42f, 0.98f));
    createUITextureMaterial("UI_LowHealthEdgeMat", "UI_LowHealthEdgeOverlay", DirectX::XMFLOAT4(1.0f, 0.02f, 0.01f, 0.0f));
    createUIMaterial("UI_MpBackMat", DirectX::XMFLOAT4(0.025f, 0.045f, 0.13f, 1.0f));
    createUIMaterial("UI_MpDelayMat", DirectX::XMFLOAT4(0.30f, 0.88f, 1.0f, 1.0f));
    createUIMaterial("UI_MpMat", DirectX::XMFLOAT4(0.04f, 0.30f, 0.94f, 1.0f));
    createUIMaterial("UI_MpGlossMat", DirectX::XMFLOAT4(0.55f, 0.86f, 1.0f, 0.38f));
    createUIMaterial("UI_LanternRingBackMat", DirectX::XMFLOAT4(0.04f, 0.10f, 0.075f, 0.88f));
    createUIMaterial("UI_LanternRingMat", DirectX::XMFLOAT4(0.08f, 0.94f, 0.38f, 1.0f));
    createUIMaterial("UI_LanternOrbGlowMat", DirectX::XMFLOAT4(0.14f, 0.95f, 0.42f, 0.34f));
    createUIMaterial("UI_LanternOrbCoreMat", DirectX::XMFLOAT4(0.12f, 0.72f, 0.30f, 0.85f));
    createUIMaterial("UI_SkillCooldown1BackMat", DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.84f));
    createUIMaterial("UI_SkillCooldown1FillMat", DirectX::XMFLOAT4(0.03f, 0.04f, 0.05f, 0.82f));
    createUIMaterial("UI_SkillCooldown2BackMat", DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.84f));
    createUIMaterial("UI_SkillCooldown2FillMat", DirectX::XMFLOAT4(0.03f, 0.04f, 0.05f, 0.82f));
    createUIMaterial("UI_DashCooldownBackMat", DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.84f));
    createUIMaterial("UI_DashCooldownFillMat", DirectX::XMFLOAT4(0.03f, 0.04f, 0.05f, 0.82f));
    createUIMaterial("UI_PotionSlotBackMat", DirectX::XMFLOAT4(0.005f, 0.006f, 0.008f, 0.92f));
    for (int i = 0; i < 3; ++i)
    {
        createUIMaterial("UI_PotionCooldownBackMat" + std::to_string(i), DirectX::XMFLOAT4(0.02f, 0.025f, 0.032f, 0.0f));
        createUIMaterial("UI_PotionCooldownFillMat" + std::to_string(i), DirectX::XMFLOAT4(0.02f, 0.025f, 0.032f, 0.0f));
    }
    if (res->GetTexture("UI_DashCooldown_Frame") != nullptr)
    {
        createUITextureMaterial("UI_DashCooldownFrameTexMat", "UI_DashCooldown_Frame", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    else
    {
        createUIMaterial("UI_DashCooldownFrameTexMat", DirectX::XMFLOAT4(0.92f, 0.95f, 1.0f, 0.98f));
    }
    createUITextureMaterial("UI_DashIconWarriorTexMat", "UI_Skill_Warrior_Dash", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_DashIconMageTexMat", "UI_Skill_Mage_Dash", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    createUITextureMaterial("UI_DashIconArcherTexMat", "UI_Skill_Archer_Dash", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    res->CreateMaterial("UI_LanternIconMat", static_cast<int>(res->mMaterials.size()), "LanternIcon", "", "", "",
        DirectX::XMFLOAT4(0.18f, 1.0f, 0.36f, 1.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_LanternIconMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    mLanternRingMat = res->GetMaterial("UI_LanternRingFillTexMat");
    mLanternGlowMat = res->GetMaterial("UI_LanternCoreGlowTexMat");
    mLanternIconMat = nullptr;
    res->CreateMaterial("UI_ChatLogMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.05f, 0.07f, 0.09f, 0.72f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ChatInputMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.14f, 0.16f, 0.2f, 0.88f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_RespawnOverlayMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.16f, 0.16f, 0.16f, 0.78f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_RespawnButtonMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.28f, 0.28f, 0.30f, 0.92f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_RespawnButtonFrameMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.78f, 0.78f, 0.80f, 0.98f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ReturnConfirmOverlayMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.58f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ReturnConfirmPanelMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.045f, 0.050f, 0.062f, 0.96f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ReturnConfirmPanelFrameMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.78f, 0.62f, 0.30f, 0.98f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ReturnConfirmYesButtonMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.28f, 0.20f, 0.10f, 0.96f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ReturnConfirmYesButtonFrameMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.90f, 0.72f, 0.34f, 0.98f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ReturnConfirmNoButtonMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.12f, 0.13f, 0.16f, 0.96f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ReturnConfirmNoButtonFrameMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.52f, 0.54f, 0.60f, 0.96f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_StageClearOverlayMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.02f, 0.03f, 0.05f, 0.84f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_StageClearPanelMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.07f, 0.08f, 0.10f, 0.96f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_StageClearPanelFrameMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.86f, 0.74f, 0.38f, 0.98f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_StageClearBannerMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.76f, 0.60f, 0.20f, 0.98f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_StageClearButtonMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.18f, 0.15f, 0.10f, 0.96f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_StageClearButtonFrameMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.92f, 0.74f, 0.36f, 0.98f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    createUIMaterial("UI_EclipseTimerPanelMat", DirectX::XMFLOAT4(0.018f, 0.020f, 0.026f, 0.80f));
    res->CreateMaterial("UI_EclipseTimerProgressBackMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.07f, 0.07f, 0.085f, 0.90f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_EclipseTimerProgressFillMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.95f, 0.64f, 0.18f, 0.96f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);

    if (auto mat = res->GetMaterial("UI_ChatLogMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ChatInputMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_RespawnOverlayMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_RespawnButtonMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_RespawnButtonFrameMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ReturnConfirmOverlayMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ReturnConfirmPanelMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ReturnConfirmPanelFrameMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ReturnConfirmYesButtonMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ReturnConfirmYesButtonFrameMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ReturnConfirmNoButtonMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ReturnConfirmNoButtonFrameMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_StageClearOverlayMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_StageClearPanelMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_StageClearPanelFrameMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_StageClearBannerMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_StageClearButtonMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_StageClearButtonFrameMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_EclipseTimerPanelMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_EclipseTimerProgressBackMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_EclipseTimerProgressFillMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    mChatLogMat = res->GetMaterial("UI_ChatLogMat");
    mChatInputMat = res->GetMaterial("UI_ChatInputMat");
    mRespawnOverlayMat = res->GetMaterial("UI_RespawnOverlayMat");
    mRespawnButtonMat = res->GetMaterial("UI_RespawnButtonMat");
    mRespawnButtonFrameMat = res->GetMaterial("UI_RespawnButtonFrameMat");
    mReturnConfirmOverlayMat = res->GetMaterial("UI_ReturnConfirmOverlayMat");
    mReturnConfirmPanelMat = res->GetMaterial("UI_ReturnConfirmPanelMat");
    mReturnConfirmPanelFrameMat = res->GetMaterial("UI_ReturnConfirmPanelFrameMat");
    mReturnConfirmYesButtonMat = res->GetMaterial("UI_ReturnConfirmYesButtonMat");
    mReturnConfirmYesButtonFrameMat = res->GetMaterial("UI_ReturnConfirmYesButtonFrameMat");
    mReturnConfirmNoButtonMat = res->GetMaterial("UI_ReturnConfirmNoButtonMat");
    mReturnConfirmNoButtonFrameMat = res->GetMaterial("UI_ReturnConfirmNoButtonFrameMat");
    mStageClearOverlayMat = res->GetMaterial("UI_StageClearOverlayMat");
    mStageClearPanelMat = res->GetMaterial("UI_StageClearPanelMat");
    mStageClearPanelFrameMat = res->GetMaterial("UI_StageClearPanelFrameMat");
    mStageClearBannerMat = res->GetMaterial("UI_StageClearBannerMat");
    mStageClearButtonMat = res->GetMaterial("UI_StageClearButtonMat");
    mStageClearButtonFrameMat = res->GetMaterial("UI_StageClearButtonFrameMat");
    mEclipseTimerPanelMat = res->GetMaterial("UI_EclipseTimerPanelMat");
    mEclipseTimerProgressBackMat = res->GetMaterial("UI_EclipseTimerProgressBackMat");
    mEclipseTimerProgressFillMat = res->GetMaterial("UI_EclipseTimerProgressFillMat");
    mBossHpBackMat = res->GetMaterial("UI_BossHpBackMat");
    mBossHpDelayMat = res->GetMaterial("UI_BossHpDelayMat");
    mBossHpFillMat = res->GetMaterial("UI_BossHpFillMat");
    mBossHpGlossMat = res->GetMaterial("UI_BossHpGlossMat");
    mMirrorCrackMat = res->GetMaterial("UI_MirrorCrackMat");
    mLowHealthEdgeMat = res->GetMaterial("UI_LowHealthEdgeMat");
    mClassEmblemWarriorMat = res->GetMaterial("UI_ClassEmblemWarriorTexMat");
    mClassEmblemMageMat = res->GetMaterial("UI_ClassEmblemMageTexMat");
    mClassEmblemArcherMat = res->GetMaterial("UI_ClassEmblemArcherTexMat");
    mSkillIcon1WarriorMat = res->GetMaterial("UI_SkillWarriorEarthquakeSlamTexMat");
    mSkillIcon2WarriorMat = res->GetMaterial("UI_SkillWarriorGreatswordSummonTexMat");
    mSkillIcon1MageMat = res->GetMaterial("UI_SkillMageHealingLightTexMat");
    mSkillIcon2MageMat = res->GetMaterial("UI_SkillMageMeteorTexMat");
    mSkillIcon1ArcherMat = res->GetMaterial("UI_SkillArcherWindImbuementTexMat");
    mSkillIcon2ArcherMat = res->GetMaterial("UI_SkillArcherArrowRainTexMat");
    mPotionHpSmallMat = res->GetMaterial("UI_PotionHpSmallTexMat");
    mPotionHpMediumMat = res->GetMaterial("UI_PotionHpMediumTexMat");
    mPotionMpSmallMat = res->GetMaterial("UI_PotionMpSmallTexMat");
    mPotionMpMediumMat = res->GetMaterial("UI_PotionMpMediumTexMat");
    mPotionBattleElixirMat = res->GetMaterial("UI_PotionBattleElixirTexMat");

    auto createUIMeshGeometry = [&](const std::string& name, const std::vector<Vertex>& vertices, const std::vector<std::uint16_t>& indices, const std::string& submeshName)
        {
            if (res->mGeometries.find(name) != res->mGeometries.end())
            {
                return;
            }

            const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
            const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

            auto geometry = std::make_unique<MeshGeometry>();
            geometry->Name = name;

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
            geometry->DrawArgs[submeshName] = submesh;

            res->mGeometries[name] = std::move(geometry);
        };

    auto buildRingGeometry = [&]()
        {
            constexpr int segmentCount = 96;
            constexpr float innerRadius = 0.84f;
            constexpr float outerRadius = 1.0f;
            std::vector<Vertex> vertices;
            std::vector<std::uint16_t> indices;
            vertices.reserve((segmentCount + 1) * 2);
            indices.reserve(segmentCount * 6);

            for (int i = 0; i <= segmentCount; ++i)
            {
                const float angle = -DirectX::XM_PIDIV2 + DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(segmentCount);
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                vertices.push_back(Vertex({ DirectX::XMFLOAT3(c * outerRadius, s * outerRadius, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
                vertices.push_back(Vertex({ DirectX::XMFLOAT3(c * innerRadius, s * innerRadius, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f + c * innerRadius * 0.5f, 0.5f - s * innerRadius * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
            }

            for (int i = 0; i < segmentCount; ++i)
            {
                const std::uint16_t outer0 = static_cast<std::uint16_t>(i * 2);
                const std::uint16_t inner0 = static_cast<std::uint16_t>(i * 2 + 1);
                const std::uint16_t outer1 = static_cast<std::uint16_t>((i + 1) * 2);
                const std::uint16_t inner1 = static_cast<std::uint16_t>((i + 1) * 2 + 1);
                indices.insert(indices.end(), { outer0, outer1, inner0, inner0, outer1, inner1 });
            }

            createUIMeshGeometry("uiLanternRingGeo", vertices, indices, "ring");
        };

    auto buildDiskGeometry = [&]()
        {
            constexpr int segmentCount = 96;
            std::vector<Vertex> vertices;
            std::vector<std::uint16_t> indices;
            vertices.reserve(segmentCount + 2);
            indices.reserve(segmentCount * 3);

            vertices.push_back(Vertex({ DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f, 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
            for (int i = 0; i <= segmentCount; ++i)
            {
                const float angle = DirectX::XM_PIDIV2 - DirectX::XM_2PI * static_cast<float>(i) / static_cast<float>(segmentCount);
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                vertices.push_back(Vertex({ DirectX::XMFLOAT3(c, s, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.5f + c * 0.5f, 0.5f - s * 0.5f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }));
            }

            for (int i = 1; i <= segmentCount; ++i)
            {
                indices.insert(indices.end(), { 0, static_cast<std::uint16_t>(i), static_cast<std::uint16_t>(i + 1) });
            }

            createUIMeshGeometry("uiLanternDiskGeo", vertices, indices, "disk");
        };

    buildRingGeometry();
    buildDiskGeometry();

    auto setupRitem = [&](RenderItem* ritem) {
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = ritem->Geo->DrawArgs["quad"].IndexCount;
        ritem->StartIndexLocation = ritem->Geo->DrawArgs["quad"].StartIndexLocation;
        ritem->BaseVertexLocation = ritem->Geo->DrawArgs["quad"].BaseVertexLocation;
        };

    auto createUIQuad = [&](const std::string& materialName, float scaleX, float scaleY, float x, float y, float z, float rotationZ = 0.0f)
        {
            auto ritem = std::make_unique<RenderItem>();
            ritem->Geo = res->mGeometries["quadGeo"].get();
            ritem->Mat = res->GetMaterial(materialName);
            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            setupRitem(ritem.get());

            auto object = std::make_unique<GameObject>();
            object->SetScale(scaleX, scaleY, 1.0f);
            object->SetPosition(x, y, z);
            object->SetRotation(0.0f, 0.0f, rotationZ);
            object->Ritem = ritem.get();
            object->Update();

            GameObject* rawObject = object.get();
            ritems.push_back(std::move(ritem));
            mUIObjects.push_back(std::move(object));
            return rawObject;
        };

    auto createUIMeshObject = [&](const std::string& materialName, const std::string& geometryName, const std::string& submeshName, float scaleX, float scaleY, float x, float y, float z, RenderItem** outRenderItem = nullptr)
        {
            auto ritem = std::make_unique<RenderItem>();
            ritem->Geo = res->mGeometries[geometryName].get();
            ritem->Mat = res->GetMaterial(materialName);
            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;

            auto object = std::make_unique<GameObject>();
            object->SetScale(scaleX, scaleY, 1.0f);
            object->SetPosition(x, y, z);
            object->Ritem = ritem.get();
            object->Update();

            GameObject* rawObject = object.get();
            if (outRenderItem != nullptr)
            {
                *outRenderItem = ritem.get();
            }
            ritems.push_back(std::move(ritem));
            mUIObjects.push_back(std::move(object));
            return rawObject;
        };

    const float lanternCenterX = 0.88f;
    const float lanternCenterY = 0.0f;
    const auto viewport = mGame->GetScreenViewport();
    const float lanternAspectFix = viewport.Width > 0.0f ? (viewport.Height / viewport.Width) : (9.0f / 16.0f);
    const float classEmblemScaleX = kHudClassEmblemScaleY * lanternAspectFix;
    const float classEmblemCenterX = kHudCenterX - kHudFrameScaleX + 0.094f;
    const float classEmblemCenterY = kHudCenterY + 0.004f;
    const float skillBarScaleX = kSkillBarScaleY * kSkillBarAspect * lanternAspectFix;
    const float skillBarCenterX = 1.0f - kSkillBarMarginX - skillBarScaleX;
    const float skillBarCenterY = -1.0f + kSkillBarMarginY + kSkillBarScaleY;

    mHpMpFrame = createUIQuad("UI_HPMPFrameMat", kHudFrameScaleX, kHudFrameScaleY, kHudCenterX, kHudCenterY, 0.142f);
    mHpBarDelay = createUIQuad("UI_HPDelayTexMat", kHudBarMaxScaleX, kHudHpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudHpY, 0.136f);
    mHpBarFill = createUIQuad("UI_HPFillTexMat", kHudBarMaxScaleX, kHudHpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudHpY, 0.132f);
    mMpBarDelay = createUIQuad("UI_MPDelayTexMat", kHudBarMaxScaleX, kHudMpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudMpY, 0.128f);
    mMpBarFill = createUIQuad("UI_MPFillTexMat", kHudBarMaxScaleX, kHudMpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudMpY, 0.124f);
    mExpBarBack = createUIQuad("UI_ExpBackMat", kHudBarMaxScaleX, kHudExpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudExpY, 0.122f);
    mExpBarFill = createUIQuad("UI_ExpMat", kHudBarMaxScaleX, kHudExpScaleY, kHudCenterX + kHudBarLeftOffsetX + kHudBarMaxScaleX, kHudExpY, 0.121f);
    mHpMpGloss = createUIQuad("UI_HPMPGlossMat", kHudFrameScaleX, kHudFrameScaleY, kHudCenterX, kHudCenterY, 0.120f);
    GameObject* classEmblem = createUIQuad("UI_ClassEmblemMageTexMat", classEmblemScaleX, kHudClassEmblemScaleY, classEmblemCenterX, classEmblemCenterY, 0.144f);
    mClassEmblem = classEmblem;
    mClassEmblemRitem = classEmblem != nullptr ? classEmblem->Ritem : nullptr;

    const float bossBarFrameScaleX = kBossBarFrameScaleY * kBossBarFrameAspect * lanternAspectFix;
    mBossHpBack = createUIQuad("UI_BossHpBackMat", kBossBarFillMaxScaleX, kBossBarFillScaleY, kBossBarCenterX, kBossBarY, 0.100f);
    mBossHpDelay = createUIQuad("UI_BossHpDelayMat", kBossBarFillMaxScaleX, kBossBarFillScaleY, kBossBarCenterX, kBossBarY, 0.095f);
    mBossHpFill = createUIQuad("UI_BossHpFillMat", kBossBarFillMaxScaleX, kBossBarFillScaleY, kBossBarCenterX, kBossBarY, 0.090f);
    mBossHpGloss = createUIQuad("UI_BossHpGlossMat", kBossBarFillMaxScaleX, kBossBarGlossScaleY, kBossBarCenterX, kBossBarY + 0.006f, 0.085f);
    mBossHpFrame = createUIQuad("UI_BossHpFrameTexMat", bossBarFrameScaleX, kBossBarFrameScaleY, kBossBarCenterX, kBossBarY, 0.080f);
    HideBossHealthBar();

    mLanternFrame = createUIQuad("UI_LanternFrameTexMat", kLanternFrameRadius * lanternAspectFix, kLanternFrameRadius, lanternCenterX, lanternCenterY, 0.103f);
    createUIMeshObject("UI_LanternRingFillTexMat", "uiLanternRingGeo", "ring", kLanternRingRadius * lanternAspectFix, kLanternRingRadius, lanternCenterX, lanternCenterY + kLanternRingOffsetY, 0.098f, &mLanternRingFillRitem);
    if (mLanternRingFillRitem)
    {
        mLanternRingFillRitem->IndexCount = 0;
        mLanternRingFillRitem->NumFramesDirty = gNumFrameResources;
    }
    mLanternOrbGlow = createUIQuad("UI_LanternCoreGlowTexMat", kLanternCoreRadius * lanternAspectFix, kLanternCoreRadius, lanternCenterX, lanternCenterY, 0.092f);
    mSkillBarBg = createUIQuad("UI_SkillBarTwoSlotsTexMat", skillBarScaleX, kSkillBarScaleY, skillBarCenterX, skillBarCenterY, 0.088f);
    const float skillIconScaleX = kSkillIconScaleY * lanternAspectFix;
    const float skillIconOffsetX = skillBarScaleX * kSkillIconOffsetXFactor;
    GameObject* skillIcon1 = createUIQuad("UI_SkillMageHealingLightTexMat", skillIconScaleX, kSkillIconScaleY,
        skillBarCenterX - skillIconOffsetX, skillBarCenterY + kSkillIconOffsetY, 0.086f);
    GameObject* skillIcon2 = createUIQuad("UI_SkillMageMeteorTexMat", skillIconScaleX, kSkillIconScaleY,
        skillBarCenterX + skillIconOffsetX, skillBarCenterY + kSkillIconOffsetY, 0.084f);
    mSkill1CooldownWidget.Icon = skillIcon1;
    mSkill2CooldownWidget.Icon = skillIcon2;
    mSkillIcon1Ritem = skillIcon1 != nullptr ? skillIcon1->Ritem : nullptr;
    mSkillIcon2Ritem = skillIcon2 != nullptr ? skillIcon2->Ritem : nullptr;
    UpdateSkillIconMaterials();

    const float skillCooldownRadiusX = kDashCooldownFillRadius * lanternAspectFix;
    const float skillCooldownRadiusY = kDashCooldownFillRadius;
    const float skill1CenterX = skillBarCenterX - skillIconOffsetX;
    const float skill2CenterX = skillBarCenterX + skillIconOffsetX;
    const float skillCenterY = skillBarCenterY + kSkillIconOffsetY;

    mSkill1CooldownWidget.CenterX = skill1CenterX;
    mSkill1CooldownWidget.CenterY = skillCenterY;
    mSkill1CooldownWidget.BackMat = res->GetMaterial("UI_SkillCooldown1BackMat");
    mSkill1CooldownWidget.FillMat = res->GetMaterial("UI_SkillCooldown1FillMat");
    mSkill1CooldownWidget.IconWarriorMat = mSkillIcon1WarriorMat;
    mSkill1CooldownWidget.IconMageMat = mSkillIcon1MageMat;
    mSkill1CooldownWidget.IconArcherMat = mSkillIcon1ArcherMat;
    mSkill1CooldownWidget.IconRitem = mSkillIcon1Ritem;
    mSkill1CooldownWidget.Back = createUIMeshObject(
        "UI_SkillCooldown1BackMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill1CenterX,
        skillCenterY,
        0.089f);
    mSkill1CooldownWidget.Fill = createUIMeshObject(
        "UI_SkillCooldown1FillMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill1CenterX,
        skillCenterY,
        0.090f,
        &mSkill1CooldownWidget.FillRitem);
    if (mSkill1CooldownWidget.FillRitem != nullptr)
    {
        mSkill1CooldownWidget.FillRitem->IndexCount = 0;
        mSkill1CooldownWidget.FillRitem->Visible = false;
        mSkill1CooldownWidget.FillRitem->NumFramesDirty = gNumFrameResources;
    }

    mSkill2CooldownWidget.CenterX = skill2CenterX;
    mSkill2CooldownWidget.CenterY = skillCenterY;
    mSkill2CooldownWidget.BackMat = res->GetMaterial("UI_SkillCooldown2BackMat");
    mSkill2CooldownWidget.FillMat = res->GetMaterial("UI_SkillCooldown2FillMat");
    mSkill2CooldownWidget.IconWarriorMat = mSkillIcon2WarriorMat;
    mSkill2CooldownWidget.IconMageMat = mSkillIcon2MageMat;
    mSkill2CooldownWidget.IconArcherMat = mSkillIcon2ArcherMat;
    mSkill2CooldownWidget.IconRitem = mSkillIcon2Ritem;
    mSkill2CooldownWidget.Back = createUIMeshObject(
        "UI_SkillCooldown2BackMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill2CenterX,
        skillCenterY,
        0.089f);
    mSkill2CooldownWidget.Fill = createUIMeshObject(
        "UI_SkillCooldown2FillMat",
        "uiLanternDiskGeo",
        "disk",
        skillCooldownRadiusX,
        skillCooldownRadiusY,
        skill2CenterX,
        skillCenterY,
        0.090f,
        &mSkill2CooldownWidget.FillRitem);
    if (mSkill2CooldownWidget.FillRitem != nullptr)
    {
        mSkill2CooldownWidget.FillRitem->IndexCount = 0;
        mSkill2CooldownWidget.FillRitem->Visible = false;
        mSkill2CooldownWidget.FillRitem->NumFramesDirty = gNumFrameResources;
    }

    const float dashFrameGap = 0.014f;
    const float dashCenterX = skillBarCenterX - skillBarScaleX - (kDashCooldownFrameScaleY * lanternAspectFix) - dashFrameGap;
    const float dashCenterY = skillBarCenterY - 0.002f;
    const float dashIconScaleX = kDashCooldownIconScaleY * lanternAspectFix;
    const float potionSlotScaleX = kDashCooldownFrameScaleY * lanternAspectFix;
    const float potionSlotScaleY = kDashCooldownFrameScaleY;
    const float potionSlotIconScaleX = kPotionSlotIconScaleY * lanternAspectFix;
    const float potionRightmostCenterX = dashCenterX - (kDashCooldownFrameScaleY * lanternAspectFix) - kPotionSlotGapX - potionSlotScaleX;
    const float potionSlotStepX = potionSlotScaleX * kPotionSlotSpacingFactor;
    const float potionLeftmostCenterX = potionRightmostCenterX - potionSlotStepX * 2.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float potionSlotCenterX = potionLeftmostCenterX + potionSlotStepX * static_cast<float>(i);
        mPotionSlotBacks[i] = createUIQuad(
            "UI_PotionSlotBackMat",
            potionSlotScaleX * 0.78f,
            potionSlotScaleY * 0.78f,
            potionSlotCenterX,
            dashCenterY,
            0.094f);
        mPotionSlotIcons[i] = createUIQuad(
            "UI_PotionHpSmallTexMat",
            potionSlotIconScaleX,
            kPotionSlotIconScaleY,
            potionSlotCenterX,
            dashCenterY,
            0.097f);
        if (mPotionSlotIcons[i] != nullptr && mPotionSlotIcons[i]->Ritem != nullptr)
        {
            mPotionSlotIconRitems[i] = mPotionSlotIcons[i]->Ritem;
            mPotionSlotIconRitems[i]->Visible = false;
            mPotionSlotIconRitems[i]->NumFramesDirty = gNumFrameResources;
        }

        const std::string cooldownBackMatName = "UI_PotionCooldownBackMat" + std::to_string(i);
        const std::string cooldownFillMatName = "UI_PotionCooldownFillMat" + std::to_string(i);
        CooldownWidget& potionWidget = mPotionCooldownWidgets[i];
        potionWidget.CenterX = potionSlotCenterX;
        potionWidget.CenterY = dashCenterY;
        potionWidget.BackMat = res->GetMaterial(cooldownBackMatName);
        potionWidget.FillMat = res->GetMaterial(cooldownFillMatName);
        potionWidget.IconRitem = mPotionSlotIconRitems[i];
        potionWidget.Back = createUIMeshObject(
            cooldownBackMatName,
            "uiLanternDiskGeo",
            "disk",
            kDashCooldownFillRadius * lanternAspectFix,
            kDashCooldownFillRadius,
            potionSlotCenterX,
            dashCenterY,
            0.099f);
        potionWidget.Fill = createUIMeshObject(
            cooldownFillMatName,
            "uiLanternDiskGeo",
            "disk",
            kDashCooldownFillRadius * lanternAspectFix,
            kDashCooldownFillRadius,
            potionSlotCenterX,
            dashCenterY,
            0.100f,
            &potionWidget.FillRitem);
        if (potionWidget.FillRitem != nullptr)
        {
            potionWidget.FillRitem->IndexCount = 0;
            potionWidget.FillRitem->Visible = false;
            potionWidget.FillRitem->NumFramesDirty = gNumFrameResources;
        }

        mPotionSlotFrames[i] = createUIQuad(
            "UI_DashCooldownFrameTexMat",
            potionSlotScaleX,
            potionSlotScaleY,
            potionSlotCenterX,
            dashCenterY,
            0.102f);
    }

    mDashCooldownWidget.CenterX = dashCenterX;
    mDashCooldownWidget.CenterY = dashCenterY;
    mDashCooldownWidget.BackMat = res->GetMaterial("UI_DashCooldownBackMat");
    mDashCooldownWidget.FillMat = res->GetMaterial("UI_DashCooldownFillMat");
    mDashCooldownWidget.IconWarriorMat = res->GetMaterial("UI_DashIconWarriorTexMat");
    mDashCooldownWidget.IconMageMat = res->GetMaterial("UI_DashIconMageTexMat");
    mDashCooldownWidget.IconArcherMat = res->GetMaterial("UI_DashIconArcherTexMat");
    mDashCooldownWidget.Back = createUIMeshObject(
        "UI_DashCooldownBackMat",
        "uiLanternDiskGeo",
        "disk",
        kDashCooldownFillRadius * lanternAspectFix,
        kDashCooldownFillRadius,
        dashCenterX,
        dashCenterY,
        0.094f);
    mDashCooldownWidget.Icon = createUIQuad(
        "UI_DashIconWarriorTexMat",
        dashIconScaleX,
        kDashCooldownIconScaleY,
        dashCenterX,
        dashCenterY,
        0.097f);
    if (mDashCooldownWidget.Icon != nullptr)
    {
        mDashCooldownWidget.IconRitem = mDashCooldownWidget.Icon->Ritem;
    }
    mDashCooldownWidget.Fill = createUIMeshObject(
        "UI_DashCooldownFillMat",
        "uiLanternDiskGeo",
        "disk",
        kDashCooldownFillRadius * lanternAspectFix,
        kDashCooldownFillRadius,
        dashCenterX,
        dashCenterY,
        0.100f,
        &mDashCooldownWidget.FillRitem);
    mDashCooldownWidget.Frame = createUIMeshObject(
        "UI_DashCooldownFrameTexMat",
        "quadGeo",
        "quad",
        kDashCooldownFrameScaleY * lanternAspectFix,
        kDashCooldownFrameScaleY,
        dashCenterX,
        dashCenterY,
        0.102f);
    if (mDashCooldownWidget.FillRitem != nullptr)
    {
        mDashCooldownWidget.FillRitem->IndexCount = 0;
        mDashCooldownWidget.FillRitem->Visible = false;
        mDashCooldownWidget.FillRitem->NumFramesDirty = gNumFrameResources;
    }
    mLastViewportWidth = 0.0f;
    mLastViewportHeight = 0.0f;
    RefreshResponsiveLayout();
    UpdateCooldownWidget(mSkill1CooldownWidget);
    UpdateCooldownWidget(mSkill2CooldownWidget);
    UpdateCooldownWidget(mDashCooldownWidget);
    for (auto& potionWidget : mPotionCooldownWidgets)
    {
        UpdatePotionCooldownWidget(potionWidget);
    }

    auto chatLogRitem = std::make_unique<RenderItem>();
    chatLogRitem->Geo = res->mGeometries["quadGeo"].get();
    chatLogRitem->Mat = res->GetMaterial("UI_ChatLogMat");
    chatLogRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    setupRitem(chatLogRitem.get());

    auto chatLogObj = std::make_unique<GameObject>();
    chatLogObj->SetScale(0.25f, 0.135f, 1.0f);
    chatLogObj->SetPosition(-0.735f, -0.74f, 0.11f);
    chatLogObj->Ritem = chatLogRitem.get();
    chatLogObj->Update();
    mChatLogBg = chatLogObj.get();
    ritems.push_back(std::move(chatLogRitem));
    mUIObjects.push_back(std::move(chatLogObj));

    auto chatInputRitem = std::make_unique<RenderItem>();
    chatInputRitem->Geo = res->mGeometries["quadGeo"].get();
    chatInputRitem->Mat = res->GetMaterial("UI_ChatInputMat");
    chatInputRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    setupRitem(chatInputRitem.get());

    auto chatInputObj = std::make_unique<GameObject>();
    chatInputObj->SetScale(0.25f, 0.038f, 1.0f);
    chatInputObj->SetPosition(-0.735f, -0.915f, 0.11f);
    chatInputObj->Ritem = chatInputRitem.get();
    chatInputObj->Update();
    mChatInputBg = chatInputObj.get();
    ritems.push_back(std::move(chatInputRitem));
    mUIObjects.push_back(std::move(chatInputObj));

    mRespawnOverlayBg = createUIQuad(
        "UI_RespawnOverlayMat",
        kRespawnOverlayScaleX,
        kRespawnOverlayScaleY,
        0.0f,
        0.0f,
        0.166f);
    mRespawnButtonFrame = createUIQuad(
        "UI_RespawnButtonFrameMat",
        kRespawnButtonFrameScaleX,
        kRespawnButtonFrameScaleY,
        kRespawnButtonCenterX,
        kRespawnButtonCenterY,
        0.160f);
    mRespawnButtonBg = createUIQuad(
        "UI_RespawnButtonMat",
        kRespawnButtonScaleX,
        kRespawnButtonScaleY,
        kRespawnButtonCenterX,
        kRespawnButtonCenterY,
        0.158f);
    if (mRespawnOverlayBg != nullptr && mRespawnOverlayBg->Ritem != nullptr)
    {
        mRespawnOverlayBg->Ritem->Visible = false;
        mRespawnOverlayBg->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mRespawnButtonFrame != nullptr && mRespawnButtonFrame->Ritem != nullptr)
    {
        mRespawnButtonFrame->Ritem->Visible = false;
        mRespawnButtonFrame->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mRespawnButtonBg != nullptr && mRespawnButtonBg->Ritem != nullptr)
    {
        mRespawnButtonBg->Ritem->Visible = false;
        mRespawnButtonBg->Ritem->NumFramesDirty = gNumFrameResources;
    }

    mReturnConfirmOverlayBg = createUIQuad(
        "UI_ReturnConfirmOverlayMat",
        kReturnConfirmOverlayScaleX,
        kReturnConfirmOverlayScaleY,
        0.0f,
        0.0f,
        0.174f);
    mReturnConfirmPanelFrame = createUIQuad(
        "UI_ReturnConfirmPanelFrameMat",
        kReturnConfirmPanelFrameScaleX,
        kReturnConfirmPanelFrameScaleY,
        kReturnConfirmPanelCenterX,
        kReturnConfirmPanelCenterY,
        0.171f);
    mReturnConfirmPanelBg = createUIQuad(
        "UI_ReturnConfirmPanelMat",
        kReturnConfirmPanelScaleX,
        kReturnConfirmPanelScaleY,
        kReturnConfirmPanelCenterX,
        kReturnConfirmPanelCenterY,
        0.170f);
    mReturnConfirmYesButtonFrame = createUIQuad(
        "UI_ReturnConfirmYesButtonFrameMat",
        kReturnConfirmButtonFrameScaleX,
        kReturnConfirmButtonFrameScaleY,
        kReturnConfirmYesButtonX,
        kReturnConfirmButtonY,
        0.168f);
    mReturnConfirmYesButtonBg = createUIQuad(
        "UI_ReturnConfirmYesButtonMat",
        kReturnConfirmButtonScaleX,
        kReturnConfirmButtonScaleY,
        kReturnConfirmYesButtonX,
        kReturnConfirmButtonY,
        0.166f);
    mReturnConfirmNoButtonFrame = createUIQuad(
        "UI_ReturnConfirmNoButtonFrameMat",
        kReturnConfirmButtonFrameScaleX,
        kReturnConfirmButtonFrameScaleY,
        kReturnConfirmNoButtonX,
        kReturnConfirmButtonY,
        0.168f);
    mReturnConfirmNoButtonBg = createUIQuad(
        "UI_ReturnConfirmNoButtonMat",
        kReturnConfirmButtonScaleX,
        kReturnConfirmButtonScaleY,
        kReturnConfirmNoButtonX,
        kReturnConfirmButtonY,
        0.166f);
    SetReturnToVillageConfirmState(false);

    mStageClearOverlayBg = createUIQuad(
        "UI_StageClearOverlayMat",
        kStageClearOverlayScaleX,
        kStageClearOverlayScaleY,
        0.0f,
        0.0f,
        0.172f);
    mStageClearPanelFrame = createUIQuad(
        "UI_StageClearPanelFrameMat",
        kStageClearPanelFrameScaleX,
        kStageClearPanelFrameScaleY,
        kStageClearPanelCenterX,
        kStageClearPanelCenterY,
        0.169f);
    mStageClearPanelBg = createUIQuad(
        "UI_StageClearPanelMat",
        kStageClearPanelScaleX,
        kStageClearPanelScaleY,
        kStageClearPanelCenterX,
        kStageClearPanelCenterY,
        0.167f);
    mStageClearBannerBg = createUIQuad(
        "UI_StageClearBannerMat",
        kStageClearBannerScaleX,
        kStageClearBannerScaleY,
        kStageClearPanelCenterX,
        kStageClearBannerCenterY,
        0.165f);
    mStageClearButtonFrame = createUIQuad(
        "UI_StageClearButtonFrameMat",
        kStageClearButtonFrameScaleX,
        kStageClearButtonFrameScaleY,
        kStageClearButtonCenterX,
        kStageClearButtonCenterY,
        0.164f);
    mStageClearButtonBg = createUIQuad(
        "UI_StageClearButtonMat",
        kStageClearButtonScaleX,
        kStageClearButtonScaleY,
        kStageClearButtonCenterX,
        kStageClearButtonCenterY,
        0.162f);
    if (mStageClearOverlayBg != nullptr && mStageClearOverlayBg->Ritem != nullptr)
    {
        mStageClearOverlayBg->Ritem->Visible = false;
        mStageClearOverlayBg->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearPanelFrame != nullptr && mStageClearPanelFrame->Ritem != nullptr)
    {
        mStageClearPanelFrame->Ritem->Visible = false;
        mStageClearPanelFrame->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearPanelBg != nullptr && mStageClearPanelBg->Ritem != nullptr)
    {
        mStageClearPanelBg->Ritem->Visible = false;
        mStageClearPanelBg->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearBannerBg != nullptr && mStageClearBannerBg->Ritem != nullptr)
    {
        mStageClearBannerBg->Ritem->Visible = false;
        mStageClearBannerBg->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearButtonFrame != nullptr && mStageClearButtonFrame->Ritem != nullptr)
    {
        mStageClearButtonFrame->Ritem->Visible = false;
        mStageClearButtonFrame->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearButtonBg != nullptr && mStageClearButtonBg->Ritem != nullptr)
    {
        mStageClearButtonBg->Ritem->Visible = false;
        mStageClearButtonBg->Ritem->NumFramesDirty = gNumFrameResources;
    }

    // Stage 2 eclipse timer panel.
    mEclipseTimerPanelBg = createUIQuad(
        "UI_EclipseTimerPanelMat",
        kEclipseTimerPanelScaleX,
        kEclipseTimerPanelScaleY,
        kEclipseTimerPanelCenterX,
        kEclipseTimerPanelCenterY,
        0.116f);
    mEclipseTimerProgressBack = createUIQuad(
        "UI_EclipseTimerProgressBackMat",
        kEclipseTimerProgressMaxScaleX,
        kEclipseTimerProgressScaleY,
        kEclipseTimerProgressCenterX,
        kEclipseTimerProgressY,
        0.112f);
    mEclipseTimerProgressFill = createUIQuad(
        "UI_EclipseTimerProgressFillMat",
        0.0f,
        kEclipseTimerProgressScaleY,
        kEclipseTimerProgressCenterX - kEclipseTimerProgressMaxScaleX,
        kEclipseTimerProgressY,
        0.108f);
    SetEclipseTimerState(false, 0.0f, 0.0f);

    mStatsPanelBg = createUIQuad(
        "UI_StatsPanelMat",
        kStatsPanelScaleX,
        kStatsPanelScaleY,
        kStatsPanelCenterX,
        kStatsPanelCenterY,
        0.118f);
    if (mStatsPanelBg != nullptr && mStatsPanelBg->Ritem != nullptr)
    {
        mStatsPanelBg->Ritem->Visible = false;
        mStatsPanelBg->Ritem->NumFramesDirty = gNumFrameResources;
    }

    // Screen flash effect materials.
    res->CreateMaterial("UI_FlashMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_FlashMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    res->CreateMaterial("UI_ScreenBgMat", static_cast<int>(res->mMaterials.size()), "white", "", "", "",
        DirectX::XMFLOAT4(0.95f, 0.9f, 0.72f, 0.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_ScreenBgMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    auto bgRitem = std::make_unique<RenderItem>();
    bgRitem->Geo = res->mGeometries["quadGeo"].get();
    bgRitem->Mat = res->GetMaterial("UI_ScreenBgMat");
    bgRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    bgRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    bgRitem->IndexCount = bgRitem->Geo->DrawArgs["quad"].IndexCount;
    bgRitem->StartIndexLocation = bgRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    bgRitem->BaseVertexLocation = bgRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto bgObj = std::make_unique<GameObject>();
    bgObj->Ritem = bgRitem.get();
    bgObj->SetScale(0.0f, 0.0f, 1.0f);
    bgObj->SetPosition(0.0f, 0.0f, 0.18f);
    bgObj->Update();
    mScreenBgObj = bgObj.get();

    ritems.push_back(std::move(bgRitem));
    mUIObjects.push_back(std::move(bgObj));

    auto flashRitem = std::make_unique<RenderItem>();
    flashRitem->Geo = res->mGeometries["quadGeo"].get();
    flashRitem->Mat = res->GetMaterial("UI_FlashMat");
    flashRitem->ObjCBIndex = static_cast<UINT>(ritems.size());

    flashRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    flashRitem->IndexCount = flashRitem->Geo->DrawArgs["quad"].IndexCount;
    flashRitem->StartIndexLocation = flashRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    flashRitem->BaseVertexLocation = flashRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto flashObj = std::make_unique<GameObject>();
    flashObj->Ritem = flashRitem.get();
    flashObj->SetScale(0.0f, 0.0f, 0.0f); 
    flashObj->Update();
    mFlashObj = flashObj.get();

    ritems.push_back(std::move(flashRitem));
    mUIObjects.push_back(std::move(flashObj));
    InitializeEffect(res->GetMaterial("UI_FlashMat"), res->GetMaterial("UI_ScreenBgMat"), mFlashObj, mScreenBgObj);

    mMirrorCrackObj = createUIQuad("UI_MirrorCrackMat", 0.0f, 0.0f, 0.0f, 0.0f, 0.060f);
    if (mMirrorCrackObj != nullptr && mMirrorCrackObj->Ritem != nullptr)
    {
        mMirrorCrackObj->Ritem->Visible = false;
        mMirrorCrackObj->Ritem->NumFramesDirty = gNumFrameResources;
    }

    mLowHealthEdgeObj = createUIQuad("UI_LowHealthEdgeMat", 1.0f, 1.0f, 0.0f, 0.0f, 0.052f);
    if (mLowHealthEdgeObj != nullptr && mLowHealthEdgeObj->Ritem != nullptr)
    {
        mLowHealthEdgeObj->Ritem->Visible = false;
        mLowHealthEdgeObj->Ritem->NumFramesDirty = gNumFrameResources;
    }

    if (device != nullptr && cmdQueue != nullptr)
    {
        try
        {
            if (!mCooldownTextHeap)
            {
                mCooldownTextHeap = std::make_unique<DirectX::DescriptorHeap>(
                    device,
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                    1);
            }

            if (!mCooldownTextFont || !mCooldownTextBatch)
            {
                DirectX::ResourceUploadBatch resourceUpload(device);
                resourceUpload.Begin();

                if (!mCooldownTextFont)
                {
                    mCooldownTextFont = std::make_unique<DirectX::SpriteFont>(
                        device,
                        resourceUpload,
                        L"Textures/chat_korean.spritefont",
                        mCooldownTextHeap->GetCpuHandle(0),
                        mCooldownTextHeap->GetGpuHandle(0));
                }

                if (!mCooldownTextBatch)
                {
                    DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
                    DirectX::SpriteBatchPipelineStateDescription pd(rtState);
                    mCooldownTextBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
                }

                auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
                uploadResourcesFinished.wait();
            }
        }
        catch (const std::exception& e)
        {
            std::string log = "[UIManager] Failed to initialize cooldown font: ";
            log += e.what();
            log += "\n";
            OutputDebugStringA(log.c_str());
            mCooldownTextFont.reset();
            mCooldownTextBatch.reset();
            mCooldownTextHeap.reset();
        }
    }
}

void UIManager::RefreshResponsiveLayout()
{
    if (mGame == nullptr)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    if (viewport.Width <= 0.0f || viewport.Height <= 0.0f)
    {
        return;
    }

    if (std::abs(viewport.Width - mLastViewportWidth) < 0.5f &&
        std::abs(viewport.Height - mLastViewportHeight) < 0.5f)
    {
        return;
    }

    mLastViewportWidth = viewport.Width;
    mLastViewportHeight = viewport.Height;

    const float aspectFix = viewport.Height / viewport.Width;
    const float lanternCenterX = 0.88f;
    const float lanternCenterY = 0.0f;
    const float classEmblemScaleX = kHudClassEmblemScaleY * aspectFix;
    const float classEmblemCenterX = kHudCenterX - kHudFrameScaleX + 0.094f;
    const float classEmblemCenterY = kHudCenterY + 0.004f;
    const float skillBarScaleX = kSkillBarScaleY * kSkillBarAspect * aspectFix;
    const float skillBarCenterX = 1.0f - kSkillBarMarginX - skillBarScaleX;
    const float skillBarCenterY = -1.0f + kSkillBarMarginY + kSkillBarScaleY;
    const float skillIconScaleX = kSkillIconScaleY * aspectFix;
    const float skillIconOffsetX = skillBarScaleX * kSkillIconOffsetXFactor;
    const float skillCenterY = skillBarCenterY + kSkillIconOffsetY;
    const float skill1CenterX = skillBarCenterX - skillIconOffsetX;
    const float skill2CenterX = skillBarCenterX + skillIconOffsetX;
    const float cooldownRadiusX = kDashCooldownFillRadius * aspectFix;
    const float dashFrameScaleX = kDashCooldownFrameScaleY * aspectFix;
    const float dashIconScaleX = kDashCooldownIconScaleY * aspectFix;
    const float dashCenterX = skillBarCenterX - skillBarScaleX - dashFrameScaleX - 0.014f;
    const float dashCenterY = skillBarCenterY - 0.002f;
    const float potionSlotScaleX = dashFrameScaleX;
    const float potionSlotIconScaleX = kPotionSlotIconScaleY * aspectFix;
    const float potionRightmostCenterX = dashCenterX - dashFrameScaleX - kPotionSlotGapX - potionSlotScaleX;
    const float potionSlotStepX = potionSlotScaleX * kPotionSlotSpacingFactor;
    const float potionLeftmostCenterX = potionRightmostCenterX - potionSlotStepX * 2.0f;
    const float bossBarFrameScaleX = kBossBarFrameScaleY * kBossBarFrameAspect * aspectFix;
    const float eclipseTimerPanelCenterX = GetEclipseTimerPanelCenterXForViewport(viewport);
    const float eclipseTimerProgressCenterX = GetEclipseTimerProgressCenterXForViewport(viewport);

    auto setTransform = [](GameObject* object, float scaleX, float scaleY, float x, float y)
    {
        if (object == nullptr)
        {
            return;
        }

        object->SetScale(scaleX, scaleY, 1.0f);
        object->SetPosition(x, y, object->GetPosition().z);
        object->Update();
    };

    setTransform(mClassEmblem, classEmblemScaleX, kHudClassEmblemScaleY, classEmblemCenterX, classEmblemCenterY);
    setTransform(mBossHpFrame, bossBarFrameScaleX, kBossBarFrameScaleY, kBossBarCenterX, kBossBarY);
    setTransform(mLanternFrame, kLanternFrameRadius * aspectFix, kLanternFrameRadius, lanternCenterX, lanternCenterY);
    setTransform(mLanternOrbGlow, kLanternCoreRadius * aspectFix, kLanternCoreRadius, lanternCenterX, lanternCenterY);
    setTransform(mLanternOrbCore, 0.057f * aspectFix, 0.057f, lanternCenterX, lanternCenterY);
    setTransform(mSkillBarBg, skillBarScaleX, kSkillBarScaleY, skillBarCenterX, skillBarCenterY);

    mSkill1CooldownWidget.CenterX = skill1CenterX;
    mSkill1CooldownWidget.CenterY = skillCenterY;
    setTransform(mSkill1CooldownWidget.Icon, skillIconScaleX, kSkillIconScaleY, skill1CenterX, skillCenterY);
    setTransform(mSkill1CooldownWidget.Back, cooldownRadiusX, kDashCooldownFillRadius, skill1CenterX, skillCenterY);
    setTransform(mSkill1CooldownWidget.Fill, cooldownRadiusX, kDashCooldownFillRadius, skill1CenterX, skillCenterY);

    mSkill2CooldownWidget.CenterX = skill2CenterX;
    mSkill2CooldownWidget.CenterY = skillCenterY;
    setTransform(mSkill2CooldownWidget.Icon, skillIconScaleX, kSkillIconScaleY, skill2CenterX, skillCenterY);
    setTransform(mSkill2CooldownWidget.Back, cooldownRadiusX, kDashCooldownFillRadius, skill2CenterX, skillCenterY);
    setTransform(mSkill2CooldownWidget.Fill, cooldownRadiusX, kDashCooldownFillRadius, skill2CenterX, skillCenterY);

    mDashCooldownWidget.CenterX = dashCenterX;
    mDashCooldownWidget.CenterY = dashCenterY;
    setTransform(mDashCooldownWidget.Back, cooldownRadiusX, kDashCooldownFillRadius, dashCenterX, dashCenterY);
    setTransform(mDashCooldownWidget.Fill, cooldownRadiusX, kDashCooldownFillRadius, dashCenterX, dashCenterY);
    setTransform(mDashCooldownWidget.Icon, dashIconScaleX, kDashCooldownIconScaleY, dashCenterX, dashCenterY);
    setTransform(mDashCooldownWidget.Frame, dashFrameScaleX, kDashCooldownFrameScaleY, dashCenterX, dashCenterY);
    for (int i = 0; i < 3; ++i)
    {
        const float potionSlotCenterX = potionLeftmostCenterX + potionSlotStepX * static_cast<float>(i);
        setTransform(
            mPotionSlotBacks[i],
            potionSlotScaleX * 0.78f,
            kDashCooldownFrameScaleY * 0.78f,
            potionSlotCenterX,
            dashCenterY);
        setTransform(
            mPotionSlotFrames[i],
            potionSlotScaleX,
            kDashCooldownFrameScaleY,
            potionSlotCenterX,
            dashCenterY);
        setTransform(
            mPotionSlotIcons[i],
            potionSlotIconScaleX,
            kPotionSlotIconScaleY,
            potionSlotCenterX,
            dashCenterY);

        mPotionCooldownWidgets[i].CenterX = potionSlotCenterX;
        mPotionCooldownWidgets[i].CenterY = dashCenterY;
        setTransform(
            mPotionCooldownWidgets[i].Back,
            cooldownRadiusX,
            kDashCooldownFillRadius,
            potionSlotCenterX,
            dashCenterY);
        setTransform(
            mPotionCooldownWidgets[i].Fill,
            cooldownRadiusX,
            kDashCooldownFillRadius,
            potionSlotCenterX,
            dashCenterY);
    }

    if (mLanternRingFillRitem != nullptr)
    {
        for (const auto& object : mUIObjects)
        {
            if (object != nullptr && object->Ritem == mLanternRingFillRitem)
            {
                setTransform(object.get(), kLanternRingRadius * aspectFix, kLanternRingRadius, lanternCenterX, lanternCenterY + kLanternRingOffsetY);
                break;
            }
        }
    }

    if (mChatLogBg != nullptr)
    {
        setTransform(mChatLogBg, 0.25f, 0.135f, -0.735f, -0.74f);
    }

    if (mChatInputBg != nullptr)
    {
        setTransform(mChatInputBg, 0.25f, 0.038f, -0.735f, -0.915f);
    }

    setTransform(
        mEclipseTimerPanelBg,
        kEclipseTimerPanelScaleX,
        kEclipseTimerPanelScaleY,
        eclipseTimerPanelCenterX,
        kEclipseTimerPanelCenterY);
    setTransform(
        mEclipseTimerProgressBack,
        kEclipseTimerProgressMaxScaleX,
        kEclipseTimerProgressScaleY,
        eclipseTimerProgressCenterX,
        kEclipseTimerProgressY);

    if (mEclipseTimerProgressFill != nullptr)
    {
        const float currentScale = kEclipseTimerProgressMaxScaleX * mEclipseTimerProgressRatio;
        const float leftEdgeX = eclipseTimerProgressCenterX - kEclipseTimerProgressMaxScaleX;
        mEclipseTimerProgressFill->SetScale(currentScale, kEclipseTimerProgressScaleY, 1.0f);
        mEclipseTimerProgressFill->SetPosition(leftEdgeX + currentScale, kEclipseTimerProgressY, mEclipseTimerProgressFill->GetPosition().z);
        mEclipseTimerProgressFill->Update();
    }
}

void UIManager::UpdateLowHealthEdgeWarning(float hpRatio, float dt)
{
    const bool active = hpRatio > 0.0f && hpRatio <= kLowHealthWarningThreshold;

    if (!active)
    {
        mLowHealthPulseTime = 0.0f;
        if (mLowHealthEdgeMat != nullptr)
        {
            mLowHealthEdgeMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 0.02f, 0.01f, 0.0f);
            mLowHealthEdgeMat->NumFramesDirty = gNumFrameResources;
        }

        if (mLowHealthEdgeObj != nullptr && mLowHealthEdgeObj->Ritem != nullptr)
        {
            mLowHealthEdgeObj->Ritem->Visible = false;
            mLowHealthEdgeObj->Ritem->NumFramesDirty = gNumFrameResources;
            mLowHealthEdgeObj->Update();
        }
        return;
    }

    mLowHealthPulseTime += (std::max)(0.0f, dt);
    const float pulse = 0.5f + 0.5f * std::sin(mLowHealthPulseTime * kLowHealthPulseSpeed);
    const float dangerRatio = (std::clamp)((kLowHealthWarningThreshold - hpRatio) / kLowHealthWarningThreshold, 0.0f, 1.0f);
    const float alpha = kLowHealthMinAlpha +
        (kLowHealthMaxAlpha - kLowHealthMinAlpha) * (0.35f + pulse * 0.65f) * (0.72f + dangerRatio * 0.28f);

    if (mLowHealthEdgeMat != nullptr)
    {
        mLowHealthEdgeMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 0.02f + pulse * 0.04f, 0.01f, alpha);
        mLowHealthEdgeMat->NumFramesDirty = gNumFrameResources;
    }

    if (mLowHealthEdgeObj != nullptr && mLowHealthEdgeObj->Ritem != nullptr)
    {
        mLowHealthEdgeObj->Ritem->Visible = true;
        mLowHealthEdgeObj->Ritem->NumFramesDirty = gNumFrameResources;
        mLowHealthEdgeObj->Update();
    }
}

void UIManager::Update(
    float currentHp,
    float maxHp,
    float currentMp,
    float maxMp,
    float currentLantern,
    float maxLantern,
    float currentDashCooldown,
    float maxDashCooldown,
    float currentExpRatio,
    int currentGold,
    PlayerClass playerClass,
    int currentLevel,
    int currentExperience,
    int experienceToNextLevel,
    const PlayerStats& playerStats,
    const std::array<PotionQuickSlot, 3>& potionQuickSlots,
    const std::array<float, 3>& potionCooldownRemaining,
    const std::array<float, 3>& potionCooldownDurations)
{
    RefreshResponsiveLayout();

    float hpRatio = maxHp > 0.0f ? (currentHp / maxHp) : 0.0f;
    float mpRatio = maxMp > 0.0f ? (currentMp / maxMp) : 0.0f;
    float lanternRatio = maxLantern > 0.0f ? (currentLantern / maxLantern) : 0.0f;
    constexpr float kUiFrameDelta = 1.0f / 60.0f;
    hpRatio = (std::clamp)(hpRatio, 0.0f, 1.0f);
    mpRatio = (std::clamp)(mpRatio, 0.0f, 1.0f);
    lanternRatio = (std::clamp)(lanternRatio, 0.0f, 1.0f);
    currentExpRatio = (std::clamp)(currentExpRatio, 0.0f, 1.0f);
    mCurrentGold = (std::max)(currentGold, 0);
    mCurrentPlayerClass = playerClass;
    mCurrentLevel = (std::max)(currentLevel, 1);
    mCurrentExperience = (std::max)(currentExperience, 0);
    mExperienceToNextLevel = (std::max)(experienceToNextLevel, 0);
    mCurrentHp = (std::max)(currentHp, 0.0f);
    mCurrentMaxHp = (std::max)(maxHp, 0.0f);
    mCurrentMp = (std::max)(currentMp, 0.0f);
    mCurrentMaxMp = (std::max)(maxMp, 0.0f);
    mCurrentPlayerStats = playerStats;
    mPotionQuickSlots = potionQuickSlots;

    const bool statsToggleKeyDown =
        !gIsChatInputActive &&
        GetForegroundWindow() == GetActiveWindow() &&
        (GetAsyncKeyState('C') & 0x8000) != 0;
    if (statsToggleKeyDown && !mStatsToggleKeyWasDown)
    {
        mStatsPanelOpen = !mStatsPanelOpen;
    }
    mStatsToggleKeyWasDown = statsToggleKeyDown;

    if (mStatsPanelBg != nullptr && mStatsPanelBg->Ritem != nullptr)
    {
        mStatsPanelBg->Ritem->Visible =
            mStatsPanelOpen &&
            !mStageClearScreenActive &&
            !mReturnToVillageConfirmActive &&
            !mRespawnScreenActive;
        mStatsPanelBg->Ritem->NumFramesDirty = gNumFrameResources;
    }
    for (int i = 0; i < 3; ++i)
    {
        mPotionCooldownWidgets[i].CooldownRemaining = potionCooldownRemaining[i];
        mPotionCooldownWidgets[i].CooldownDuration = potionCooldownDurations[i];
    }

    if (kDebugAutoDrainHudBars)
    {
        mDebugHudDrainTime += kUiFrameDelta;
        const float phase = std::fmod(mDebugHudDrainTime, kDebugHudDrainCycleSeconds) / kDebugHudDrainCycleSeconds;
        const float drainRatio = 1.0f - phase;
        hpRatio = drainRatio;
        mpRatio = drainRatio;
    }

    UpdateLowHealthEdgeWarning(hpRatio, kUiFrameDelta);

    if (hpRatio > mHpDelayRatio)
        mHpDelayRatio = hpRatio;
    else
        mHpDelayRatio += (hpRatio - mHpDelayRatio) * 0.075f;

    if (mpRatio > mMpDelayRatio)
        mMpDelayRatio = mpRatio;
    else
        mMpDelayRatio += (mpRatio - mMpDelayRatio) * 0.09f;

    if (lanternRatio > mLanternDelayRatio)
        mLanternDelayRatio = lanternRatio;
    else
        mLanternDelayRatio += (lanternRatio - mLanternDelayRatio) * 0.08f;

    const bool isLanternFull = lanternRatio >= 0.999f;
    if (isLanternFull || mMirrorCrackWarningActive)
    {
        mLanternGlowTime += kUiFrameDelta;
    }
    else
    {
        mLanternGlowTime = 0.0f;
    }

    if (mMirrorCrackWarningActive)
    {
        mMirrorCrackWarningTime += kUiFrameDelta;
    }

    auto updateBar = [](GameObject* bar, float ratio, float maxScaleX, float scaleY, float leftEdgeX, float y, float z)
        {
            if (!bar) return;

            ratio = (std::clamp)(ratio, 0.0f, 1.0f);
            const float currentScale = maxScaleX * ratio;
            bar->SetScale(currentScale, scaleY, 1.0f);
            bar->SetPosition(leftEdgeX + currentScale, y, z);
            SetTexScale(bar->Ritem, ratio);
        };

    const float barLeftEdgeX = kHudCenterX + kHudBarLeftOffsetX;
    updateBar(mHpBarDelay, mHpDelayRatio, kHudBarMaxScaleX, kHudHpScaleY, barLeftEdgeX, kHudHpY, 0.136f);
    updateBar(mHpBarFill, hpRatio, kHudBarMaxScaleX, kHudHpScaleY, barLeftEdgeX, kHudHpY, 0.132f);
    updateBar(mMpBarDelay, mMpDelayRatio, kHudBarMaxScaleX, kHudMpScaleY, barLeftEdgeX, kHudMpY, 0.128f);
    updateBar(mMpBarFill, mpRatio, kHudBarMaxScaleX, kHudMpScaleY, barLeftEdgeX, kHudMpY, 0.124f);
    updateBar(mExpBarFill, currentExpRatio, kHudBarMaxScaleX, kHudExpScaleY, barLeftEdgeX, kHudExpY, 0.121f);

    const auto viewport = mGame->GetScreenViewport();
    const float lanternAspectFix = viewport.Width > 0.0f ? (viewport.Height / viewport.Width) : (9.0f / 16.0f);

    if (mLanternRingFillRitem)
    {
        constexpr UINT kIndicesPerRingSegment = 6;
        const UINT fullIndexCount = mLanternRingFillRitem->Geo->DrawArgs["ring"].IndexCount;
        const UINT segmentCount = fullIndexCount / kIndicesPerRingSegment;
        const UINT activeSegments = static_cast<UINT>(std::ceil(mLanternDelayRatio * static_cast<float>(segmentCount)));
        mLanternRingFillRitem->IndexCount = (std::min)(activeSegments, segmentCount) * kIndicesPerRingSegment;
    }

    const float glowPulse = isLanternFull ? (0.5f + 0.5f * std::sin(mLanternGlowTime * 7.5f)) : 0.0f;
    const float mirrorWarningPulse = mMirrorCrackWarningActive ? (0.5f + 0.5f * std::sin(mLanternGlowTime * 13.0f)) : 0.0f;
    if (mLanternRingMat)
    {
        mLanternRingMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.35f + glowPulse * 0.25f, 1.0f, 0.52f + glowPulse * 0.25f, 1.0f)
            : DirectX::XMFLOAT4(0.08f, 0.94f, 0.38f, 1.0f);
        if (mMirrorCrackWarningActive)
        {
            mLanternRingMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.42f + mirrorWarningPulse * 0.28f, 1.0f, 0.88f + mirrorWarningPulse * 0.12f, 1.0f);
        }
        mLanternRingMat->NumFramesDirty = gNumFrameResources;
    }
    if (mLanternGlowMat)
    {
        mLanternGlowMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.28f, 1.0f, 0.48f, 0.52f + glowPulse * 0.14f)
            : DirectX::XMFLOAT4(0.14f, 0.95f, 0.42f, 0.34f);
        if (mMirrorCrackWarningActive)
        {
            mLanternGlowMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.34f, 1.0f, 0.88f, 0.62f + mirrorWarningPulse * 0.24f);
        }
        mLanternGlowMat->NumFramesDirty = gNumFrameResources;
    }
    if (mLanternIconMat)
    {
        mLanternIconMat->DiffuseAlbedo = isLanternFull
            ? DirectX::XMFLOAT4(0.24f, 1.0f, 0.55f + glowPulse * 0.28f, 1.0f)
            : DirectX::XMFLOAT4(0.18f, 1.0f, 0.36f, 1.0f);
        mLanternIconMat->NumFramesDirty = gNumFrameResources;
    }

    if (mLanternOrbGlow)
    {
        const float glowScale =
            kLanternCoreRadius +
            (isLanternFull ? glowPulse * 0.008f : 0.0f) +
            (mMirrorCrackWarningActive ? mirrorWarningPulse * 0.014f : 0.0f);
        mLanternOrbGlow->SetScale(glowScale * lanternAspectFix, glowScale, 1.0f);
    }
    if (mLanternOrbCore)
    {
        const float coreScale = 0.057f;
        mLanternOrbCore->SetScale(coreScale * lanternAspectFix, coreScale, 1.0f);
    }

    mDashCooldownWidget.CooldownRemaining = currentDashCooldown;
    mDashCooldownWidget.CooldownDuration = maxDashCooldown;
    UpdateSkillIconMaterials();
    UpdatePotionQuickSlotIcons();
    UpdateCooldownWidget(mSkill1CooldownWidget);
    UpdateCooldownWidget(mSkill2CooldownWidget);
    UpdateCooldownWidget(mDashCooldownWidget);
    for (auto& potionWidget : mPotionCooldownWidgets)
    {
        UpdatePotionCooldownWidget(potionWidget);
    }

    for (auto& obj : mUIObjects)
    {
        obj->Update();
    }
}

void UIManager::SetSkillCooldowns(float currentSkill1Cooldown, float maxSkill1Cooldown, float currentSkill2Cooldown, float maxSkill2Cooldown)
{
    mSkill1CooldownWidget.CooldownRemaining = currentSkill1Cooldown;
    mSkill1CooldownWidget.CooldownDuration = maxSkill1Cooldown;
    mSkill2CooldownWidget.CooldownRemaining = currentSkill2Cooldown;
    mSkill2CooldownWidget.CooldownDuration = maxSkill2Cooldown;
}

void UIManager::UpdateCooldownWidget(CooldownWidget& widget)
{
    const bool isDashWidget = (&widget == &mDashCooldownWidget);
    const int skillIndex =
        (&widget == &mSkill1CooldownWidget) ? 1 :
        ((&widget == &mSkill2CooldownWidget) ? 2 : 0);
    bool isSkillUnlocked = true;
    if (!isDashWidget)
    {
        if (mGame != nullptr)
        {
            if (auto* player = mGame->GetPlayer())
            {
                isSkillUnlocked = player->IsSkillUnlocked(skillIndex);
            }
            else
            {
                isSkillUnlocked = false;
            }
        }
        else
        {
            isSkillUnlocked = false;
        }
    }

    widget.CooldownRemaining = (std::max)(widget.CooldownRemaining, 0.0f);
    widget.CooldownDuration = (std::max)(widget.CooldownDuration, 0.0f);
    widget.CooldownRatio = (widget.CooldownDuration > 0.0f && widget.CooldownRemaining > 0.0f)
        ? (widget.CooldownRemaining / widget.CooldownDuration)
        : 0.0f;
    widget.CooldownRatio = (std::clamp)(widget.CooldownRatio, 0.0f, 1.0f);

    const bool isActive = widget.CooldownRatio > 0.001f;

    if (widget.FillRitem != nullptr && widget.FillRitem->Geo != nullptr)
    {
        constexpr UINT kIndicesPerDiskSegment = 3;
        const UINT fullIndexCount = widget.FillRitem->Geo->DrawArgs["disk"].IndexCount;
        const UINT segmentCount = fullIndexCount / kIndicesPerDiskSegment;
        UINT activeSegments = 0;
        if (isActive)
        {
            activeSegments = static_cast<UINT>(std::ceil(widget.CooldownRatio * static_cast<float>(segmentCount)));
            activeSegments = (std::min)(activeSegments, segmentCount);
        }

        widget.FillRitem->IndexCount = activeSegments * kIndicesPerDiskSegment;
        widget.FillRitem->Visible = activeSegments > 0;
        widget.FillRitem->NumFramesDirty = gNumFrameResources;
    }

    if (widget.BackMat != nullptr)
    {
        if (!isDashWidget && !isSkillUnlocked)
        {
            widget.BackMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.04f, 0.04f, 0.04f, 0.42f);
        }
        else
        {
            widget.BackMat->DiffuseAlbedo = isActive
                ? (isDashWidget
                    ? DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.92f)
                    : DirectX::XMFLOAT4(0.05f, 0.06f, 0.08f, 0.18f))
                : (isDashWidget
                    ? DirectX::XMFLOAT4(0.08f, 0.09f, 0.12f, 0.74f)
                    : DirectX::XMFLOAT4(0.05f, 0.06f, 0.08f, 0.0f));
        }
        widget.BackMat->NumFramesDirty = gNumFrameResources;
    }

    if (widget.FillMat != nullptr)
    {
        widget.FillMat->DiffuseAlbedo = (!isDashWidget && !isSkillUnlocked)
            ? DirectX::XMFLOAT4(0.02f, 0.03f, 0.04f, 0.0f)
            : (isActive
                ? (isDashWidget
                    ? DirectX::XMFLOAT4(0.02f, 0.03f, 0.04f, 0.82f)
                    : DirectX::XMFLOAT4(0.08f, 0.10f, 0.14f, 0.28f))
                : DirectX::XMFLOAT4(0.02f, 0.03f, 0.04f, 0.0f));
        widget.FillMat->NumFramesDirty = gNumFrameResources;
    }

    Material* targetIconMat = widget.IconWarriorMat;
    if (mGame != nullptr)
    {
        switch (mGame->GetSelectedPlayerClass())
        {
        case PlayerClass::Warrior:
            targetIconMat = widget.IconWarriorMat;
            break;
        case PlayerClass::Archer:
            targetIconMat = widget.IconArcherMat;
            break;
        case PlayerClass::Mage:
        case PlayerClass::None:
        default:
            targetIconMat = widget.IconMageMat;
            break;
        }
    }

    if (widget.IconRitem != nullptr && targetIconMat != nullptr)
    {
        widget.IconRitem->Mat = targetIconMat;
        widget.IconRitem->Visible = true;
        targetIconMat->DiffuseAlbedo = (!isDashWidget && !isSkillUnlocked)
            ? DirectX::XMFLOAT4(0.38f, 0.38f, 0.38f, 1.0f)
            : (isActive
                ? (isDashWidget
                    ? DirectX::XMFLOAT4(0.72f, 0.72f, 0.72f, 1.0f)
                    : DirectX::XMFLOAT4(0.93f, 0.93f, 0.93f, 1.0f))
                : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        targetIconMat->NumFramesDirty = gNumFrameResources;
        widget.IconRitem->ColorMultiplier = (!isDashWidget && !isSkillUnlocked)
            ? DirectX::XMFLOAT4(0.55f, 0.55f, 0.55f, 1.0f)
            : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        widget.IconRitem->NumFramesDirty = gNumFrameResources;
    }

    if (widget.Frame != nullptr && widget.Frame->Ritem != nullptr && widget.Frame->Ritem->Mat != nullptr)
    {
        widget.Frame->Ritem->Mat->DiffuseAlbedo = isActive
            ? DirectX::XMFLOAT4(0.90f, 0.93f, 0.98f, 0.98f)
            : DirectX::XMFLOAT4(0.98f, 1.0f, 1.0f, 1.0f);
        widget.Frame->Ritem->Mat->NumFramesDirty = gNumFrameResources;
    }
}

void UIManager::UpdatePotionCooldownWidget(CooldownWidget& widget)
{
    const bool hasPotionIcon = widget.IconRitem != nullptr && widget.IconRitem->Visible;

    widget.CooldownRemaining = (std::max)(widget.CooldownRemaining, 0.0f);
    widget.CooldownDuration = (std::max)(widget.CooldownDuration, 0.0f);
    widget.CooldownRatio = (hasPotionIcon && widget.CooldownDuration > 0.0f && widget.CooldownRemaining > 0.0f)
        ? (widget.CooldownRemaining / widget.CooldownDuration)
        : 0.0f;
    widget.CooldownRatio = (std::clamp)(widget.CooldownRatio, 0.0f, 1.0f);

    const bool isActive = widget.CooldownRatio > 0.001f;

    if (widget.FillRitem != nullptr && widget.FillRitem->Geo != nullptr)
    {
        constexpr UINT kIndicesPerDiskSegment = 3;
        const UINT fullIndexCount = widget.FillRitem->Geo->DrawArgs["disk"].IndexCount;
        const UINT segmentCount = fullIndexCount / kIndicesPerDiskSegment;
        UINT activeSegments = 0;
        if (isActive)
        {
            activeSegments = static_cast<UINT>(std::ceil(widget.CooldownRatio * static_cast<float>(segmentCount)));
            activeSegments = (std::min)(activeSegments, segmentCount);
        }

        widget.FillRitem->IndexCount = activeSegments * kIndicesPerDiskSegment;
        widget.FillRitem->Visible = activeSegments > 0;
        widget.FillRitem->NumFramesDirty = gNumFrameResources;
    }

    if (widget.BackMat != nullptr)
    {
        widget.BackMat->DiffuseAlbedo = isActive
            ? DirectX::XMFLOAT4(0.01f, 0.012f, 0.016f, 0.54f)
            : DirectX::XMFLOAT4(0.01f, 0.012f, 0.016f, 0.0f);
        widget.BackMat->NumFramesDirty = gNumFrameResources;
    }

    if (widget.FillMat != nullptr)
    {
        widget.FillMat->DiffuseAlbedo = isActive
            ? DirectX::XMFLOAT4(0.01f, 0.012f, 0.016f, 0.78f)
            : DirectX::XMFLOAT4(0.01f, 0.012f, 0.016f, 0.0f);
        widget.FillMat->NumFramesDirty = gNumFrameResources;
    }

    if (widget.IconRitem != nullptr)
    {
        widget.IconRitem->ColorMultiplier = isActive
            ? DirectX::XMFLOAT4(0.56f, 0.56f, 0.56f, 1.0f)
            : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        widget.IconRitem->NumFramesDirty = gNumFrameResources;
    }
}

void UIManager::UpdateSkillIconMaterials()
{
    PlayerClass activeClass = PlayerClass::Mage;
    if (mGame != nullptr)
    {
        if (auto* player = mGame->GetPlayer())
        {
            activeClass = player->GetClassType();
        }
        else
        {
            activeClass = mGame->GetSelectedPlayerClass();
        }
    }

    Material* skillIcon1Mat = mSkillIcon1MageMat;
    Material* skillIcon2Mat = mSkillIcon2MageMat;
    Material* classEmblemMat = mClassEmblemMageMat;
    switch (activeClass)
    {
    case PlayerClass::Warrior:
        skillIcon1Mat = mSkillIcon1WarriorMat;
        skillIcon2Mat = mSkillIcon2WarriorMat;
        classEmblemMat = mClassEmblemWarriorMat;
        break;
    case PlayerClass::Archer:
        skillIcon1Mat = mSkillIcon1ArcherMat;
        skillIcon2Mat = mSkillIcon2ArcherMat;
        classEmblemMat = mClassEmblemArcherMat;
        break;
    case PlayerClass::Mage:
    case PlayerClass::None:
    default:
        break;
    }

    if (mSkillIcon1Ritem != nullptr && skillIcon1Mat != nullptr)
    {
        mSkillIcon1Ritem->Mat = skillIcon1Mat;
        mSkillIcon1Ritem->NumFramesDirty = gNumFrameResources;
    }

    if (mSkillIcon2Ritem != nullptr && skillIcon2Mat != nullptr)
    {
        mSkillIcon2Ritem->Mat = skillIcon2Mat;
        mSkillIcon2Ritem->NumFramesDirty = gNumFrameResources;
    }

    if (mClassEmblemRitem != nullptr && classEmblemMat != nullptr)
    {
        mClassEmblemRitem->Mat = classEmblemMat;
        mClassEmblemRitem->Visible = true;
        mClassEmblemRitem->NumFramesDirty = gNumFrameResources;
        classEmblemMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        classEmblemMat->NumFramesDirty = gNumFrameResources;
    }
}

Material* UIManager::GetPotionQuickSlotMaterial(PotionQuickSlot potion) const
{
    switch (potion)
    {
    case PotionQuickSlot::HpSmall:
        return mPotionHpSmallMat;
    case PotionQuickSlot::HpMedium:
        return mPotionHpMediumMat;
    case PotionQuickSlot::MpSmall:
        return mPotionMpSmallMat;
    case PotionQuickSlot::MpMedium:
        return mPotionMpMediumMat;
    case PotionQuickSlot::BattleElixir:
        return mPotionBattleElixirMat;
    case PotionQuickSlot::Empty:
    default:
        return nullptr;
    }
}

void UIManager::UpdatePotionQuickSlotIcons()
{
    for (int i = 0; i < 3; ++i)
    {
        RenderItem* iconRitem = mPotionSlotIconRitems[i];
        if (iconRitem == nullptr)
        {
            continue;
        }

        Material* iconMat = GetPotionQuickSlotMaterial(mPotionQuickSlots[i]);
        if (iconMat == nullptr)
        {
            iconRitem->Visible = false;
            iconRitem->NumFramesDirty = gNumFrameResources;
            continue;
        }

        iconRitem->Mat = iconMat;
        iconRitem->Visible = true;
        iconRitem->ColorMultiplier = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        iconRitem->NumFramesDirty = gNumFrameResources;
        iconMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        iconMat->NumFramesDirty = gNumFrameResources;
    }
}

void UIManager::DrawCooldownOverlay()
{
    const bool hasActiveSkill1Cooldown = mSkill1CooldownWidget.CooldownRatio > 0.001f;
    const bool hasActiveSkill2Cooldown = mSkill2CooldownWidget.CooldownRatio > 0.001f;
    const bool hasActiveDashCooldown = mDashCooldownWidget.CooldownRatio > 0.001f;
    bool hasActivePotionCooldown = false;
    for (const auto& potionWidget : mPotionCooldownWidgets)
    {
        hasActivePotionCooldown = hasActivePotionCooldown || potionWidget.CooldownRatio > 0.001f;
    }
    const bool hasGoldDisplay = true;
    const bool hasRespawnOverlay = mRespawnScreenActive;
    const bool hasReturnConfirmOverlay = mReturnToVillageConfirmActive;
    const bool hasStageClearOverlay = mStageClearScreenActive;
    const bool hasEclipseTimer = mEclipseTimerActive;
    const bool hasStatsPanel =
        mStatsPanelOpen &&
        !hasStageClearOverlay &&
        !hasReturnConfirmOverlay &&
        !hasRespawnOverlay;

    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        mCooldownTextHeap == nullptr ||
        (!hasActiveSkill1Cooldown &&
            !hasActiveSkill2Cooldown &&
            !hasActiveDashCooldown &&
            !hasActivePotionCooldown &&
            !hasGoldDisplay &&
            !hasEclipseTimer &&
            !hasStatsPanel &&
            !hasReturnConfirmOverlay &&
            !hasRespawnOverlay &&
            !hasStageClearOverlay))
    {
        return;
    }

    auto* cmdList = mGame->GetCommandList();
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
        ID3D12DescriptorHeap* heaps[] = { mCooldownTextHeap->Heap() };
        cmdList->SetDescriptorHeaps(1, heaps);

        mCooldownTextBatch->SetViewport(viewport);
        mCooldownTextBatch->Begin(cmdList);
        if (!hasStageClearOverlay && !hasReturnConfirmOverlay)
        {
            DrawGoldText();
            DrawCooldownWidgetText(mSkill1CooldownWidget);
            DrawCooldownWidgetText(mSkill2CooldownWidget);
            DrawCooldownWidgetText(mDashCooldownWidget);
            for (const auto& potionWidget : mPotionCooldownWidgets)
            {
                DrawCooldownWidgetText(potionWidget);
            }
            DrawKeyHintText();
            DrawRespawnOverlayText();
            DrawEclipseTimerText();
            DrawStatsPanelText();
        }
        DrawReturnToVillageConfirmText();
        DrawStageClearOverlayText();
        mCooldownTextBatch->End();
    }
    catch (const std::exception& e)
    {
        std::string log = "[UIManager] Failed to draw cooldown text: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());
    }
}

void UIManager::DrawGoldText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const float textScale = GetGoldTextUiScale(viewport);
    const float finalScale = kGoldTextScale * textScale;
    const std::wstring label = L"GOLD  " + FormatGoldAmount(mCurrentGold);
    const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(label.c_str());
    const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
    const float topMargin = (mEclipseTimerActive ? kGoldTextTopMarginWithTimer : kGoldTextTopMargin) * textScale;
    const float rightMargin = kGoldTextRightMargin * textScale;
    const DirectX::XMFLOAT2 textPos(
        viewport.Width - textWidth - rightMargin,
        topMargin);

    const DirectX::XMVECTORF32 shadowColor = { 0.0f, 0.0f, 0.0f, 0.84f };
    const DirectX::XMVECTORF32 textColor = { 1.0f, 0.88f, 0.40f, 1.0f };

    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        DirectX::XMFLOAT2(textPos.x + 2.0f, textPos.y + 2.0f),
        shadowColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        finalScale);
    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        textPos,
        textColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        finalScale);
}

void UIManager::DrawStatsPanelText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        !mStatsPanelOpen ||
        mStageClearScreenActive ||
        mReturnToVillageConfirmActive ||
        mRespawnScreenActive)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    if (viewport.Width <= 0.0f || viewport.Height <= 0.0f)
    {
        return;
    }

    const float uiScale = GetResponsiveTextScale(viewport, 0.82f, 1.35f);
    const float panelLeft = (kStatsPanelCenterX - kStatsPanelScaleX + 1.0f) * 0.5f * viewport.Width;
    const float panelTop = (1.0f - (kStatsPanelCenterY + kStatsPanelScaleY)) * 0.5f * viewport.Height;
    const float panelWidth = kStatsPanelScaleX * viewport.Width;
    const float left = panelLeft + kStatsPanelTextLeftMargin * uiScale;
    const float right = panelLeft + panelWidth - kStatsPanelValueRightMargin * uiScale;
    float y = panelTop + kStatsPanelTextTopMargin * uiScale;

    const DirectX::XMVECTORF32 shadowColor = { 0.0f, 0.0f, 0.0f, 0.82f };
    const DirectX::XMVECTORF32 titleColor = { 0.96f, 0.82f, 0.52f, 1.0f };
    const DirectX::XMVECTORF32 labelColor = { 0.72f, 0.76f, 0.82f, 1.0f };
    const DirectX::XMVECTORF32 valueColor = { 0.94f, 0.96f, 1.0f, 1.0f };

    const auto drawText = [&](const std::wstring& text, const DirectX::XMFLOAT2& pos, const DirectX::XMVECTORF32& color, float scale)
    {
        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            DirectX::XMFLOAT2(pos.x + 2.0f, pos.y + 2.0f),
            shadowColor,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            scale);
        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            pos,
            color,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            scale);
    };

    const auto drawPair = [&](const std::wstring& label, const std::wstring& value)
    {
        const float rowScale = kStatsPanelTextScale * uiScale;
        drawText(label, DirectX::XMFLOAT2(left, y), labelColor, rowScale);

        const DirectX::XMVECTOR valueSize = mCooldownTextFont->MeasureString(value.c_str());
        const float valueWidth = DirectX::XMVectorGetX(valueSize) * rowScale;
        drawText(value, DirectX::XMFLOAT2(right - valueWidth, y), valueColor, rowScale);
        y += kStatsPanelRowHeight * uiScale;
    };

    drawText(L"상태", DirectX::XMFLOAT2(left, panelTop + 27.0f * uiScale), titleColor, kStatsPanelTitleScale * uiScale);

    const std::wstring expLabel = mExperienceToNextLevel > 0
        ? std::to_wstring(mCurrentExperience) + L" / " + std::to_wstring(mCurrentExperience + mExperienceToNextLevel)
        : L"최대";

    drawPair(L"직업", GetPlayerClassName(mCurrentPlayerClass));
    drawPair(L"레벨", std::to_wstring(mCurrentLevel));
    drawPair(L"경험치", expLabel);
    y += kStatsPanelSectionGap * uiScale;
    drawPair(L"체력", FormatStatNumber(mCurrentHp) + L" / " + FormatStatNumber(mCurrentMaxHp));
    drawPair(L"마나", FormatStatNumber(mCurrentMp) + L" / " + FormatStatNumber(mCurrentMaxMp));
    y += kStatsPanelSectionGap * uiScale;
    drawPair(L"공격력", FormatStatNumber(mCurrentPlayerStats.AttackPower));
    drawPair(L"마력", FormatStatNumber(mCurrentPlayerStats.MagicPower));
    drawPair(L"방어력", FormatStatNumber(mCurrentPlayerStats.Defense));
    drawPair(L"공격 속도", FormatStatNumber(mCurrentPlayerStats.AttackSpeed));
    drawPair(L"이동 속도", FormatStatNumber(mCurrentPlayerStats.MoveSpeed));
}

void UIManager::DrawKeyHintText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    if (viewport.Width <= 0.0f || viewport.Height <= 0.0f)
    {
        return;
    }

    const float textScale = GetResponsiveTextScale(viewport, 0.80f, 1.45f);
    const float finalScale = kKeyHintTextScale * textScale;
    const DirectX::XMVECTORF32 shadowColor = { 0.0f, 0.0f, 0.0f, 0.82f };
    const DirectX::XMVECTORF32 textColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    const auto drawCentered = [&](const std::wstring& label, float ndcX, float ndcY)
    {
        const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(label.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * finalScale;
        const float centerX = (ndcX + 1.0f) * 0.5f * viewport.Width;
        const float centerY = (1.0f - ndcY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            centerX - textWidth * 0.5f,
            centerY - textHeight * 0.5f);

        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            label.c_str(),
            DirectX::XMFLOAT2(textPos.x + 1.0f, textPos.y + 1.0f),
            shadowColor,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            label.c_str(),
            textPos,
            textColor,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
    };

    drawCentered(L"Q", mSkill1CooldownWidget.CenterX, mSkill1CooldownWidget.CenterY + kSkillKeyHintOffsetY);
    drawCentered(L"E", mSkill2CooldownWidget.CenterX, mSkill2CooldownWidget.CenterY + kSkillKeyHintOffsetY);

    for (int i = 0; i < 3; ++i)
    {
        drawCentered(
            std::to_wstring(i + 1),
            mPotionCooldownWidgets[i].CenterX,
            mPotionCooldownWidgets[i].CenterY + kPotionKeyHintOffsetY);
    }
}

void UIManager::DrawCooldownWidgetText(const CooldownWidget& widget)
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        widget.CooldownRatio <= 0.001f)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const float textScale = GetResponsiveTextScale(viewport);
    const int cooldownSeconds = (std::max)(1, static_cast<int>(std::ceil(widget.CooldownRemaining)));
    const std::wstring label = std::to_wstring(cooldownSeconds);
    const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(label.c_str());
    const float finalScale = kDashCooldownTextScale * textScale;
    const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
    const float textHeight = DirectX::XMVectorGetY(textSize) * finalScale;
    const float centerX = (widget.CenterX + 1.0f) * 0.5f * viewport.Width;
    const float centerY = (1.0f - widget.CenterY) * 0.5f * viewport.Height;
    const DirectX::XMFLOAT2 textPos(
        centerX - textWidth * 0.5f,
        centerY - textHeight * 0.5f - 1.0f);

    const DirectX::XMVECTORF32 shadowColor = { 0.0f, 0.0f, 0.0f, 0.82f };
    const DirectX::XMVECTORF32 textColor = { 0.96f, 0.98f, 1.0f, 1.0f };

    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        DirectX::XMFLOAT2(textPos.x + 1.0f, textPos.y + 1.0f),
        shadowColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        finalScale);
    mCooldownTextFont->DrawString(
        mCooldownTextBatch.get(),
        label.c_str(),
        textPos,
        textColor,
        0.0f,
        DirectX::XMFLOAT2(0.0f, 0.0f),
        finalScale);
}

void UIManager::DrawEclipseTimerText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        !mEclipseTimerActive)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const float textScale = GetGoldTextUiScale(viewport);
    const auto drawCentered = [&](const std::wstring& text,
                                  float ndcY,
                                  float scale,
                                  const DirectX::XMVECTORF32& color,
                                  float shadowOffset)
    {
        const float finalScale = scale * textScale;
        const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(text.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * finalScale;
        const float centerX = (GetEclipseTimerPanelCenterXForViewport(viewport) + 1.0f) * 0.5f * viewport.Width;
        const float centerY = (1.0f - ndcY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            centerX - textWidth * 0.5f,
            centerY - textHeight * 0.5f);

        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            DirectX::XMFLOAT2(textPos.x + shadowOffset, textPos.y + shadowOffset),
            DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.86f },
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            textPos,
            color,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
    };

    const DirectX::XMVECTORF32 labelColor = { 0.74f, 0.80f, 0.92f, 1.0f };
    const DirectX::XMVECTORF32 normalTimeColor = { 1.0f, 0.86f, 0.56f, 1.0f };
    const DirectX::XMVECTORF32 urgentTimeColor = { 1.0f, 0.28f, 0.18f, 1.0f };
    const DirectX::XMVECTORF32& timeColor =
        mEclipseTimerRemainingSeconds <= 30.0f ? urgentTimeColor : normalTimeColor;

    drawCentered(L"ECLIPSE", kEclipseTimerLabelY, kEclipseTimerLabelScale, labelColor, 1.0f);
    drawCentered(
        FormatEclipseCountdownTime(mEclipseTimerRemainingSeconds),
        kEclipseTimerTimeY,
        kEclipseTimerTimeScale,
        timeColor,
        1.5f);
}

void UIManager::DrawRespawnOverlayText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        !mRespawnScreenActive)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const float textScale = GetResponsiveTextScale(viewport);
    const auto drawCentered = [&](const std::wstring& text,
                                 float ndcY,
                                 float scale,
                                 const DirectX::XMVECTORF32& color,
                                 bool drawShadow = true)
    {
        const float finalScale = scale * textScale;
        const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(text.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * finalScale;
        const float centerX = viewport.Width * 0.5f;
        const float centerY = (1.0f - ndcY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            centerX - textWidth * 0.5f,
            centerY - textHeight * 0.5f);

        if (drawShadow)
        {
            mCooldownTextFont->DrawString(
                mCooldownTextBatch.get(),
                text.c_str(),
                DirectX::XMFLOAT2(textPos.x + 2.0f, textPos.y + 2.0f),
                DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.78f },
                0.0f,
                DirectX::XMFLOAT2(0.0f, 0.0f),
                finalScale);
        }
        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            textPos,
            color,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
    };

    drawCentered(L"사망", kRespawnTitleY, kRespawnTitleScale, DirectX::XMVECTORF32{ 1.0f, 1.0f, 1.0f, 1.0f });

    drawCentered(
        L"부활",
        kRespawnButtonCenterY,
        kRespawnButtonTextScale,
        mRespawnButtonEnabled
            ? DirectX::XMVECTORF32{ 0.14f, 0.10f, 0.04f, 1.0f }
            : DirectX::XMVECTORF32{ 0.30f, 0.24f, 0.14f, 1.0f },
        false);

    if (!mRespawnButtonEnabled)
    {
        const int secondsRemaining = (std::max)(1, static_cast<int>(std::ceil(mRespawnCountdownRemaining)));
        const std::wstring countdownText = L"부활까지 " + std::to_wstring(secondsRemaining) + L"초";
        drawCentered(countdownText, kRespawnCountdownY, kRespawnCountdownScale, DirectX::XMVECTORF32{ 0.88f, 0.88f, 0.90f, 1.0f });
    }
}

void UIManager::DrawReturnToVillageConfirmText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        !mReturnToVillageConfirmActive)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const float textScale = GetResponsiveTextScale(viewport);
    const auto drawCentered = [&](const std::wstring& text,
                                  float ndcX,
                                  float ndcY,
                                  float scale,
                                  const DirectX::XMVECTORF32& color,
                                  bool drawShadow = true)
    {
        const float finalScale = scale * textScale;
        const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(text.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * finalScale;
        const float centerX = (ndcX + 1.0f) * 0.5f * viewport.Width;
        const float centerY = (1.0f - ndcY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            centerX - textWidth * 0.5f,
            centerY - textHeight * 0.5f);

        if (drawShadow)
        {
            mCooldownTextFont->DrawString(
                mCooldownTextBatch.get(),
                text.c_str(),
                DirectX::XMFLOAT2(textPos.x + 2.0f, textPos.y + 2.0f),
                DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.80f },
                0.0f,
                DirectX::XMFLOAT2(0.0f, 0.0f),
                finalScale);
        }

        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            textPos,
            color,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
    };

    drawCentered(
        L"마을로 귀환할까요?",
        kReturnConfirmPanelCenterX,
        kReturnConfirmTitleY,
        kReturnConfirmTitleScale,
        DirectX::XMVECTORF32{ 1.0f, 0.94f, 0.76f, 1.0f });
    drawCentered(
        L"현재 스테이지 진행 상황은 저장되지 않습니다.",
        kReturnConfirmPanelCenterX,
        kReturnConfirmSubtitleY,
        kReturnConfirmSubtitleScale,
        DirectX::XMVECTORF32{ 0.82f, 0.84f, 0.88f, 1.0f });
    drawCentered(
        L"예",
        kReturnConfirmYesButtonX,
        kReturnConfirmButtonY,
        kReturnConfirmButtonTextScale,
        DirectX::XMVECTORF32{ 1.0f, 0.90f, 0.62f, 1.0f },
        false);
    drawCentered(
        L"아니요",
        kReturnConfirmNoButtonX,
        kReturnConfirmButtonY,
        kReturnConfirmButtonTextScale,
        DirectX::XMVECTORF32{ 0.88f, 0.90f, 0.96f, 1.0f },
        false);
}

void UIManager::DrawStageClearOverlayText()
{
    if (mGame == nullptr ||
        mCooldownTextFont == nullptr ||
        mCooldownTextBatch == nullptr ||
        !mStageClearScreenActive)
    {
        return;
    }

    const auto viewport = mGame->GetScreenViewport();
    const float textScale = GetResponsiveTextScale(viewport);
    const auto drawCentered = [&](const std::wstring& text,
                                  float ndcY,
                                  float scale,
                                  const DirectX::XMVECTORF32& color,
                                  bool drawShadow = true)
    {
        const float finalScale = scale * textScale;
        const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(text.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * finalScale;
        const float centerX = viewport.Width * 0.5f;
        const float centerY = (1.0f - ndcY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            centerX - textWidth * 0.5f,
            centerY - textHeight * 0.5f);

        if (drawShadow)
        {
            mCooldownTextFont->DrawString(
                mCooldownTextBatch.get(),
                text.c_str(),
                DirectX::XMFLOAT2(textPos.x + 2.0f, textPos.y + 2.0f),
                DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.82f },
                0.0f,
                DirectX::XMFLOAT2(0.0f, 0.0f),
                finalScale);
        }

        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            textPos,
            color,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
    };

    const auto drawAt = [&](const std::wstring& text,
                            float ndcX,
                            float ndcY,
                            float scale,
                            const DirectX::XMVECTORF32& color,
                            bool centered)
    {
        const float finalScale = scale * textScale;
        const DirectX::XMVECTOR textSize = mCooldownTextFont->MeasureString(text.c_str());
        const float textWidth = DirectX::XMVectorGetX(textSize) * finalScale;
        const float textHeight = DirectX::XMVectorGetY(textSize) * finalScale;
        const float anchorX = (ndcX + 1.0f) * 0.5f * viewport.Width;
        const float anchorY = (1.0f - ndcY) * 0.5f * viewport.Height;
        const DirectX::XMFLOAT2 textPos(
            centered ? (anchorX - textWidth * 0.5f) : anchorX,
            anchorY - textHeight * 0.5f);

        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            DirectX::XMFLOAT2(textPos.x + 1.0f, textPos.y + 1.0f),
            DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.78f },
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
        mCooldownTextFont->DrawString(
            mCooldownTextBatch.get(),
            text.c_str(),
            textPos,
            color,
            0.0f,
            DirectX::XMFLOAT2(0.0f, 0.0f),
            finalScale);
    };

    const auto trimText = [](const std::wstring& text, size_t maxLength)
    {
        if (text.size() <= maxLength)
        {
            return text;
        }

        if (maxLength <= 3)
        {
            return text.substr(0, maxLength);
        }

        return text.substr(0, maxLength - 3) + L"...";
    };

    if (mStageGameOverScreenActive)
    {
        drawCentered(L"TOTAL ECLIPSE", kStageClearTimeAboveTitleY, kStageClearTimeScale, DirectX::XMVECTORF32{ 0.74f, 0.78f, 0.90f, 1.0f });
        drawCentered(L"GAME OVER", kStageClearTitleY, kStageClearTitleScale, DirectX::XMVECTORF32{ 0.82f, 0.05f, 0.04f, 1.0f }, false);
        drawCentered(L"The eclipse has swallowed the world.", kStageClearSubtitleY, kStageClearSubtitleScale, DirectX::XMVECTORF32{ 0.92f, 0.88f, 0.82f, 1.0f });
        drawAt(
            L"END GAME",
            kStageClearButtonCenterX,
            kStageClearButtonCenterY,
            kStageClearButtonTextScale,
            DirectX::XMVECTORF32{ 1.0f, 0.76f, 0.60f, 1.0f },
            true);
        return;
    }

    if (!mStageClearRecordsView)
    {
        drawCentered(FormatClearTimeLabel(mStageClearTimeSeconds), kStageClearTimeAboveTitleY, kStageClearTimeScale, DirectX::XMVECTORF32{ 1.0f, 0.94f, 0.74f, 1.0f });
        drawCentered(L"STAGE CLEAR", kStageClearTitleY, kStageClearTitleScale, DirectX::XMVECTORF32{ 0.12f, 0.08f, 0.02f, 1.0f }, false);

        if (mStageClearCurrentRecordRank > 0)
        {
            drawCentered(
                L"현재 기록 순위  #" + std::to_wstring(mStageClearCurrentRecordRank),
                0.155f,
                0.44f,
                DirectX::XMVECTORF32{ 0.76f, 0.88f, 1.0f, 1.0f });
        }

        drawAt(L"플레이어", -0.18f, kStageClearHeaderY, kStageClearHeaderScale, DirectX::XMVECTORF32{ 0.88f, 0.84f, 0.72f, 1.0f }, true);
        drawAt(L"보스 피해량", 0.24f, kStageClearHeaderY, kStageClearHeaderScale, DirectX::XMVECTORF32{ 0.88f, 0.84f, 0.72f, 1.0f }, true);

        if (mStageClearEntries.empty())
        {
            drawCentered(L"딜량 기록 없음", -0.065f, kStageClearRowScale, DirectX::XMVECTORF32{ 0.72f, 0.76f, 0.82f, 1.0f });
        }

        for (size_t i = 0; i < mStageClearEntries.size(); ++i)
        {
            const float rowY = kStageClearFirstRowY - static_cast<float>(i) * kStageClearRowStepY;
            const StageClearEntry& entry = mStageClearEntries[i];
            const std::wstring damageText = std::to_wstring((std::max)(0, entry.Damage));
            drawAt(
                trimText(entry.Name.empty() ? L"Player" : entry.Name, 18),
                -0.18f,
                rowY,
                kStageClearRowScale,
                DirectX::XMVECTORF32{ 0.96f, 0.96f, 0.98f, 1.0f },
                true);
            drawAt(
                damageText,
                0.24f,
                rowY,
                kStageClearRowScale,
                DirectX::XMVECTORF32{ 1.0f, 0.84f, 0.48f, 1.0f },
                true);
        }

        drawAt(
            L"NEXT",
            kStageClearButtonCenterX,
            kStageClearButtonCenterY,
            kStageClearButtonTextScale,
            DirectX::XMVECTORF32{ 1.0f, 0.90f, 0.58f, 1.0f },
            true);
        return;
    }

    drawCentered(L"RECORDS", kStageClearTitleY, kStageClearTitleScale, DirectX::XMVECTORF32{ 0.12f, 0.08f, 0.02f, 1.0f }, false);

    std::wstring currentSummary = L"CURRENT  " + FormatClearTimeShort(mStageClearTimeSeconds);
    if (mStageClearCurrentRecordRank > 0)
    {
        currentSummary += L"  #" + std::to_wstring(mStageClearCurrentRecordRank);
    }
    drawCentered(currentSummary, kStageClearSubtitleY, kStageClearSubtitleScale, DirectX::XMVECTORF32{ 1.0f, 0.94f, 0.74f, 1.0f });

    if (mStageClearRecords.empty())
    {
        drawCentered(L"저장된 기록 없음", 0.02f, 0.62f, DirectX::XMVECTORF32{ 0.76f, 0.80f, 0.86f, 1.0f });
    }
    else
    {
        drawAt(L"순위", -0.50f, kStageClearRecordHeaderY, kStageClearRecordHeaderScale, DirectX::XMVECTORF32{ 0.88f, 0.84f, 0.72f, 1.0f }, true);
        drawAt(L"시간", -0.33f, kStageClearRecordHeaderY, kStageClearRecordHeaderScale, DirectX::XMVECTORF32{ 0.88f, 0.84f, 0.72f, 1.0f }, true);
        drawAt(L"MVP", -0.13f, kStageClearRecordHeaderY, kStageClearRecordHeaderScale, DirectX::XMVECTORF32{ 0.88f, 0.84f, 0.72f, 1.0f }, true);
        drawAt(L"파티", 0.06f, kStageClearRecordHeaderY, kStageClearRecordHeaderScale, DirectX::XMVECTORF32{ 0.88f, 0.84f, 0.72f, 1.0f }, false);

        const size_t maxRows = (std::min)(mStageClearRecords.size(), static_cast<size_t>(MAX_GAME_RECORDS));
        for (size_t i = 0; i < maxRows; ++i)
        {
            const StageClearRecordEntry& entry = mStageClearRecords[i];
            const float rowY = kStageClearRecordFirstRowY - static_cast<float>(i) * kStageClearRecordRowStepY;
            const DirectX::XMVECTORF32 rowColor =
                (entry.Rank == mStageClearCurrentRecordRank && mStageClearCurrentRecordRank > 0)
                ? DirectX::XMVECTORF32{ 1.0f, 0.90f, 0.52f, 1.0f }
                : DirectX::XMVECTORF32{ 0.94f, 0.96f, 0.98f, 1.0f };

            drawAt(L"#" + std::to_wstring(entry.Rank), -0.50f, rowY, kStageClearRecordRowScale, rowColor, true);
            drawAt(FormatClearTimeShort(entry.ClearTimeSeconds), -0.33f, rowY, kStageClearRecordRowScale, rowColor, true);
            drawAt(trimText(entry.TopDealerName.empty() ? L"-" : entry.TopDealerName, 9), -0.13f, rowY, kStageClearRecordRowScale, rowColor, true);
            drawAt(trimText(entry.PartySummary.empty() ? L"-" : entry.PartySummary, 34), 0.06f, rowY, kStageClearRecordRowScale, rowColor, false);
        }
    }

    drawAt(
        L"END GAME",
        kStageClearButtonCenterX,
        kStageClearButtonCenterY,
        kStageClearButtonTextScale,
        DirectX::XMVECTORF32{ 1.0f, 0.90f, 0.58f, 1.0f },
        true);
}

void UIManager::UpdateBossHealthBar(float currentHp, float maxHp)
{
    if (maxHp <= 0.0f || currentHp <= 0.0f)
    {
        HideBossHealthBar();
        return;
    }

    constexpr int kBossHpLayerCount = 200;
    const float clampedHp = (std::clamp)(currentHp, 0.0f, maxHp);
    const float hpPerLayer = maxHp / static_cast<float>(kBossHpLayerCount);
    const int visibleLayer = (std::clamp)(
        static_cast<int>(std::ceil(clampedHp / hpPerLayer)),
        1,
        kBossHpLayerCount);
    const float layerBaseHp = hpPerLayer * static_cast<float>(visibleLayer - 1);
    const float layerHp = clampedHp - layerBaseHp;
    const float layerRatio = (std::clamp)(layerHp / hpPerLayer, 0.0f, 1.0f);

    if (visibleLayer != mBossHpVisibleLayer)
    {
        mBossHpVisibleLayer = visibleLayer;
        mBossHpDelayRatio = 1.0f;
    }
    else if (layerRatio > mBossHpDelayRatio)
    {
        mBossHpDelayRatio = layerRatio;
    }
    else
    {
        mBossHpDelayRatio += (layerRatio - mBossHpDelayRatio) * 0.055f;
    }

    const DirectX::XMFLOAT4 nonFinalLayerColors[] =
    {
        { 0.17f, 0.62f, 0.92f, 1.0f },
        { 0.42f, 0.25f, 0.95f, 1.0f },
        { 0.84f, 0.16f, 0.76f, 1.0f },
        { 0.88f, 0.58f, 0.06f, 1.0f },
        { 0.92f, 0.22f, 0.05f, 1.0f }
    };
    constexpr int nonFinalPaletteCount = static_cast<int>(sizeof(nonFinalLayerColors) / sizeof(nonFinalLayerColors[0]));
    const DirectX::XMFLOAT4 fillColor = (visibleLayer == 1)
        ? DirectX::XMFLOAT4{ 0.78f, 0.025f, 0.04f, 1.0f }
        : nonFinalLayerColors[(kBossHpLayerCount - visibleLayer) % nonFinalPaletteCount];
    const DirectX::XMFLOAT4 nextLayerColor = (visibleLayer <= 1)
        ? DirectX::XMFLOAT4{ 0.09f, 0.012f, 0.016f, 0.94f }
        : ((visibleLayer - 1 == 1)
            ? DirectX::XMFLOAT4{ 0.78f, 0.025f, 0.04f, 1.0f }
            : nonFinalLayerColors[(kBossHpLayerCount - (visibleLayer - 1)) % nonFinalPaletteCount]);
    if (mBossHpBackMat != nullptr)
    {
        mBossHpBackMat->DiffuseAlbedo = nextLayerColor;
        mBossHpBackMat->NumFramesDirty = gNumFrameResources;
    }
    if (mBossHpFillMat != nullptr)
    {
        mBossHpFillMat->DiffuseAlbedo = fillColor;
        mBossHpFillMat->NumFramesDirty = gNumFrameResources;
    }
    if (mBossHpDelayMat != nullptr)
    {
        mBossHpDelayMat->DiffuseAlbedo = {
            (std::min)(fillColor.x + 0.24f, 1.0f),
            (std::min)(fillColor.y + 0.24f, 1.0f),
            (std::min)(fillColor.z + 0.18f, 1.0f),
            0.90f
        };
        mBossHpDelayMat->NumFramesDirty = gNumFrameResources;
    }
    if (mBossHpGlossMat != nullptr)
    {
        mBossHpGlossMat->DiffuseAlbedo = {
            (std::min)(fillColor.x + 0.35f, 1.0f),
            (std::min)(fillColor.y + 0.35f, 1.0f),
            (std::min)(fillColor.z + 0.30f, 1.0f),
            0.34f
        };
        mBossHpGlossMat->NumFramesDirty = gNumFrameResources;
    }

    auto setVisible = [](GameObject* object, bool visible)
        {
            if (object != nullptr && object->Ritem != nullptr)
            {
                object->Ritem->Visible = visible;
            }
        };

    setVisible(mBossHpFrame, true);
    setVisible(mBossHpBack, true);
    setVisible(mBossHpDelay, true);
    setVisible(mBossHpFill, true);
    setVisible(mBossHpGloss, true);

    auto updateBar = [](GameObject* bar, float ratio, float maxScaleX, float scaleY, float leftEdgeX, float y, float z)
        {
            if (bar == nullptr)
            {
                return;
            }

            const float currentScale = maxScaleX * (std::max)(ratio, 0.0f);
            bar->SetScale(currentScale, scaleY, 1.0f);
            bar->SetPosition(leftEdgeX + currentScale, y, z);
            bar->Update();
        };

    constexpr float bossBarLeftEdgeX = -kBossBarFillMaxScaleX;

    updateBar(mBossHpDelay, mBossHpDelayRatio, kBossBarFillMaxScaleX, kBossBarFillScaleY, bossBarLeftEdgeX, kBossBarY, 0.095f);
    updateBar(mBossHpFill, layerRatio, kBossBarFillMaxScaleX, kBossBarFillScaleY, bossBarLeftEdgeX, kBossBarY, 0.090f);
    updateBar(mBossHpGloss, layerRatio, kBossBarFillMaxScaleX, kBossBarGlossScaleY, bossBarLeftEdgeX, kBossBarY + 0.006f, 0.085f);

    if (mBossHpFrame) mBossHpFrame->Update();
    if (mBossHpBack) mBossHpBack->Update();
}

void UIManager::HideBossHealthBar()
{
    mBossHpDelayRatio = 1.0f;
    mBossHpVisibleLayer = 0;

    GameObject* bossBarObjects[] =
    {
        mBossHpFrame,
        mBossHpBack,
        mBossHpDelay,
        mBossHpFill,
        mBossHpGloss,
        mBossHpLeftCap,
        mBossHpRightCap
    };

    for (GameObject* object : bossBarObjects)
    {
        if (object != nullptr && object->Ritem != nullptr)
        {
            object->Ritem->Visible = false;
            object->Update();
        }
    }

}

void UIManager::ShowMirrorCrackWarning(float progress)
{
    progress = (std::clamp)(progress, 0.0f, 1.0f);
    mMirrorCrackWarningActive = true;
    mMirrorCrackWarningProgress = progress;

    if (mGame != nullptr)
    {
        mGame->SetMirrorBreakEffect(progress);
    }

    const float pulse = 0.5f + 0.5f * std::sin(mMirrorCrackWarningTime * 12.0f);
    const float alpha = (std::clamp)(0.14f + progress * 0.58f + pulse * 0.08f, 0.0f, 0.84f);

    if (mMirrorCrackMat != nullptr)
    {
        mMirrorCrackMat->DiffuseAlbedo = DirectX::XMFLOAT4(
            0.72f + pulse * 0.18f,
            0.92f + pulse * 0.08f,
            1.0f,
            alpha);
        mMirrorCrackMat->NumFramesDirty = gNumFrameResources;
    }

    if (mMirrorCrackObj != nullptr)
    {
        mMirrorCrackObj->SetScale(0.0f, 0.0f, 1.0f);
        if (mMirrorCrackObj->Ritem != nullptr)
        {
            mMirrorCrackObj->Ritem->Visible = false;
            mMirrorCrackObj->Ritem->NumFramesDirty = gNumFrameResources;
        }
        mMirrorCrackObj->Update();
    }
}

void UIManager::HideMirrorCrackWarning()
{
    mMirrorCrackWarningActive = false;
    mMirrorCrackWarningProgress = 0.0f;
    mMirrorCrackWarningTime = 0.0f;

    if (mGame != nullptr)
    {
        mGame->ClearMirrorBreakEffect();
    }

    if (mMirrorCrackMat != nullptr)
    {
        mMirrorCrackMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.82f, 0.96f, 1.0f, 0.0f);
        mMirrorCrackMat->NumFramesDirty = gNumFrameResources;
    }

    if (mMirrorCrackObj != nullptr)
    {
        mMirrorCrackObj->SetScale(0.0f, 0.0f, 1.0f);
        if (mMirrorCrackObj->Ritem != nullptr)
        {
            mMirrorCrackObj->Ritem->Visible = false;
            mMirrorCrackObj->Ritem->NumFramesDirty = gNumFrameResources;
        }
        mMirrorCrackObj->Update();
    }
}

void UIManager::SetEclipseTimerState(bool active, float remainingSeconds, float progressRatio)
{
    mEclipseTimerActive = active;
    mEclipseTimerRemainingSeconds = (std::max)(0.0f, remainingSeconds);
    mEclipseTimerProgressRatio = (std::clamp)(progressRatio, 0.0f, 1.0f);
    const auto viewport = mGame != nullptr ? mGame->GetScreenViewport() : D3D12_VIEWPORT{};
    const float eclipseTimerPanelCenterX = GetEclipseTimerPanelCenterXForViewport(viewport);
    const float eclipseTimerProgressCenterX = GetEclipseTimerProgressCenterXForViewport(viewport);

    auto setVisible = [active](GameObject* object)
    {
        if (object != nullptr && object->Ritem != nullptr)
        {
            object->Ritem->Visible = active;
            object->Ritem->NumFramesDirty = gNumFrameResources;
        }
    };

    setVisible(mEclipseTimerPanelBg);
    setVisible(mEclipseTimerProgressBack);
    setVisible(mEclipseTimerProgressFill);

    if (mEclipseTimerPanelMat != nullptr)
    {
        mEclipseTimerPanelMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.018f, 0.020f, 0.026f, 0.80f)
            : DirectX::XMFLOAT4(0.018f, 0.020f, 0.026f, 0.0f);
        mEclipseTimerPanelMat->NumFramesDirty = gNumFrameResources;
    }
    if (mEclipseTimerProgressBackMat != nullptr)
    {
        mEclipseTimerProgressBackMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.07f, 0.07f, 0.085f, 0.90f)
            : DirectX::XMFLOAT4(0.07f, 0.07f, 0.085f, 0.0f);
        mEclipseTimerProgressBackMat->NumFramesDirty = gNumFrameResources;
    }
    if (mEclipseTimerProgressFillMat != nullptr)
    {
        const float warningRatio = (std::clamp)((mEclipseTimerProgressRatio - 0.75f) / 0.25f, 0.0f, 1.0f);
        mEclipseTimerProgressFillMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(
                0.86f + 0.14f * warningRatio,
                0.64f - 0.32f * warningRatio,
                0.30f - 0.18f * warningRatio,
                0.96f)
            : DirectX::XMFLOAT4(0.86f, 0.64f, 0.30f, 0.0f);
        mEclipseTimerProgressFillMat->NumFramesDirty = gNumFrameResources;
    }

    if (mEclipseTimerProgressFill != nullptr)
    {
        const float currentScale = kEclipseTimerProgressMaxScaleX * mEclipseTimerProgressRatio;
        const float leftEdgeX = eclipseTimerProgressCenterX - kEclipseTimerProgressMaxScaleX;
        mEclipseTimerProgressFill->SetScale(currentScale, kEclipseTimerProgressScaleY, 1.0f);
        mEclipseTimerProgressFill->SetPosition(leftEdgeX + currentScale, kEclipseTimerProgressY, mEclipseTimerProgressFill->GetPosition().z);
        mEclipseTimerProgressFill->Update();
    }
    if (mEclipseTimerPanelBg != nullptr)
    {
        mEclipseTimerPanelBg->SetPosition(eclipseTimerPanelCenterX, kEclipseTimerPanelCenterY, mEclipseTimerPanelBg->GetPosition().z);
        mEclipseTimerPanelBg->Update();
    }
    if (mEclipseTimerProgressBack != nullptr)
    {
        mEclipseTimerProgressBack->SetPosition(eclipseTimerProgressCenterX, kEclipseTimerProgressY, mEclipseTimerProgressBack->GetPosition().z);
        mEclipseTimerProgressBack->Update();
    }
}

void UIManager::SetCutsceneFadeAlpha(float alpha)
{
    const float clampedAlpha = (std::clamp)(alpha, 0.0f, 1.0f);
    const bool visible = clampedAlpha > 0.001f;

    if (mBgMat != nullptr)
    {
        mBgMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, clampedAlpha);
        mBgMat->NumFramesDirty = gNumFrameResources;
    }

    if (mFlashMat != nullptr)
    {
        mFlashMat->DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, clampedAlpha);
        mFlashMat->NumFramesDirty = gNumFrameResources;
    }

    if (mScreenBgObj != nullptr)
    {
        mScreenBgObj->SetScale(visible ? 1.05f : 0.0f, visible ? 1.05f : 0.0f, 1.0f);
        mScreenBgObj->SetPosition(0.0f, 0.0f, 0.18f);
        mScreenBgObj->Update();
    }

    if (mFlashObj != nullptr)
    {
        mFlashObj->SetScale(visible ? 1.35f : 0.0f, visible ? 1.35f : 0.0f, 1.0f);
        mFlashObj->SetPosition(0.0f, 0.0f, 0.12f);
        mFlashObj->Update();
    }
}

void UIManager::SetChatBoxState(bool active, bool hasMessages)
{
    if (mChatLogMat)
    {
        mChatLogMat->DiffuseAlbedo = hasMessages
            ? DirectX::XMFLOAT4(0.04f, 0.05f, 0.07f, 0.84f)
            : DirectX::XMFLOAT4(0.04f, 0.05f, 0.07f, 0.72f);
        mChatLogMat->NumFramesDirty = 3;
    }

    if (mChatInputMat)
    {
        mChatInputMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.16f, 0.12f, 0.05f, 0.94f)
            : DirectX::XMFLOAT4(0.1f, 0.12f, 0.16f, 0.9f);
        mChatInputMat->NumFramesDirty = 3;
    }

    if (mChatLogBg) mChatLogBg->Update();
    if (mChatInputBg) mChatInputBg->Update();
}

void UIManager::SetReturnToVillageConfirmState(bool active)
{
    mReturnToVillageConfirmActive = active;

    GameObject* objects[] =
    {
        mReturnConfirmOverlayBg,
        mReturnConfirmPanelFrame,
        mReturnConfirmPanelBg,
        mReturnConfirmYesButtonFrame,
        mReturnConfirmYesButtonBg,
        mReturnConfirmNoButtonFrame,
        mReturnConfirmNoButtonBg
    };

    for (GameObject* object : objects)
    {
        if (object != nullptr && object->Ritem != nullptr)
        {
            object->Ritem->Visible = active;
            object->Ritem->NumFramesDirty = gNumFrameResources;
            object->Update();
        }
    }

    if (mReturnConfirmOverlayMat != nullptr)
    {
        mReturnConfirmOverlayMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.58f)
            : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        mReturnConfirmOverlayMat->NumFramesDirty = gNumFrameResources;
    }
    if (mReturnConfirmPanelMat != nullptr)
    {
        mReturnConfirmPanelMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.045f, 0.050f, 0.062f, 0.96f)
            : DirectX::XMFLOAT4(0.045f, 0.050f, 0.062f, 0.0f);
        mReturnConfirmPanelMat->NumFramesDirty = gNumFrameResources;
    }
    if (mReturnConfirmPanelFrameMat != nullptr)
    {
        mReturnConfirmPanelFrameMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.78f, 0.62f, 0.30f, 0.98f)
            : DirectX::XMFLOAT4(0.78f, 0.62f, 0.30f, 0.0f);
        mReturnConfirmPanelFrameMat->NumFramesDirty = gNumFrameResources;
    }
    if (mReturnConfirmYesButtonMat != nullptr)
    {
        mReturnConfirmYesButtonMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.28f, 0.20f, 0.10f, 0.96f)
            : DirectX::XMFLOAT4(0.28f, 0.20f, 0.10f, 0.0f);
        mReturnConfirmYesButtonMat->NumFramesDirty = gNumFrameResources;
    }
    if (mReturnConfirmYesButtonFrameMat != nullptr)
    {
        mReturnConfirmYesButtonFrameMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.90f, 0.72f, 0.34f, 0.98f)
            : DirectX::XMFLOAT4(0.90f, 0.72f, 0.34f, 0.0f);
        mReturnConfirmYesButtonFrameMat->NumFramesDirty = gNumFrameResources;
    }
    if (mReturnConfirmNoButtonMat != nullptr)
    {
        mReturnConfirmNoButtonMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.12f, 0.13f, 0.16f, 0.96f)
            : DirectX::XMFLOAT4(0.12f, 0.13f, 0.16f, 0.0f);
        mReturnConfirmNoButtonMat->NumFramesDirty = gNumFrameResources;
    }
    if (mReturnConfirmNoButtonFrameMat != nullptr)
    {
        mReturnConfirmNoButtonFrameMat->DiffuseAlbedo = active
            ? DirectX::XMFLOAT4(0.52f, 0.54f, 0.60f, 0.96f)
            : DirectX::XMFLOAT4(0.52f, 0.54f, 0.60f, 0.0f);
        mReturnConfirmNoButtonFrameMat->NumFramesDirty = gNumFrameResources;
    }
}

void UIManager::SetRespawnScreenState(bool active, float countdownRemaining, bool buttonEnabled)
{
    mRespawnScreenActive = active;
    mRespawnCountdownRemaining = (std::max)(0.0f, countdownRemaining);
    mRespawnButtonEnabled = active && buttonEnabled;

    const bool visible = mRespawnScreenActive;
    if (mRespawnOverlayBg != nullptr && mRespawnOverlayBg->Ritem != nullptr)
    {
        mRespawnOverlayBg->Ritem->Visible = visible;
        mRespawnOverlayBg->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mRespawnButtonFrame != nullptr && mRespawnButtonFrame->Ritem != nullptr)
    {
        mRespawnButtonFrame->Ritem->Visible = visible;
        mRespawnButtonFrame->Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mRespawnButtonBg != nullptr && mRespawnButtonBg->Ritem != nullptr)
    {
        mRespawnButtonBg->Ritem->Visible = visible;
        mRespawnButtonBg->Ritem->NumFramesDirty = gNumFrameResources;
    }

    if (mRespawnOverlayMat != nullptr)
    {
        mRespawnOverlayMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.16f, 0.16f, 0.16f, 0.78f)
            : DirectX::XMFLOAT4(0.16f, 0.16f, 0.16f, 0.0f);
        mRespawnOverlayMat->NumFramesDirty = gNumFrameResources;
    }
    if (mRespawnButtonMat != nullptr)
    {
        mRespawnButtonMat->DiffuseAlbedo = mRespawnButtonEnabled
            ? DirectX::XMFLOAT4(0.82f, 0.72f, 0.36f, 0.96f)
            : DirectX::XMFLOAT4(0.24f, 0.24f, 0.26f, 0.88f);
        mRespawnButtonMat->NumFramesDirty = gNumFrameResources;
    }
    if (mRespawnButtonFrameMat != nullptr)
    {
        mRespawnButtonFrameMat->DiffuseAlbedo = mRespawnButtonEnabled
            ? DirectX::XMFLOAT4(0.98f, 0.90f, 0.62f, 1.0f)
            : DirectX::XMFLOAT4(0.52f, 0.52f, 0.56f, 0.92f);
        mRespawnButtonFrameMat->NumFramesDirty = gNumFrameResources;
    }

    if (mRespawnOverlayBg != nullptr) mRespawnOverlayBg->Update();
    if (mRespawnButtonFrame != nullptr) mRespawnButtonFrame->Update();
    if (mRespawnButtonBg != nullptr) mRespawnButtonBg->Update();
}

void UIManager::SetStageClearScreenState(
    bool active,
    float clearTimeSeconds,
    const std::vector<StageClearEntry>& entries,
    const std::vector<StageClearRecordEntry>& records,
    int currentRecordRank)
{
    const bool wasActive = mStageClearScreenActive;
    mStageClearScreenActive = active;
    mStageGameOverScreenActive = false;
    mStageClearRecordsView = false;
    mStageClearTimeSeconds = (std::max)(0.0f, clearTimeSeconds);
    mStageClearEntries = active ? entries : std::vector<StageClearEntry>{};
    mStageClearRecords = active ? records : std::vector<StageClearRecordEntry>{};
    mStageClearCurrentRecordRank = active ? currentRecordRank : 0;

    if (active && !wasActive)
    {
        AudioManager::Get().PlayEffect(kStageClearSound, kStageClearSoundVolume);
    }

    const bool visible = mStageClearScreenActive;
    auto setVisible = [visible](GameObject* object)
    {
        if (object != nullptr && object->Ritem != nullptr)
        {
            object->Ritem->Visible = visible;
            object->Ritem->NumFramesDirty = gNumFrameResources;
            object->Update();
        }
    };

    setVisible(mStageClearOverlayBg);
    setVisible(mStageClearPanelFrame);
    setVisible(mStageClearPanelBg);
    setVisible(mStageClearBannerBg);
    setVisible(mStageClearButtonFrame);
    setVisible(mStageClearButtonBg);

    if (mStageClearOverlayMat != nullptr)
    {
        mStageClearOverlayMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.02f, 0.03f, 0.05f, 0.84f)
            : DirectX::XMFLOAT4(0.02f, 0.03f, 0.05f, 0.0f);
        mStageClearOverlayMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearPanelMat != nullptr)
    {
        mStageClearPanelMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.07f, 0.08f, 0.10f, 0.96f)
            : DirectX::XMFLOAT4(0.07f, 0.08f, 0.10f, 0.0f);
        mStageClearPanelMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearPanelFrameMat != nullptr)
    {
        mStageClearPanelFrameMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.86f, 0.74f, 0.38f, 0.98f)
            : DirectX::XMFLOAT4(0.86f, 0.74f, 0.38f, 0.0f);
        mStageClearPanelFrameMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearBannerMat != nullptr)
    {
        mStageClearBannerMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.76f, 0.60f, 0.20f, 0.98f)
            : DirectX::XMFLOAT4(0.76f, 0.60f, 0.20f, 0.0f);
        mStageClearBannerMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearButtonMat != nullptr)
    {
        mStageClearButtonMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.18f, 0.15f, 0.10f, 0.96f)
            : DirectX::XMFLOAT4(0.18f, 0.15f, 0.10f, 0.0f);
        mStageClearButtonMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearButtonFrameMat != nullptr)
    {
        mStageClearButtonFrameMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.92f, 0.74f, 0.36f, 0.98f)
            : DirectX::XMFLOAT4(0.92f, 0.74f, 0.36f, 0.0f);
        mStageClearButtonFrameMat->NumFramesDirty = gNumFrameResources;
    }
}

void UIManager::SetGameOverScreenState(bool active, float elapsedSeconds)
{
    mStageClearScreenActive = active;
    mStageGameOverScreenActive = active;
    mStageClearRecordsView = false;
    mStageClearTimeSeconds = (std::max)(0.0f, elapsedSeconds);
    mStageClearEntries.clear();
    mStageClearRecords.clear();
    mStageClearCurrentRecordRank = 0;

    const bool visible = mStageClearScreenActive;
    auto setVisible = [visible](GameObject* object)
    {
        if (object != nullptr && object->Ritem != nullptr)
        {
            object->Ritem->Visible = visible;
            object->Ritem->NumFramesDirty = gNumFrameResources;
            object->Update();
        }
    };

    setVisible(mStageClearOverlayBg);
    setVisible(mStageClearPanelFrame);
    setVisible(mStageClearPanelBg);
    setVisible(mStageClearBannerBg);
    setVisible(mStageClearButtonFrame);
    setVisible(mStageClearButtonBg);

    if (mStageClearOverlayMat != nullptr)
    {
        mStageClearOverlayMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.90f)
            : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        mStageClearOverlayMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearPanelMat != nullptr)
    {
        mStageClearPanelMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.025f, 0.025f, 0.035f, 0.98f)
            : DirectX::XMFLOAT4(0.025f, 0.025f, 0.035f, 0.0f);
        mStageClearPanelMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearPanelFrameMat != nullptr)
    {
        mStageClearPanelFrameMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.55f, 0.04f, 0.03f, 0.98f)
            : DirectX::XMFLOAT4(0.55f, 0.04f, 0.03f, 0.0f);
        mStageClearPanelFrameMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearBannerMat != nullptr)
    {
        mStageClearBannerMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.34f, 0.02f, 0.02f, 0.98f)
            : DirectX::XMFLOAT4(0.34f, 0.02f, 0.02f, 0.0f);
        mStageClearBannerMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearButtonMat != nullptr)
    {
        mStageClearButtonMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.08f, 0.06f, 0.06f, 0.96f)
            : DirectX::XMFLOAT4(0.08f, 0.06f, 0.06f, 0.0f);
        mStageClearButtonMat->NumFramesDirty = gNumFrameResources;
    }
    if (mStageClearButtonFrameMat != nullptr)
    {
        mStageClearButtonFrameMat->DiffuseAlbedo = visible
            ? DirectX::XMFLOAT4(0.68f, 0.12f, 0.08f, 0.98f)
            : DirectX::XMFLOAT4(0.68f, 0.12f, 0.08f, 0.0f);
        mStageClearButtonFrameMat->NumFramesDirty = gNumFrameResources;
    }
}

bool UIManager::IsRespawnButtonHovered() const
{
    if (!mRespawnScreenActive || !mRespawnButtonEnabled || mGame == nullptr)
    {
        return false;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
    {
        return false;
    }

    RECT clientRect{};
    if (!GetClientRect(mGame->GetMainWindowHandle(), &clientRect))
    {
        return false;
    }

    const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (clientWidth <= 0.0f || clientHeight <= 0.0f)
    {
        return false;
    }

    const float centerX = (kRespawnButtonCenterX + 1.0f) * 0.5f * clientWidth;
    const float centerY = (1.0f - kRespawnButtonCenterY) * 0.5f * clientHeight;
    const float halfWidth = kRespawnButtonScaleX * 0.5f * clientWidth;
    const float halfHeight = kRespawnButtonScaleY * 0.5f * clientHeight;

    return std::fabs(static_cast<float>(cursor.x) - centerX) <= halfWidth &&
        std::fabs(static_cast<float>(cursor.y) - centerY) <= halfHeight;
}

bool UIManager::IsReturnToVillageButtonHovered(float buttonCenterX) const
{
    if (!mReturnToVillageConfirmActive || mGame == nullptr)
    {
        return false;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
    {
        return false;
    }

    RECT clientRect{};
    if (!GetClientRect(mGame->GetMainWindowHandle(), &clientRect))
    {
        return false;
    }

    const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (clientWidth <= 0.0f || clientHeight <= 0.0f)
    {
        return false;
    }

    const float centerX = (buttonCenterX + 1.0f) * 0.5f * clientWidth;
    const float centerY = (1.0f - kReturnConfirmButtonY) * 0.5f * clientHeight;
    const float halfWidth = kReturnConfirmButtonScaleX * 0.5f * clientWidth;
    const float halfHeight = kReturnConfirmButtonScaleY * 0.5f * clientHeight;

    return std::fabs(static_cast<float>(cursor.x) - centerX) <= halfWidth &&
        std::fabs(static_cast<float>(cursor.y) - centerY) <= halfHeight;
}

bool UIManager::IsReturnToVillageYesButtonHovered() const
{
    return IsReturnToVillageButtonHovered(kReturnConfirmYesButtonX);
}

bool UIManager::IsReturnToVillageNoButtonHovered() const
{
    return IsReturnToVillageButtonHovered(kReturnConfirmNoButtonX);
}

void UIManager::ShowStageClearRecords()
{
    if (mStageClearScreenActive)
    {
        mStageClearRecordsView = true;
    }
}

bool UIManager::IsStageClearButtonHovered() const
{
    if (!mStageClearScreenActive || mGame == nullptr)
    {
        return false;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
    {
        return false;
    }

    RECT clientRect{};
    if (!GetClientRect(mGame->GetMainWindowHandle(), &clientRect))
    {
        return false;
    }

    const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (clientWidth <= 0.0f || clientHeight <= 0.0f)
    {
        return false;
    }

    const float centerX = (kStageClearButtonCenterX + 1.0f) * 0.5f * clientWidth;
    const float centerY = (1.0f - kStageClearButtonCenterY) * 0.5f * clientHeight;
    const float halfWidth = kStageClearButtonScaleX * 0.5f * clientWidth;
    const float halfHeight = kStageClearButtonScaleY * 0.5f * clientHeight;

    return std::fabs(static_cast<float>(cursor.x) - centerX) <= halfWidth &&
        std::fabs(static_cast<float>(cursor.y) - centerY) <= halfHeight;
}

bool UIManager::IsStageClearNextButtonHovered() const
{
    return mStageClearScreenActive &&
        !mStageGameOverScreenActive &&
        !mStageClearRecordsView &&
        IsStageClearButtonHovered();
}

bool UIManager::IsStageClearEndButtonHovered() const
{
    return mStageClearScreenActive &&
        (mStageGameOverScreenActive || mStageClearRecordsView) &&
        IsStageClearButtonHovered();
}

void UIManager::InitializeEffect(Material* flashMat, Material* bgMat, GameObject* flashObj, GameObject* screenBgObj)
{
    mFlashMat = flashMat;
    mBgMat = bgMat;
    mFlashObj = flashObj;
    mScreenBgObj = screenBgObj;
    mUseShortFlashProfile = false;
    mFlashPeakAlpha = 1.0f;
    mBgPeakAlpha = 1.0f;
    mFlashBaseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    mBgBaseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

    // 평소에는 눈에 보이지 않도록 투명도(Alpha)를 0으로 꺼둡니다.
    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        mFlashMat->NumFramesDirty = 3;
    }
    if (mBgMat) {
        mBgMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        mBgMat->NumFramesDirty = 3;
    }

    if (mFlashObj) {
        mFlashObj->SetScale(0.0f, 0.0f, 1.0f);
        mFlashObj->Update();
    }

    if (mScreenBgObj) {
        mScreenBgObj->SetScale(0.0f, 0.0f, 1.0f);
        mScreenBgObj->Update();
    }
}

void UIManager::TriggerFlashEffect()
{
    mIsFlashActive = true;
    mCurrentTime = 0.0f;
    mFlashDuration = 1.55f;
    mUseShortFlashProfile = false;
    mFlashPeakAlpha = 1.0f;
    mBgPeakAlpha = 1.0f;
    mFlashBaseColor = XMFLOAT4(1.0f, 0.95f, 0.82f, 0.0f);
    mBgBaseColor = XMFLOAT4(0.95f, 0.9f, 0.72f, 0.0f);

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = mFlashBaseColor;
        mFlashMat->NumFramesDirty = 3;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = mBgBaseColor;
        mBgMat->NumFramesDirty = 3;
    }

    if (mScreenBgObj) {
        mScreenBgObj->SetScale(1.05f, 1.05f, 1.0f);
        mScreenBgObj->SetPosition(0.0f, 0.0f, 0.18f);
        mScreenBgObj->Update();
    }

    if (mFlashObj) {
        mFlashObj->SetScale(1.35f, 1.35f, 1.0f);
        mFlashObj->SetPosition(0.0f, 0.0f, 0.12f);
        mFlashObj->Update();
    }
}

void UIManager::TriggerLevelUpFlashEffect(PlayerClass playerClass, int newLevel)
{
    mIsFlashActive = true;
    mCurrentTime = 0.0f;
    mFlashDuration = newLevel >= 3 ? 0.58f : 0.44f;
    mUseShortFlashProfile = true;
    mFlashPeakAlpha = newLevel >= 3 ? 0.62f : 0.44f;
    mBgPeakAlpha = newLevel >= 3 ? 0.24f : 0.16f;
    mFlashBaseColor = GetLevelUpFlashColor(playerClass, newLevel);
    mBgBaseColor = {
        mFlashBaseColor.x * 0.42f,
        mFlashBaseColor.y * 0.42f,
        mFlashBaseColor.z * 0.42f,
        0.0f
    };

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = mFlashBaseColor;
        mFlashMat->NumFramesDirty = 3;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = mBgBaseColor;
        mBgMat->NumFramesDirty = 3;
    }

    if (mScreenBgObj) {
        mScreenBgObj->SetScale(1.02f, 1.02f, 1.0f);
        mScreenBgObj->SetPosition(0.0f, 0.0f, 0.18f);
        mScreenBgObj->Update();
    }

    if (mFlashObj) {
        const float flashScale = newLevel >= 3 ? 1.25f : 1.15f;
        mFlashObj->SetScale(flashScale, flashScale, 1.0f);
        mFlashObj->SetPosition(0.0f, 0.0f, 0.12f);
        mFlashObj->Update();
    }
}

void UIManager::UpdateEffect(float dt)
{
    if (!mIsFlashActive) return;

    mCurrentTime += dt;
    float bgAlpha = 0.0f;
    float flashAlpha = 0.0f;

    if (mUseShortFlashProfile)
    {
        const float duration = (std::max)(mFlashDuration, 0.001f);
        const float normalizedTime = (std::clamp)(mCurrentTime / duration, 0.0f, 1.0f);
        if (normalizedTime < 0.18f)
        {
            const float t = normalizedTime / 0.18f;
            bgAlpha = mBgPeakAlpha * t;
            flashAlpha = mFlashPeakAlpha * t;
        }
        else if (normalizedTime < 0.42f)
        {
            const float t = (normalizedTime - 0.18f) / 0.24f;
            bgAlpha = mBgPeakAlpha + (mBgPeakAlpha * 0.45f - mBgPeakAlpha) * t;
            flashAlpha = mFlashPeakAlpha + (mFlashPeakAlpha * 0.60f - mFlashPeakAlpha) * t;
        }
        else if (normalizedTime < 1.0f)
        {
            const float t = (normalizedTime - 0.42f) / 0.58f;
            bgAlpha = (mBgPeakAlpha * 0.45f) * (1.0f - t);
            flashAlpha = (mFlashPeakAlpha * 0.60f) * (1.0f - t);
        }
        else
        {
            bgAlpha = 0.0f;
            flashAlpha = 0.0f;
            mIsFlashActive = false;
        }
    }
    else if (mCurrentTime < 0.22f)
    {
        float t = mCurrentTime / 0.22f;
        bgAlpha = 0.18f + (t * 0.45f);
        flashAlpha = 0.35f + (t * 0.45f);
    }
    else if (mCurrentTime < 0.46f)
    {
        float t = (mCurrentTime - 0.22f) / 0.24f;
        bgAlpha = 0.63f + (t * 0.28f);
        flashAlpha = 0.8f + (t * 0.2f);
    }
    else if (mCurrentTime < 1.12f)
    {
        bgAlpha = 1.0f;
        flashAlpha = 1.0f;
    }
    else if (mCurrentTime < mFlashDuration)
    {
        float t = (mCurrentTime - 1.12f) / (mFlashDuration - 1.12f);
        bgAlpha = (1.0f - t);
        flashAlpha = (1.0f - t) * 0.78f;
    }
    else
    {
        bgAlpha = 0.0f;
        flashAlpha = 0.0f;
        mIsFlashActive = false;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = mBgBaseColor;
        mBgMat->DiffuseAlbedo.w = bgAlpha;
        mBgMat->NumFramesDirty = 3;
    }

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = mFlashBaseColor;
        mFlashMat->DiffuseAlbedo.w = flashAlpha;
        mFlashMat->NumFramesDirty = 3;
    }

    if (!mIsFlashActive)
    {
        if (mScreenBgObj) {
            mScreenBgObj->SetScale(0.0f, 0.0f, 1.0f);
            mScreenBgObj->Update();
        }

        if (mFlashObj) {
            mFlashObj->SetScale(0.0f, 0.0f, 1.0f);
            mFlashObj->Update();
        }
    }
}
