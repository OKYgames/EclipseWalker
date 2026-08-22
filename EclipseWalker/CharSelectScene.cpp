#include "CharSelectScene.h"
#include "CharacterVisualFactory.h"
#include "DebugConfig.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "SkeletalAnimationComponent.h"
#include "VillageScene.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

using namespace DirectX;

namespace
{
    constexpr float kCameraZ = -10.0f;
    constexpr float kMenuFovY = 0.25f * DirectX::XM_PI;
    constexpr float kPreviewModelZ = -0.18f;
    constexpr float kBackgroundZ = 1.45f;
    constexpr float kDimOverlayZ = 1.40f;
    constexpr float kTitlePanelZ = -0.002f;
    constexpr float kInfoPanelZ = -0.010f;
    constexpr float kListPanelZ = -0.012f;
    constexpr float kClassCardZ = -0.018f;
    constexpr float kSelectionHighlightZ = -0.028f;
    constexpr float kConfirmButtonZ = -0.030f;
    constexpr float kSkillIconZ = -0.040f;

    DirectX::XMFLOAT2 GetUiClientSize(EclipseWalkerGame* game)
    {
        RECT clientRect = {};
        if (game != nullptr && GetClientRect(game->GetMainWindowHandle(), &clientRect))
        {
            return {
                static_cast<float>((std::max)(1L, clientRect.right - clientRect.left)),
                static_cast<float>((std::max)(1L, clientRect.bottom - clientRect.top))
            };
        }

        const auto viewport = game != nullptr ? game->GetScreenViewport() : D3D12_VIEWPORT{};
        return {
            (std::max)(1.0f, viewport.Width),
            (std::max)(1.0f, viewport.Height)
        };
    }

    DirectX::XMFLOAT2 GetUiRenderSize(EclipseWalkerGame* game)
    {
        const auto viewport = game != nullptr ? game->GetScreenViewport() : D3D12_VIEWPORT{};
        return {
            (std::max)(1.0f, viewport.Width),
            (std::max)(1.0f, viewport.Height)
        };
    }

    float GetHalfViewHeight(float z)
    {
        return std::tan(kMenuFovY * 0.5f) * std::abs(z - kCameraZ);
    }

    float PixelToWorldX(float px, float screenW, float screenH, float z)
    {
        const float halfViewH = GetHalfViewHeight(z);
        const float halfViewW = halfViewH * (screenW / screenH);
        return ((px / screenW) * 2.0f - 1.0f) * halfViewW;
    }

    float PixelToWorldY(float py, float screenH, float z)
    {
        const float halfViewH = GetHalfViewHeight(z);
        return (1.0f - (py / screenH) * 2.0f) * halfViewH;
    }

    float GetUiTextScale(float screenW, float screenH)
    {
        const float widthScale = screenW / 1280.0f;
        const float heightScale = screenH / 720.0f;
        return std::clamp((std::min)(widthScale, heightScale), 0.85f, 1.50f);
    }

