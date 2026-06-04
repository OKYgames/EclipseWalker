#include "CharSelectScene.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Stage1Scene.h"
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
    constexpr float kUiPlaneZ = 0.0f;
    constexpr float kMenuFovY = 0.25f * DirectX::XM_PI;

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
    OutputDebugStringA("\n[Character Select Scene] Enter without 3D preview.\n");

    mLeftKeyPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    mRightKeyPressed = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    mEnterKeyPressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    mMousePressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    mClassCardObjects = {};
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
    loadTextureIfExists("UI_Skill_Warrior_EarthquakeSlam", L"Textures/UI/Skill_Warrior_EarthquakeSlam_512x512.dds");
    loadTextureIfExists("UI_Skill_Warrior_GreatswordSummon", L"Textures/UI/Skill_Warrior_GreatswordSummon_512x512.dds");
    loadTextureIfExists("UI_Skill_Mage_HealingLight", L"Textures/UI/Skill_Mage_HealingLight_512x512.dds");
    loadTextureIfExists("UI_Skill_Mage_Meteor", L"Textures/UI/Skill_Mage_Meteor_512x512.dds");
    loadTextureIfExists("UI_Skill_Archer_WindImbuement", L"Textures/UI/Skill_Archer_WindImbuement_512x512.dds");
    loadTextureIfExists("UI_Skill_Archer_ArrowRain", L"Textures/UI/Skill_Archer_ArrowRain_512x512.dds");

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

    mGame->BuildDescriptorHeaps();

    const auto viewport = mGame->GetScreenViewport();
    const float screenW = (std::max)(1.0f, viewport.Width);
    const float screenH = (std::max)(1.0f, viewport.Height);

    const float cardW = (std::min)(screenW * 0.22f, 310.0f);
    const float cardH = (std::min)(screenH * 0.40f, 320.0f);
    const float cardTop = screenH * 0.255f;
    const std::array<float, 3> cardCenters = { screenW * 0.235f, screenW * 0.500f, screenW * 0.765f };
    for (int i = 0; i < 3; ++i)
    {
        mClassCardRects[i] = {
            cardCenters[i] - cardW * 0.5f,
            cardTop,
            cardCenters[i] + cardW * 0.5f,
            cardTop + cardH
        };
    }

    const UiRect fullScreen = { 0.0f, 0.0f, screenW, screenH };
    const UiRect titlePanel = { screenW * 0.245f, screenH * 0.075f, screenW * 0.755f, screenH * 0.185f };
    const UiRect infoPanel = { screenW * 0.120f, screenH * 0.710f, screenW * 0.880f, screenH * 0.925f };
    mConfirmButtonRect = { screenW * 0.675f, screenH * 0.820f, screenW * 0.855f, screenH * 0.895f };
    const UiRect skillIcon1Rect = { screenW * 0.185f, screenH * 0.755f, screenW * 0.245f, screenH * 0.862f };
    const UiRect skillIcon2Rect = { screenW * 0.285f, screenH * 0.755f, screenW * 0.345f, screenH * 0.862f };

    auto applyRectToObject = [&](GameObject* object, const UiRect& rect, float z)
        {
            if (object == nullptr)
            {
                return;
            }

            const float distance = std::abs(kUiPlaneZ - kCameraZ);
            const float halfViewH = std::tan(kMenuFovY * 0.5f) * distance;
            const float halfViewW = halfViewH * (screenW / screenH);

            auto toWorldX = [&](float px)
                {
                    return ((px / screenW) * 2.0f - 1.0f) * halfViewW;
                };
            auto toWorldY = [&](float py)
                {
                    return (1.0f - (py / screenH) * 2.0f) * halfViewH;
                };

            const float left = toWorldX(rect.left);
            const float right = toWorldX(rect.right);
            const float top = toWorldY(rect.top);
            const float bottom = toWorldY(rect.bottom);

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

    createQuad("MainMenuMat", fullScreen, 0.040f);
    createQuad("CS_DimMat", fullScreen, 0.020f);
    createQuad("CS_TitlePanelMat", titlePanel, -0.002f);
    createQuad("CS_InfoPanelMat", infoPanel, -0.010f);
    createQuad("CS_SelectedCardMat", ExpandRect(mClassCardRects[GetClassIndex(mGame->GetSelectedPlayerClass())], 9.0f), -0.028f, &mSelectionHighlightObj);

    for (int i = 0; i < 3; ++i)
    {
        createQuad("CS_CardMat", mClassCardRects[i], -0.018f, &mClassCardObjects[i]);
    }

    createQuad("CS_ButtonMat", mConfirmButtonRect, -0.030f);
    createQuad("CS_Skill_Mage_HealingLightMat", skillIcon1Rect, -0.040f, nullptr, &mSkillIcon1Ritem);
    createQuad("CS_Skill_Mage_MeteorMat", skillIcon2Rect, -0.040f, nullptr, &mSkillIcon2Ritem);
}

