#include "LoadingScene.h"

#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "MathHelper.h"
#include <RenderTargetState.h>
#include <ResourceUploadBatch.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    constexpr float kLoadingBackgroundScaleX = 8.0f;
    constexpr float kLoadingBackgroundScaleY = 4.5f;
    constexpr float kLoadingBackgroundZ = 0.0f;
}

void LoadingScene::TrackOwned(GameObject* object, RenderItem* renderItem)
{
    if (object != nullptr)
    {
        mOwnedObjects.push_back(object);
    }

    if (renderItem != nullptr)
    {
        mOwnedRenderItems.push_back(renderItem);
    }
}

void LoadingScene::Enter()
{
    mGame->FlushCommandQueue();
    OutputDebugStringA("[LoadingScene] Enter\n");

    auto* res = mGame->GetResources();
    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects();
    mOwnedObjects.clear();
    mOwnedRenderItems.clear();
    mHiddenRenderItems.clear();

    for (const auto& renderItem : ritems)
    {
        if (renderItem == nullptr)
        {
            continue;
        }

        mHiddenRenderItems.push_back({ renderItem.get(), renderItem->Visible });
        renderItem->Visible = false;
        renderItem->NumFramesDirty = gNumFrameResources;
    }

    auto* camera = mGame->GetCamera();
    camera->SetPosition(0.0f, 0.0f, -10.0f);
    camera->LookAt(
        XMFLOAT3(0.0f, 0.0f, -10.0f),
        XMFLOAT3(0.0f, 0.0f, 0.0f),
        XMFLOAT3(0.0f, 1.0f, 0.0f));
    camera->UpdateViewMatrix();

    const char* sourceMatName = res->GetMaterial("MainMenuMat") != nullptr ? "MainMenuMat" : "TitleMat";
    if (res->GetMaterial("LoadingBackgroundMat") == nullptr)
    {
        const std::string textureName =
            (sourceMatName == std::string("MainMenuMat") && res->GetTexture("MainMenuTex") != nullptr)
            ? "MainMenuTex"
            : "TitleTex";
        res->CreateMaterial(
            "LoadingBackgroundMat",
            static_cast<int>(res->mMaterials.size()),
            textureName,
            "",
            "",
            "",
            XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f),
            XMFLOAT3(0.0f, 0.0f, 0.0f),
            1.0f);
        if (auto* mat = res->GetMaterial("LoadingBackgroundMat"))
        {
            mat->NumFramesDirty = gNumFrameResources;
        }
    }

    auto backgroundRitem = std::make_unique<RenderItem>();
    backgroundRitem->TexTransform = MathHelper::Identity4x4();
    backgroundRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    backgroundRitem->NumFramesDirty = gNumFrameResources;
    backgroundRitem->Mat = res->GetMaterial("LoadingBackgroundMat");
    backgroundRitem->Geo = res->mGeometries["quadGeo"].get();
    backgroundRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    backgroundRitem->IndexCount = backgroundRitem->Geo->DrawArgs["quad"].IndexCount;
    backgroundRitem->StartIndexLocation = backgroundRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    backgroundRitem->BaseVertexLocation = backgroundRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

    auto backgroundObj = std::make_unique<GameObject>();
    backgroundObj->Ritem = backgroundRitem.get();
    backgroundObj->SetScale(kLoadingBackgroundScaleX, kLoadingBackgroundScaleY, 1.0f);
    backgroundObj->SetPosition(0.0f, 0.0f, kLoadingBackgroundZ);
    backgroundObj->Update();

    TrackOwned(backgroundObj.get(), backgroundRitem.get());
    ritems.push_back(std::move(backgroundRitem));
    gameObjects.push_back(std::move(backgroundObj));

    InitializeUiResources();
    mElapsedSeconds = 0.0f;
    mHasPresentedFrame = false;
}

void LoadingScene::Exit()
{
    mGame->FlushCommandQueue();

    auto& ritems = mGame->GetRitems();
    auto& gameObjects = mGame->GetGameObjects();

    gameObjects.erase(
        std::remove_if(
            gameObjects.begin(),
            gameObjects.end(),
            [&](const std::unique_ptr<GameObject>& object)
            {
                return std::find(mOwnedObjects.begin(), mOwnedObjects.end(), object.get()) != mOwnedObjects.end();
            }),
        gameObjects.end());

    ritems.erase(
        std::remove_if(
            ritems.begin(),
            ritems.end(),
            [&](const std::unique_ptr<RenderItem>& renderItem)
            {
                return std::find(mOwnedRenderItems.begin(), mOwnedRenderItems.end(), renderItem.get()) != mOwnedRenderItems.end();
            }),
        ritems.end());

    for (UINT i = 0; i < ritems.size(); ++i)
    {
        ritems[i]->ObjCBIndex = i;
        ritems[i]->NumFramesDirty = gNumFrameResources;
    }

    for (const HiddenRenderItemState& hiddenState : mHiddenRenderItems)
    {
        if (hiddenState.Item == nullptr)
        {
            continue;
        }

        hiddenState.Item->Visible = hiddenState.WasVisible;
        hiddenState.Item->NumFramesDirty = gNumFrameResources;
    }

    mOwnedObjects.clear();
    mOwnedRenderItems.clear();
    mHiddenRenderItems.clear();
}