    std::string ToLowerAscii(std::string value)
    {
        for (char& ch : value)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }
        return value;
    }

    bool ContainsAsciiInsensitive(const std::string& value, const std::string& pattern)
    {
        return ToLowerAscii(value).find(ToLowerAscii(pattern)) != std::string::npos;
    }

    struct ClassUiInfo
    {
        PlayerClass playerClass;
        const wchar_t* displayName;
        const wchar_t* roleText;
        const wchar_t* skill1;
        const wchar_t* skill2;
        const char* skillMat1;
        const char* skillMat2;
        DirectX::XMFLOAT4 accent;
    };

    const std::array<ClassUiInfo, 3> kClassInfos = { {
        {
            PlayerClass::Warrior,
            L"전사",
            L"근접 전투와 강한 한 방",
            L"지진 강타",
            L"대검 소환",
            "CS_Skill_Warrior_EarthquakeSlamMat",
            "CS_Skill_Warrior_GreatswordSummonMat",
            { 0.95f, 0.34f, 0.25f, 1.0f }
        },
        {
            PlayerClass::Mage,
            L"마법사",
            L"회복과 광역 마법",
            L"치유의 빛",
            L"메테오",
            "CS_Skill_Mage_HealingLightMat",
            "CS_Skill_Mage_MeteorMat",
            { 0.42f, 0.72f, 1.0f, 1.0f }
        },
        {
            PlayerClass::Archer,
            L"궁수",
            L"빠른 원거리 공격",
            L"바람의 깃들임",
            L"화살비",
            "CS_Skill_Archer_WindImbuementMat",
            "CS_Skill_Archer_ArrowRainMat",
            { 0.40f, 0.94f, 0.55f, 1.0f }
        }
    } };

    const char* ToClassName(PlayerClass playerClass)
    {
        switch (playerClass)
        {
        case PlayerClass::Warrior: return "Warrior";
        case PlayerClass::Mage: return "Mage";
        case PlayerClass::Archer: return "Archer";
        default: return "None";
        }
    }

    int GetClassIndex(PlayerClass playerClass)
    {
        for (int i = 0; i < static_cast<int>(kClassInfos.size()); ++i)
        {
            if (kClassInfos[i].playerClass == playerClass)
            {
                return i;
            }
        }

        return 1;
    }

    const ClassUiInfo& GetClassInfo(PlayerClass playerClass)
    {
        return kClassInfos[GetClassIndex(playerClass)];
    }

    CharacterVisualSpec BuildPreviewVisualSpec(PlayerClass playerClass, const DirectX::XMFLOAT3& spawnPosition, float targetHeight)
    {
        CharacterVisualSpec spec;
        spec.UseSkinned = true;
        spec.LoadModelAnimations = false;
        spec.DefaultClipName = "";
        spec.SpawnPosition = spawnPosition;
        spec.UseActorOrigin = false;
        spec.TargetHeight = targetHeight;
        spec.RotationOffset = { 0.0f, 0.0f, 0.0f };
        spec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        spec.FresnelR0 = { 0.06f, 0.06f, 0.06f };
        spec.Roughness = 0.65f;
        spec.IsToon = true;
        spec.OutlineThickness = 0.010f;
        spec.OutlineColor = { 0.04f, 0.04f, 0.06f, 1.0f };
        spec.FallbackMaterialName = "CS_CardMat";
        spec.FallbackScale = { 0.32f, 0.58f, 0.32f };

        switch (playerClass)
        {
        case PlayerClass::Warrior:
            spec.ModelPath = "Models/Player/Warrior_Lv3.fbx";
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Female_Warrior/Female_Warrior_Idle.fbx", "FemaleIdle" });
            spec.GeometryName = "csWarriorPreviewGeo";
            spec.MaterialName = "CS_Preview_WarriorMat";
            spec.DiffuseTextureName = "CS_Preview_WarriorTex";
            spec.DiffuseTexturePath = L"Textures/P09_Female_Armor_006_Diff.dds";
            spec.OutlineColor = { 0.08f, 0.04f, 0.035f, 1.0f };
            break;

        case PlayerClass::Mage:
            spec.ModelPath = "Models/Player/Wizard_Lv3.fbx";
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/Male_Wizard/Standing Torch Idle 01.fbx", "FemaleIdle" });
            spec.GeometryName = "csMagePreviewGeo";
            spec.MaterialName = "CS_Preview_MageMat";
            spec.DiffuseTextureName = "CS_Preview_MageTex";
            spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_009_BaseMap.dds";
            spec.OutlineColor = { 0.035f, 0.055f, 0.09f, 1.0f };
            break;

        case PlayerClass::Archer:
            spec.ModelPath = "Models/Player/Archer_Lv3.fbx";
            spec.AdditionalAnimationClips.push_back({ "Models/Animated/male_archer/Standing Idle.fbx", "FemaleIdle" });
            spec.GeometryName = "csArcherPreviewGeo";
            spec.MaterialName = "CS_Preview_ArcherMat";
            spec.DiffuseTextureName = "CS_Preview_ArcherTex";
            spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_012_BaseMap.dds";
            spec.OutlineColor = { 0.035f, 0.07f, 0.04f, 1.0f };
            break;

        case PlayerClass::None:
        default:
            spec.ModelPath = "Models/Player/Wizard_Lv3.fbx";
            spec.GeometryName = "csMagePreviewGeo";
            spec.MaterialName = "CS_Preview_MageMat";
            spec.DiffuseTextureName = "CS_Preview_MageTex";
            spec.DiffuseTexturePath = L"Textures/P09_Male_Armor_009_BaseMap.dds";
            break;
        }

        return spec;
    }

    bool ContainsPoint(const CharSelectScene::UiRect& rect, float x, float y)
    {
        return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
    }

    CharSelectScene::UiRect ExpandRect(const CharSelectScene::UiRect& rect, float amount)
    {
        return {
            rect.left - amount,
            rect.top - amount,
            rect.right + amount,
            rect.bottom + amount
        };
    }
}

void CharSelectScene::Enter()
{
    mGame->FlushCommandQueue();
    mGame->ClearSocketAttachments();
    OutputDebugStringA("\n[Character Select Scene] Enter with class previews.\n");

    mLeftKeyPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    mRightKeyPressed = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    mEnterKeyPressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    mMousePressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    mLastViewportWidth = 0.0f;
    mLastViewportHeight = 0.0f;
    mClassCardObjects = {};
    mClassPreviewObjects = {};
    mClassPreviewOverlayObjects = {};
    mSelectionHighlightObj = nullptr;
    mSkillIcon1Ritem = nullptr;
    mSkillIcon2Ritem = nullptr;

    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects();
    ritems.clear();
    gameObjects.clear();

    auto camera = mGame->GetCamera();
    camera->SetPosition(0.0f, 0.0f, kCameraZ);
    camera->LookAt(
        DirectX::XMFLOAT3(0.0f, 0.0f, kCameraZ),
        DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
    camera->UpdateViewMatrix();

    if (mGame->GetSelectedPlayerClass() == PlayerClass::None)
    {
        mGame->SetSelectedPlayerClass(PlayerClass::Mage);
    }

    InitializeUiResources();
    BuildStaticUi();
    UpdateSelectionVisuals();
}

void CharSelectScene::Exit()
{
    mGame->FlushCommandQueue();
    mGame->UnloadSharedGameResources();
    mGame->ResetLights();
    mGame->GetRitems().clear();
    mGame->GetGameObjects().clear();
}

void CharSelectScene::InitializeUiResources()
{
    auto* device = mGame->GetDevice();
    auto* cmdQueue = mGame->GetCommandQueue();

    if (!mGraphicsMemory)
    {
        mGraphicsMemory = std::make_unique<GraphicsMemory>(device);
    }

    if (!mFontHeap)
    {
        mFontHeap = std::make_unique<DirectX::DescriptorHeap>(
            device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            1);
    }

    if (!mFont || !mSpriteBatch)
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        if (!mFont)
        {
            mFont = std::make_unique<SpriteFont>(
                device,
                resourceUpload,
                L"Textures/chat_korean.spritefont",
                mFontHeap->GetCpuHandle(0),
                mFontHeap->GetGpuHandle(0));
        }

        if (!mSpriteBatch)
        {
            RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
            SpriteBatchPipelineStateDescription pd(rtState);
            mSpriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
        }

        auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
        uploadResourcesFinished.wait();
    }
}