void CharSelectScene::UpdateSelectionVisuals()
{
    const int selectedIndex = GetClassIndex(mGame->GetSelectedPlayerClass());
    const auto& selectedInfo = GetClassInfo(mGame->GetSelectedPlayerClass());

    if (mSelectionHighlightObj != nullptr)
    {
        const auto viewport = mGame->GetScreenViewport();
        const float screenW = (std::max)(1.0f, viewport.Width);
        const float screenH = (std::max)(1.0f, viewport.Height);
        const float distance = std::abs(kUiPlaneZ - kCameraZ);
        const float halfViewH = std::tan(kMenuFovY * 0.5f) * distance;
        const float halfViewW = halfViewH * (screenW / screenH);
        const UiRect highlightRect = ExpandRect(mClassCardRects[selectedIndex], 9.0f);

        auto toWorldX = [&](float px)
            {
                return ((px / screenW) * 2.0f - 1.0f) * halfViewW;
            };
        auto toWorldY = [&](float py)
            {
                return (1.0f - (py / screenH) * 2.0f) * halfViewH;
            };

        const float left = toWorldX(highlightRect.left);
        const float right = toWorldX(highlightRect.right);
        const float top = toWorldY(highlightRect.top);
        const float bottom = toWorldY(highlightRect.bottom);

        mSelectionHighlightObj->SetScale(std::abs(right - left) * 0.5f, std::abs(top - bottom) * 0.5f, 1.0f);
        mSelectionHighlightObj->SetPosition((left + right) * 0.5f, (top + bottom) * 0.5f, -0.028f);
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
    const float x = static_cast<float>(cursor.x);
    const float y = static_cast<float>(cursor.y);

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
    mGame->ChangeScene(std::make_unique<Stage1Scene>(mGame));
    return true;
}

void CharSelectScene::Update(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

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

    const auto viewport = mGame->GetScreenViewport();
    const float screenW = (std::max)(1.0f, viewport.Width);
    const float screenH = (std::max)(1.0f, viewport.Height);
    const UiRect titleTextRect = { screenW * 0.245f, screenH * 0.078f, screenW * 0.755f, screenH * 0.160f };
    drawCentered(L"CHARACTER SELECT", titleTextRect, Colors::White, 1.0f);

    const int selectedIndex = GetClassIndex(mGame->GetSelectedPlayerClass());
    for (int i = 0; i < static_cast<int>(kClassInfos.size()); ++i)
    {
        const bool selected = i == selectedIndex;
        const auto& info = kClassInfos[i];
        const UiRect& card = mClassCardRects[i];
        const XMVECTORF32 nameColor = selected
            ? XMVECTORF32{ { info.accent.x, info.accent.y, info.accent.z, 1.0f } }
            : Colors::White;

        UiRect nameRect = { card.left, card.top + 34.0f, card.right, card.top + 92.0f };
        drawCentered(info.displayName, nameRect, nameColor, 0.95f);

        UiRect roleRect = { card.left + 18.0f, card.top + 112.0f, card.right - 18.0f, card.top + 164.0f };
        drawCentered(info.roleText, roleRect, selected ? Colors::LightYellow : Colors::LightGray, 0.58f);

        UiRect skill1Rect = { card.left, card.top + 202.0f, card.right, card.top + 238.0f };
        UiRect skill2Rect = { card.left, card.top + 244.0f, card.right, card.top + 280.0f };
        drawCentered(info.skill1, skill1Rect, Colors::Gainsboro, 0.56f);
        drawCentered(info.skill2, skill2Rect, Colors::Gainsboro, 0.56f);
    }

    const auto& selectedInfo = GetClassInfo(mGame->GetSelectedPlayerClass());
    mFont->DrawString(mSpriteBatch.get(), L"선택한 직업", XMFLOAT2(screenW * 0.405f, screenH * 0.746f), Colors::LightGray, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.58f);
    mFont->DrawString(
        mSpriteBatch.get(),
        selectedInfo.displayName,
        XMFLOAT2(screenW * 0.405f, screenH * 0.790f),
        XMVECTORF32{ { selectedInfo.accent.x, selectedInfo.accent.y, selectedInfo.accent.z, 1.0f } },
        0.0f,
        XMFLOAT2(0.0f, 0.0f),
        0.86f);
    mFont->DrawString(mSpriteBatch.get(), selectedInfo.roleText, XMFLOAT2(screenW * 0.405f, screenH * 0.850f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.60f);
    mFont->DrawString(mSpriteBatch.get(), selectedInfo.skill1, XMFLOAT2(screenW * 0.185f, screenH * 0.868f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.48f);
    mFont->DrawString(mSpriteBatch.get(), selectedInfo.skill2, XMFLOAT2(screenW * 0.285f, screenH * 0.868f), Colors::White, 0.0f, XMFLOAT2(0.0f, 0.0f), 0.48f);

    drawCentered(L"선택 완료", mConfirmButtonRect, Colors::White, 0.64f);

    mSpriteBatch->End();
}