void LoadingScene::InitializeUiResources()
{
    auto* device = mGame->GetDevice();
    auto* cmdQueue = mGame->GetCommandQueue();

    if (!mGraphicsMemory)
    {
        mGraphicsMemory = std::make_unique<GraphicsMemory>(device);
    }

    if (!mFontHeap)
    {
        mFontHeap = std::make_unique<DescriptorHeap>(
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

        auto uploadFinished = resourceUpload.End(cmdQueue);
        uploadFinished.wait();
    }
}

void LoadingScene::Update(const GameTimer& gt)
{
    mElapsedSeconds += gt.DeltaTime();

    if (mGraphicsMemory)
    {
        mGraphicsMemory->Commit(mGame->GetCommandQueue());
    }

    if (!mGame->HasPendingSceneChange())
    {
        return;
    }

    if (!mHasPresentedFrame && mElapsedSeconds < 0.05f)
    {
        return;
    }

    mGame->FinalizePendingSceneChange();
}

void LoadingScene::EnforceHiddenGameplayRenderItems()
{
    for (const HiddenRenderItemState& hiddenState : mHiddenRenderItems)
    {
        if (hiddenState.Item == nullptr)
        {
            continue;
        }

        if (hiddenState.Item->Visible)
        {
            hiddenState.Item->Visible = false;
            hiddenState.Item->NumFramesDirty = gNumFrameResources;
        }
    }
}

void LoadingScene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);

    mHasPresentedFrame = true;

    if (!mFont || !mSpriteBatch || !mFontHeap)
    {
        return;
    }

    std::wstring loadingText = mGame->GetPendingSceneLoadingLabel();
    if (loadingText.empty())
    {
        loadingText = L"LOADING";
    }

    const int dotCount = static_cast<int>(std::fmod(mElapsedSeconds * 3.0f, 4.0f));
    for (int i = 0; i < dotCount; ++i)
    {
        loadingText += L".";
    }

    auto* cmdList = mGame->GetCommandList();
    ID3D12DescriptorHeap* heaps[] = { mFontHeap->Heap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    mSpriteBatch->SetViewport(mGame->GetScreenViewport());
    mSpriteBatch->Begin(cmdList);

    const auto viewport = mGame->GetScreenViewport();
    const XMVECTOR titleSize = mFont->MeasureString(loadingText.c_str());
    const float titleScale = 1.1f;
    const float titleWidth = XMVectorGetX(titleSize) * titleScale;
    const float titleHeight = XMVectorGetY(titleSize) * titleScale;
    const XMFLOAT2 titlePos(
        viewport.Width * 0.5f - titleWidth * 0.5f,
        viewport.Height * 0.52f - titleHeight * 0.5f);

    const wchar_t* subtitle = L"PLEASE WAIT";
    const XMVECTOR subtitleSize = mFont->MeasureString(subtitle);
    const float subtitleScale = 0.6f;
    const float subtitleWidth = XMVectorGetX(subtitleSize) * subtitleScale;
    const float subtitleHeight = XMVectorGetY(subtitleSize) * subtitleScale;
    const XMFLOAT2 subtitlePos(
        viewport.Width * 0.5f - subtitleWidth * 0.5f,
        titlePos.y + titleHeight + 22.0f - subtitleHeight * 0.5f);

    mFont->DrawString(
        mSpriteBatch.get(),
        loadingText.c_str(),
        XMFLOAT2(titlePos.x + 2.0f, titlePos.y + 2.0f),
        Colors::Black,
        0.0f,
        XMFLOAT2(0.0f, 0.0f),
        titleScale);
    mFont->DrawString(
        mSpriteBatch.get(),
        loadingText.c_str(),
        titlePos,
        Colors::White,
        0.0f,
        XMFLOAT2(0.0f, 0.0f),
        titleScale);

    mFont->DrawString(
        mSpriteBatch.get(),
        subtitle,
        XMFLOAT2(subtitlePos.x + 1.0f, subtitlePos.y + 1.0f),
        Colors::Black,
        0.0f,
        XMFLOAT2(0.0f, 0.0f),
        subtitleScale);
    mFont->DrawString(
        mSpriteBatch.get(),
        subtitle,
        subtitlePos,
        Colors::LightGray,
        0.0f,
        XMFLOAT2(0.0f, 0.0f),
        subtitleScale);

    mSpriteBatch->End();
}