void CharSelectScene::BuildStaticUi()
{
    auto* res = mGame->GetResources();
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects();

    auto loadTextureIfExists = [&](const std::string& name, const std::wstring& path)
        {
            if (res->GetTexture(name) == nullptr && std::filesystem::exists(path))
            {
                res->LoadTexture(name, path);
            }
        };

    loadTextureIfExists("white", L"Textures/white.dds");
    loadTextureIfExists("CS_BackgroundTex", L"Textures/UI/CharSelectBackground_Simple.dds");
    loadTextureIfExists("UI_Skill_Warrior_EarthquakeSlam", L"Textures/UI/Skill_Warrior_EarthquakeSlam_512x512.dds");
    loadTextureIfExists("UI_Skill_Warrior_GreatswordSummon", L"Textures/UI/Skill_Warrior_GreatswordSummon_512x512.dds");
    loadTextureIfExists("UI_Skill_Mage_HealingLight", L"Textures/UI/Skill_Mage_HealingLight_512x512.dds");
    loadTextureIfExists("UI_Skill_Mage_Meteor", L"Textures/UI/Skill_Mage_Meteor_512x512.dds");
    loadTextureIfExists("UI_Skill_Archer_WindImbuement", L"Textures/UI/Skill_Archer_WindImbuement_512x512.dds");
    loadTextureIfExists("UI_Skill_Archer_ArrowRain", L"Textures/UI/Skill_Archer_ArrowRain_512x512.dds");
    loadTextureIfExists("WarriorLv3SwordTex", L"Textures/P09_Weapon_Sword_05_Diff.dds");
    loadTextureIfExists("WarriorLv3ShieldTex", L"Textures/P09_Weapon_Shield_05_Diff.dds");
    loadTextureIfExists("ArcherLv3BowTex", L"Textures/P09_Weapon_Bow_04_BaseMap.dds");
    loadTextureIfExists("WizardLv3StaffTex", L"Textures/P09_Weapon_Staff_04_BaseMap.dds");
    loadTextureIfExists("CS_Preview_SkinTex", L"Textures/P09_Female_Body_Bright_Diff.dds");
    loadTextureIfExists("CS_Preview_FemaleSkinTex", L"Textures/P09_Female_Body_Bright_Diff.dds");
    loadTextureIfExists("CS_Preview_MaleSkinTex", L"Textures/P09_Male_Body_Bright_Diff.dds");

    auto ensureMaterial = [&](const std::string& name, const std::string& textureName, const DirectX::XMFLOAT4& color)
        {
            const std::string diffuseName = res->GetTexture(textureName) != nullptr ? textureName : "white";
            res->CreateMaterial(
                name,
                static_cast<int>(res->mMaterials.size()),
                diffuseName,
                "",
                "",
                "",
                color,
                DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f),
                0.55f);

            if (auto* mat = res->GetMaterial(name))
            {
                mat->DiffuseMapName = diffuseName;
                mat->DiffuseAlbedo = color;
                mat->IsTransparent = 1;
                mat->NumFramesDirty = gNumFrameResources;
            }
        };

    {
        const std::string backgroundDiffuseName = res->GetTexture("CS_BackgroundTex") != nullptr ? "CS_BackgroundTex" : "white";
        res->CreateMaterial(
            "CS_BackgroundMat",
            static_cast<int>(res->mMaterials.size()),
            backgroundDiffuseName,
            "",
            "",
            "",
            DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
            1.0f);

        if (auto* mat = res->GetMaterial("CS_BackgroundMat"))
        {
            mat->DiffuseMapName = backgroundDiffuseName;
            mat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            mat->IsTransparent = 0;
            mat->IsToon = 0;
            mat->NumFramesDirty = gNumFrameResources;
        }
    }

    ensureMaterial("CS_DimMat", "white", DirectX::XMFLOAT4(0.015f, 0.016f, 0.020f, 0.58f));
    ensureMaterial("CS_TitlePanelMat", "white", DirectX::XMFLOAT4(0.045f, 0.050f, 0.064f, 0.82f));
    ensureMaterial("CS_CardMat", "white", DirectX::XMFLOAT4(0.055f, 0.058f, 0.072f, 0.86f));
    ensureMaterial("CS_SelectedCardMat", "white", DirectX::XMFLOAT4(0.55f, 0.44f, 0.22f, 0.38f));
    ensureMaterial("CS_InfoPanelMat", "white", DirectX::XMFLOAT4(0.030f, 0.034f, 0.045f, 0.88f));
    ensureMaterial("CS_ButtonMat", "white", DirectX::XMFLOAT4(0.18f, 0.32f, 0.42f, 0.88f));
    ensureMaterial("CS_Skill_Warrior_EarthquakeSlamMat", "UI_Skill_Warrior_EarthquakeSlam", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    ensureMaterial("CS_Skill_Warrior_GreatswordSummonMat", "UI_Skill_Warrior_GreatswordSummon", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    ensureMaterial("CS_Skill_Mage_HealingLightMat", "UI_Skill_Mage_HealingLight", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    ensureMaterial("CS_Skill_Mage_MeteorMat", "UI_Skill_Mage_Meteor", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    ensureMaterial("CS_Skill_Archer_WindImbuementMat", "UI_Skill_Archer_WindImbuement", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    ensureMaterial("CS_Skill_Archer_ArrowRainMat", "UI_Skill_Archer_ArrowRain", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

    auto ensureOpaqueMaterial = [&](const std::string& name, const std::string& textureName, const DirectX::XMFLOAT4& color, float roughness)
        {
            const std::string diffuseName = res->GetTexture(textureName) != nullptr ? textureName : "white";
            res->CreateMaterial(
                name,
                static_cast<int>(res->mMaterials.size()),
                diffuseName,
                "",
                "",
                "",
                color,
                DirectX::XMFLOAT3(0.06f, 0.06f, 0.06f),
                roughness);

            if (auto* mat = res->GetMaterial(name))
            {
                mat->DiffuseMapName = diffuseName;
                mat->DiffuseAlbedo = color;
                mat->IsTransparent = 0;
                mat->IsToon = 1;
                mat->OutlineThickness = 0.008f;
                mat->OutlineColor = { 0.04f, 0.04f, 0.05f, 1.0f };
                mat->NumFramesDirty = gNumFrameResources;
            }
        };

    ensureOpaqueMaterial("PlayerSwordMat", "WarriorLv3SwordTex", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.35f);
    ensureOpaqueMaterial("PlayerShieldMat", "WarriorLv3ShieldTex", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.35f);
    ensureOpaqueMaterial("PlayerBowMat", "ArcherLv3BowTex", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.38f);
    ensureOpaqueMaterial("PlayerStaffMat", "WizardLv3StaffTex", DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.38f);
    ensureOpaqueMaterial("CS_Preview_SkinMat", "CS_Preview_SkinTex", DirectX::XMFLOAT4(1.0f, 0.94f, 0.88f, 1.0f), 0.62f);
    ensureOpaqueMaterial("CS_Preview_FemaleSkinMat", "CS_Preview_FemaleSkinTex", DirectX::XMFLOAT4(1.0f, 0.94f, 0.88f, 1.0f), 0.62f);
    ensureOpaqueMaterial("CS_Preview_MaleSkinMat", "CS_Preview_MaleSkinTex", DirectX::XMFLOAT4(1.0f, 0.94f, 0.88f, 1.0f), 0.62f);
    ensureOpaqueMaterial("CS_Preview_HairMat", "white", DirectX::XMFLOAT4(0.070f, 0.055f, 0.045f, 1.0f), 0.70f);

    const DirectX::XMFLOAT2 renderSize = GetUiRenderSize(mGame);
    const float screenW = renderSize.x;
    const float screenH = renderSize.y;
    mLastViewportWidth = screenW;
    mLastViewportHeight = screenH;

    const float listLeft = screenW * 0.785f;
    const float listRight = screenW * 0.965f;
    const float rowTop = screenH * 0.185f;
    const float rowH = std::clamp(screenH * 0.070f, 48.0f, 78.0f);
    const float rowGap = std::clamp(screenH * 0.012f, 7.0f, 14.0f);
    for (int i = 0; i < 3; ++i)
    {
        const float top = rowTop + static_cast<float>(i) * (rowH + rowGap);
        mClassCardRects[i] = {
            listLeft,
            top,
            listRight,
            top + rowH
        };
    }

    const UiRect fullScreen = { 0.0f, 0.0f, screenW, screenH };
    const UiRect titlePanel = { screenW * 0.785f, screenH * 0.070f, screenW * 0.965f, screenH * 0.135f };
    const UiRect infoPanel = { screenW * 0.045f, screenH * 0.115f, screenW * 0.305f, screenH * 0.895f };
    const UiRect listPanel = { screenW * 0.770f, screenH * 0.145f, screenW * 0.980f, screenH * 0.520f };
    mConfirmButtonRect = { screenW * 0.355f, screenH * 0.815f, screenW * 0.645f, screenH * 0.905f };
    const UiRect skillIcon1Rect = { screenW * 0.078f, screenH * 0.625f, screenW * 0.138f, screenH * 0.730f };
    const UiRect skillIcon2Rect = { screenW * 0.175f, screenH * 0.625f, screenW * 0.235f, screenH * 0.730f };

    auto applyRectToObject = [&](GameObject* object, const UiRect& rect, float z)
        {
            if (object == nullptr)
            {
                return;
            }

            const float left = PixelToWorldX(rect.left, screenW, screenH, z);
            const float right = PixelToWorldX(rect.right, screenW, screenH, z);
            const float top = PixelToWorldY(rect.top, screenH, z);
            const float bottom = PixelToWorldY(rect.bottom, screenH, z);

            object->SetScale(std::abs(right - left) * 0.5f, std::abs(top - bottom) * 0.5f, 1.0f);
            object->SetPosition((left + right) * 0.5f, (top + bottom) * 0.5f, z);
            object->Update();
        };

    auto createQuad = [&](const std::string& materialName, const UiRect& rect, float z, GameObject** outObject = nullptr, RenderItem** outRitem = nullptr)
        {
            auto ritem = std::make_unique<RenderItem>();
            ritem->TexTransform = MathHelper::Identity4x4();
            ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
            ritem->NumFramesDirty = gNumFrameResources;
            ritem->Mat = res->GetMaterial(materialName);
            ritem->Geo = res->mGeometries["quadGeo"].get();
            ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            ritem->IndexCount = ritem->Geo->DrawArgs["quad"].IndexCount;
            ritem->StartIndexLocation = ritem->Geo->DrawArgs["quad"].StartIndexLocation;
            ritem->BaseVertexLocation = ritem->Geo->DrawArgs["quad"].BaseVertexLocation;

            auto object = std::make_unique<GameObject>();
            object->Ritem = ritem.get();
            applyRectToObject(object.get(), rect, z);

            if (outObject != nullptr)
            {
                *outObject = object.get();
            }
            if (outRitem != nullptr)
            {
                *outRitem = ritem.get();
            }

            ritems.push_back(std::move(ritem));
            gameObjects.push_back(std::move(object));
        };

    std::array<DirectX::XMFLOAT3, 3> previewSpawnPositions = {};
    std::array<float, 3> previewTargetHeights = {};
    const UiRect previewRect = {
        screenW * 0.335f,
        screenH * 0.125f,
        screenW * 0.725f,
        screenH * 0.780f
    };
    const float previewCenterX = (previewRect.left + previewRect.right) * 0.5f;
    const float previewTopY = PixelToWorldY(previewRect.top, screenH, kPreviewModelZ);
    const float previewBottomY = PixelToWorldY(previewRect.bottom, screenH, kPreviewModelZ);
    const float previewHeight = std::abs(previewTopY - previewBottomY) * 0.96f;
    const float previewLift = previewHeight * 0.055f;
    const DirectX::XMFLOAT3 previewSpawn = { PixelToWorldX(previewCenterX, screenW, screenH, kPreviewModelZ), previewBottomY + previewLift, kPreviewModelZ };
    mGame->ApplyCharacterSelectLighting({ previewSpawn.x, previewSpawn.y + previewHeight * 0.72f, previewSpawn.z });
    for (int i = 0; i < 3; ++i)
    {
        previewSpawnPositions[i] = previewSpawn;
        previewTargetHeights[i] = previewHeight;
    }

    BuildClassPreviewModels(previewSpawnPositions, previewTargetHeights);
    mGame->BuildDescriptorHeaps();

    createQuad("CS_BackgroundMat", fullScreen, kBackgroundZ);
    createQuad("CS_DimMat", fullScreen, kDimOverlayZ);
    createQuad("CS_TitlePanelMat", titlePanel, kTitlePanelZ);
    createQuad("CS_InfoPanelMat", infoPanel, kInfoPanelZ);
    createQuad("CS_InfoPanelMat", listPanel, kListPanelZ);
    createQuad("CS_SelectedCardMat", ExpandRect(mClassCardRects[GetClassIndex(mGame->GetSelectedPlayerClass())], 9.0f), kSelectionHighlightZ, &mSelectionHighlightObj);

    for (int i = 0; i < 3; ++i)
    {
        createQuad("CS_CardMat", mClassCardRects[i], kClassCardZ, &mClassCardObjects[i]);
    }

    createQuad("CS_ButtonMat", mConfirmButtonRect, kConfirmButtonZ);
    createQuad("CS_Skill_Mage_HealingLightMat", skillIcon1Rect, kSkillIconZ, nullptr, &mSkillIcon1Ritem);
    createQuad("CS_Skill_Mage_MeteorMat", skillIcon2Rect, kSkillIconZ, nullptr, &mSkillIcon2Ritem);
}

void CharSelectScene::RebuildStaticUiForCurrentViewport()
{
    mGame->FlushCommandQueue();
    mClassCardObjects = {};
    mClassPreviewObjects = {};
    mClassPreviewOverlayObjects = {};
    mSelectionHighlightObj = nullptr;
    mSkillIcon1Ritem = nullptr;
    mSkillIcon2Ritem = nullptr;
    mMousePressed = false;

    mGame->ClearSocketAttachments();
    mGame->GetRitems().clear();
    mGame->GetGameObjects().clear();

    BuildStaticUi();
    UpdateSelectionVisuals();
}

void CharSelectScene::BuildClassPreviewModels(
    const std::array<DirectX::XMFLOAT3, 3>& spawnPositions,
    const std::array<float, 3>& targetHeights)
{
    auto* res = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* cmdList = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects();
    if (res == nullptr || device == nullptr || cmdList == nullptr)
    {
        return;
    }

    auto createPreviewOverlay = [&](int classIndex, GameObject* parentObject, RenderItem* parentRitem)
        {
            if (parentObject == nullptr || parentRitem == nullptr || parentRitem->Geo == nullptr)
            {
                return;
            }

            const bool isSelected = classIndex == GetClassIndex(mGame->GetSelectedPlayerClass());
            for (const auto& drawArgPair : parentRitem->Geo->DrawArgs)
            {
                const std::string& subsetName = drawArgPair.first;
                const bool isHairSubset = ContainsAsciiInsensitive(subsetName, "hair");
                const bool isFaceSubset = ContainsAsciiInsensitive(subsetName, "face");
                if (!isHairSubset && !isFaceSubset)
                {
                    continue;
                }

                const bool isFemalePreview = kClassInfos[classIndex].playerClass == PlayerClass::Warrior;
                const char* skinMaterialName = isFemalePreview ? "CS_Preview_FemaleSkinMat" : "CS_Preview_MaleSkinMat";
                Material* material = res->GetMaterial(isHairSubset ? "CS_Preview_HairMat" : skinMaterialName);
                if (material == nullptr && isFaceSubset)
                {
                    material = res->GetMaterial("CS_Preview_SkinMat");
                }
                if (material == nullptr)
                {
                    continue;
                }

                const auto& submesh = drawArgPair.second;
                auto overlayRitem = std::make_unique<RenderItem>();
                overlayRitem->World = parentRitem->World;
                overlayRitem->TexTransform = MathHelper::Identity4x4();
                overlayRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
                overlayRitem->NumFramesDirty = gNumFrameResources;
                overlayRitem->Geo = parentRitem->Geo;
                overlayRitem->Mat = material;
                overlayRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                overlayRitem->IndexCount = submesh.IndexCount;
                overlayRitem->StartIndexLocation = submesh.StartIndexLocation;
                overlayRitem->BaseVertexLocation = submesh.BaseVertexLocation;
                overlayRitem->IsSkinned = parentRitem->IsSkinned;
                overlayRitem->SkinnedCBIndex = parentRitem->SkinnedCBIndex;
                overlayRitem->Visible = isSelected;

                auto overlayObject = std::make_unique<GameObject>();
                overlayObject->Ritem = overlayRitem.get();
                overlayObject->SetWorldTransform(DirectX::XMLoadFloat4x4(&parentObject->World));

                mClassPreviewOverlayObjects[classIndex].push_back(overlayObject.get());
                ritems.push_back(std::move(overlayRitem));
                gameObjects.push_back(std::move(overlayObject));
            }
        };

    for (int i = 0; i < static_cast<int>(kClassInfos.size()); ++i)
    {
        auto ritem = std::make_unique<RenderItem>();
        ritem->ObjCBIndex = static_cast<UINT>(ritems.size());
        ritem->NumFramesDirty = gNumFrameResources;

        auto object = std::make_unique<GameObject>();
        const CharacterVisualSpec visualSpec = BuildPreviewVisualSpec(
            kClassInfos[i].playerClass,
            spawnPositions[i],
            targetHeights[i]);

        if (!CharacterVisualFactory::ApplyVisual(
            object.get(),
            ritem.get(),
            device,
            cmdList,
            res,
            visualSpec))
        {
            std::string log = "[Character Select] Failed to build preview model: ";
            log += ToClassName(kClassInfos[i].playerClass);
            log += "\n";
            OutputDebugStringA(log.c_str());
            continue;
        }

        if (auto* animation = object->GetSkeletalAnimation())
        {
            animation->Play("FemaleIdle");
        }

        if (object->Ritem != nullptr)
        {
            object->Ritem->Visible = (i == GetClassIndex(mGame->GetSelectedPlayerClass()));
        }

        GameObject* previewObject = object.get();
        RenderItem* previewRitem = ritem.get();
        mClassPreviewObjects[i] = previewObject;
        ritems.push_back(std::move(ritem));
        gameObjects.push_back(std::move(object));

        createPreviewOverlay(i, previewObject, previewRitem);

        GameObject* weaponObject = nullptr;
        GameObject* shieldObject = nullptr;
        mGame->BuildPlayerEquipment(
            previewObject,
            kClassInfos[i].playerClass,
            ClassTier::Tier3,
            weaponObject,
            shieldObject,
            false);
    }
}

void CharSelectScene::UpdateSelectionVisuals()
{
    const int selectedIndex = GetClassIndex(mGame->GetSelectedPlayerClass());
    const auto& selectedInfo = GetClassInfo(mGame->GetSelectedPlayerClass());

    for (int i = 0; i < static_cast<int>(mClassPreviewObjects.size()); ++i)
    {
        GameObject* previewObject = mClassPreviewObjects[i];
        if (previewObject != nullptr && previewObject->Ritem != nullptr)
        {
            previewObject->Ritem->Visible = (i == selectedIndex);
            previewObject->Ritem->NumFramesDirty = gNumFrameResources;
        }

        for (GameObject* overlayObject : mClassPreviewOverlayObjects[i])
        {
            if (overlayObject != nullptr && overlayObject->Ritem != nullptr)
            {
                overlayObject->Ritem->Visible = (i == selectedIndex);
                overlayObject->Ritem->NumFramesDirty = gNumFrameResources;
            }
        }
    }

    if (mSelectionHighlightObj != nullptr)
    {
        const DirectX::XMFLOAT2 renderSize = GetUiRenderSize(mGame);
        const float screenW = renderSize.x;
        const float screenH = renderSize.y;
        const UiRect highlightRect = ExpandRect(mClassCardRects[selectedIndex], 9.0f);

        const float left = PixelToWorldX(highlightRect.left, screenW, screenH, kSelectionHighlightZ);
        const float right = PixelToWorldX(highlightRect.right, screenW, screenH, kSelectionHighlightZ);
        const float top = PixelToWorldY(highlightRect.top, screenH, kSelectionHighlightZ);
        const float bottom = PixelToWorldY(highlightRect.bottom, screenH, kSelectionHighlightZ);

        mSelectionHighlightObj->SetScale(std::abs(right - left) * 0.5f, std::abs(top - bottom) * 0.5f, 1.0f);
        mSelectionHighlightObj->SetPosition((left + right) * 0.5f, (top + bottom) * 0.5f, kSelectionHighlightZ);
        mSelectionHighlightObj->Update();
    }

    auto* res = mGame->GetResources();
    if (mSkillIcon1Ritem != nullptr)
    {
        mSkillIcon1Ritem->Mat = res->GetMaterial(selectedInfo.skillMat1);
        mSkillIcon1Ritem->NumFramesDirty = gNumFrameResources;
    }
    if (mSkillIcon2Ritem != nullptr)
    {
        mSkillIcon2Ritem->Mat = res->GetMaterial(selectedInfo.skillMat2);
        mSkillIcon2Ritem->NumFramesDirty = gNumFrameResources;
    }

    if (auto* mat = res->GetMaterial("CS_SelectedCardMat"))
    {
        mat->DiffuseAlbedo = {
            selectedInfo.accent.x,
            selectedInfo.accent.y,
            selectedInfo.accent.z,
            0.38f
        };
        mat->NumFramesDirty = gNumFrameResources;
    }
}

void CharSelectScene::SelectClass(PlayerClass playerClass)
{
    if (mGame->GetSelectedPlayerClass() == playerClass)
    {
        return;
    }

    mGame->SetSelectedPlayerClass(playerClass);
    UpdateSelectionVisuals();

    std::string debugText = "[Character Select] Current class: ";
    debugText += ToClassName(playerClass);
    debugText += "\n";
    OutputDebugStringA(debugText.c_str());
}

void CharSelectScene::CycleSelection(int direction)
{
    int currentIndex = GetClassIndex(mGame->GetSelectedPlayerClass());
    currentIndex = (currentIndex + direction + static_cast<int>(kClassInfos.size())) % static_cast<int>(kClassInfos.size());
    SelectClass(kClassInfos[currentIndex].playerClass);
}

bool CharSelectScene::HandleMouseInput()
{
    const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!mouseDown)
    {
        mMousePressed = false;
        return false;
    }

    if (mMousePressed)
    {
        return false;
    }

    mMousePressed = true;

    POINT cursor = {};
    if (!GetCursorPos(&cursor))
    {
        return false;
    }

    ScreenToClient(mGame->GetMainWindowHandle(), &cursor);
    const DirectX::XMFLOAT2 clientSize = GetUiClientSize(mGame);
    const DirectX::XMFLOAT2 renderSize = GetUiRenderSize(mGame);
    const float x = static_cast<float>(cursor.x) * (renderSize.x / clientSize.x);
    const float y = static_cast<float>(cursor.y) * (renderSize.y / clientSize.y);

    if (ContainsPoint(mConfirmButtonRect, x, y))
    {
        return ConfirmSelection();
    }

    for (int i = 0; i < static_cast<int>(mClassCardRects.size()); ++i)
    {
        if (ContainsPoint(mClassCardRects[i], x, y))
        {
            SelectClass(kClassInfos[i].playerClass);
            break;
        }
    }

    return false;
}

bool CharSelectScene::ConfirmSelection()
{
    if (GetTickCount64() - gLastSceneChangeTime <= 500)
    {
        return false;
    }

    gLastSceneChangeTime = GetTickCount64();
    mGame->PrepareSelectedPlayerForNewRun();
    mGame->RequestSceneChange(std::make_unique<VillageScene>(mGame), L"LOADING VILLAGE");
    return true;
}

void CharSelectScene::Update(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

    const DirectX::XMFLOAT2 renderSize = GetUiRenderSize(mGame);
    const float screenW = renderSize.x;
    const float screenH = renderSize.y;
    if (std::abs(screenW - mLastViewportWidth) > 0.5f ||
        std::abs(screenH - mLastViewportHeight) > 0.5f)
    {
        RebuildStaticUiForCurrentViewport();
    }

    if (GetForegroundWindow() != mGame->GetMainWindowHandle())
    {
        mLeftKeyPressed = false;
        mRightKeyPressed = false;
        mEnterKeyPressed = false;
        mMousePressed = false;
        if (mGraphicsMemory)
        {
            mGraphicsMemory->Commit(mGame->GetCommandQueue());
        }
        return;
    }

    if (HandleMouseInput())
    {
        return;
    }

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
    {
        if (!mLeftKeyPressed)
        {
            CycleSelection(-1);
            mLeftKeyPressed = true;
        }
    }
    else
    {
        mLeftKeyPressed = false;
    }

    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
    {
        if (!mRightKeyPressed)
        {
            CycleSelection(1);
            mRightKeyPressed = true;
        }
    }
    else
    {
        mRightKeyPressed = false;
    }

    if (GetAsyncKeyState('1') & 0x8000)
    {
        SelectClass(PlayerClass::Warrior);
    }
    if (GetAsyncKeyState('2') & 0x8000)
    {
        SelectClass(PlayerClass::Mage);
    }
    if (GetAsyncKeyState('3') & 0x8000)
    {
        SelectClass(PlayerClass::Archer);
    }

    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    {
        if (!mEnterKeyPressed)
        {
            mEnterKeyPressed = true;
            if (ConfirmSelection())
            {
                return;
            }
        }
    }
    else
    {
        mEnterKeyPressed = false;
    }

    UpdateSelectionVisuals();

    if (mGraphicsMemory)
    {
        mGraphicsMemory->Commit(mGame->GetCommandQueue());
    }
}

void CharSelectScene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

    if (!mFont || !mSpriteBatch || !mFontHeap)
    {
        return;
    }

    auto* cmdList = mGame->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { mFontHeap->Heap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    const DirectX::XMFLOAT2 renderSize = GetUiRenderSize(mGame);
    mSpriteBatch->SetViewport(mGame->GetScreenViewport());
    mSpriteBatch->Begin(cmdList);

    auto drawCentered = [&](const wchar_t* text, const UiRect& rect, XMVECTORF32 color, float scale)
        {
            const XMVECTOR textSize = mFont->MeasureString(text);
            const float textW = XMVectorGetX(textSize) * scale;
            const float textH = XMVectorGetY(textSize) * scale;
            const float x = rect.left + (rect.right - rect.left - textW) * 0.5f;
            const float y = rect.top + (rect.bottom - rect.top - textH) * 0.5f;
            mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(x, y), color, 0.0f, XMFLOAT2(0.0f, 0.0f), scale);
        };

    const float screenW = renderSize.x;
    const float screenH = renderSize.y;
    const float textScale = GetUiTextScale(screenW, screenH);
    const UiRect titleTextRect = { screenW * 0.785f, screenH * 0.074f, screenW * 0.965f, screenH * 0.122f };
    drawCentered(L"클래스 선택", titleTextRect, Colors::White, 0.64f * textScale);

    const int selectedIndex = GetClassIndex(mGame->GetSelectedPlayerClass());
    const auto& selectedInfo = GetClassInfo(mGame->GetSelectedPlayerClass());

    mFont->DrawString(
        mSpriteBatch.get(),
        selectedInfo.displayName,
        XMFLOAT2(screenW * 0.070f, screenH * 0.170f),
        XMVECTORF32{ { selectedInfo.accent.x, selectedInfo.accent.y, selectedInfo.accent.z, 1.0f } },
        0.0f,
        XMFLOAT2(0.0f, 0.0f),
        0.95f * textScale);
    mFont->DrawString(mSpriteBatch.get(), selectedInfo.roleText, XMFLOAT2(screenW * 0.070f, screenH * 0.235f), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.55f * textScale);
    mFont->DrawString(mSpriteBatch.get(), L"기본 정보", XMFLOAT2(screenW * 0.070f, screenH * 0.355f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.58f * textScale);
    mFont->DrawString(mSpriteBatch.get(), selectedInfo.skill1, XMFLOAT2(screenW * 0.070f, screenH * 0.742f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.47f * textScale);
    mFont->DrawString(mSpriteBatch.get(), selectedInfo.skill2, XMFLOAT2(screenW * 0.170f, screenH * 0.742f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.47f * textScale);

    for (int i = 0; i < static_cast<int>(kClassInfos.size()); ++i)
    {
        const bool selected = i == selectedIndex;
        const auto& info = kClassInfos[i];
        const UiRect& row = mClassCardRects[i];
        const XMVECTORF32 nameColor = selected
            ? XMVECTORF32{ { info.accent.x, info.accent.y, info.accent.z, 1.0f } }
            : Colors::Gainsboro;

        const float rowTextScale = 0.54f * textScale;
        const XMVECTOR textSize = mFont->MeasureString(info.displayName);
        const float textH = XMVectorGetY(textSize) * rowTextScale;
        const float rowPadding = 18.0f * textScale;
        mFont->DrawString(
            mSpriteBatch.get(),
            info.displayName,
            XMFLOAT2(row.left + rowPadding, row.top + (row.bottom - row.top - textH) * 0.5f),
            nameColor,
            0.0f,
            XMFLOAT2(0.0f, 0.0f),
            rowTextScale);
    }

    drawCentered(L"캐릭터 생성", mConfirmButtonRect, Colors::White, 0.70f * textScale);

    mSpriteBatch->End();
}
