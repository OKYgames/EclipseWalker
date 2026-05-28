#include "DamageTextRenderer.h"

#include "Camera.h"
#include "EclipseWalkerGame.h"
#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <string>

using namespace DirectX;

DamageTextRenderer::DamageTextRenderer(EclipseWalkerGame* game)
    : mGame(game)
{
}

void DamageTextRenderer::Initialize()
{
    auto* device = mGame != nullptr ? mGame->GetDevice() : nullptr;
    auto* cmdQueue = mGame != nullptr ? mGame->GetCommandQueue() : nullptr;
    if (device == nullptr || cmdQueue == nullptr)
    {
        return;
    }

    try
    {
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

            auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
            uploadResourcesFinished.wait();
        }
    }
    catch (const std::exception& e)
    {
        std::string log = "[DamageText] Failed to initialize font renderer: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());

        mFont.reset();
        mSpriteBatch.reset();
        mFontHeap.reset();
    }
}

void DamageTextRenderer::Reset()
{
    mTexts.clear();
}

void DamageTextRenderer::Update(float dt)
{
    for (auto& text : mTexts)
    {
        text.Age += dt;
        text.WorldPosition.y += text.RiseSpeed * dt;
    }

    mTexts.erase(
        std::remove_if(mTexts.begin(), mTexts.end(),
            [](const DamageText& text)
            {
                return text.Age >= text.Lifetime;
            }),
        mTexts.end());
}

void DamageTextRenderer::Draw()
{
    if (mTexts.empty() || !mFont || !mSpriteBatch || !mFontHeap || mGame == nullptr)
    {
        return;
    }

    auto* cmdList = mGame->GetCommandList();
    if (cmdList == nullptr)
    {
        return;
    }

    try
    {
        ID3D12DescriptorHeap* heaps[] = { mFontHeap->Heap() };
        cmdList->SetDescriptorHeaps(1, heaps);

        mSpriteBatch->SetViewport(mGame->GetScreenViewport());
        mSpriteBatch->Begin(cmdList);

        for (const DamageText& text : mTexts)
        {
            XMFLOAT2 screenPosition{};
            if (!ProjectToScreen(text.WorldPosition, screenPosition))
            {
                continue;
            }

            const std::wstring label = std::to_wstring(text.Damage);
            const float fadeStart = text.Lifetime * 0.58f;
            const float fadeRatio = text.Age <= fadeStart
                ? 1.0f
                : (std::clamp)((text.Lifetime - text.Age) / (text.Lifetime - fadeStart), 0.0f, 1.0f);
            const float popScale = text.Age < 0.10f ? (1.18f - text.Age * 1.8f) : 1.0f;
            const float scale = text.Scale * popScale;

            const XMVECTOR textSize = mFont->MeasureString(label.c_str());
            const XMFLOAT2 origin(
                XMVectorGetX(textSize) * 0.5f,
                XMVectorGetY(textSize) * 0.5f);
            const XMFLOAT2 drawPosition(screenPosition.x, screenPosition.y);
            const XMVECTOR shadowColor = XMVectorSet(0.0f, 0.0f, 0.0f, 0.78f * fadeRatio);
            const XMVECTOR mainColor = GetColor(text.TextType, fadeRatio);

            constexpr XMFLOAT2 shadowOffsets[] =
            {
                { -1.5f, 0.0f },
                { 1.5f, 0.0f },
                { 0.0f, -1.5f },
                { 0.0f, 1.5f }
            };

            for (const XMFLOAT2& offset : shadowOffsets)
            {
                mFont->DrawString(
                    mSpriteBatch.get(),
                    label.c_str(),
                    XMFLOAT2(drawPosition.x + offset.x, drawPosition.y + offset.y),
                    shadowColor,
                    0.0f,
                    origin,
                    scale);
            }

            mFont->DrawString(
                mSpriteBatch.get(),
                label.c_str(),
                drawPosition,
                mainColor,
                0.0f,
                origin,
                scale);
        }

        mSpriteBatch->End();
    }
    catch (const std::exception& e)
    {
        std::string log = "[DamageText] Failed to draw damage text: ";
        log += e.what();
        log += "\n";
        OutputDebugStringA(log.c_str());
    }
}

void DamageTextRenderer::Spawn(const XMFLOAT3& worldPosition, float damage, Type type)
{
    if (damage <= 0.0f)
    {
        return;
    }

    DamageText text;
    text.WorldPosition = worldPosition;
    text.Damage = (std::max)(1, static_cast<int>(std::round(damage)));
    text.TextType = type;
    text.RiseSpeed = (type == Type::Outgoing) ? 0.34f : 0.28f;
    text.Scale = (type == Type::Outgoing) ? 1.15f : 1.05f;
    mTexts.push_back(text);

    if (mTexts.size() > 64)
    {
        mTexts.erase(mTexts.begin());
    }
}

void DamageTextRenderer::SpawnOutgoing(const XMFLOAT3& worldPosition, float damage)
{
    Spawn(worldPosition, damage, Type::Outgoing);
}

void DamageTextRenderer::SpawnIncoming(const XMFLOAT3& worldPosition, float damage)
{
    Spawn(worldPosition, damage, Type::Incoming);
}

bool DamageTextRenderer::ProjectToScreen(const XMFLOAT3& worldPosition, XMFLOAT2& outScreenPosition) const
{
    if (mGame == nullptr || mGame->GetCamera() == nullptr)
    {
        return false;
    }

    Camera* camera = mGame->GetCamera();
    camera->UpdateViewMatrix();

    const XMMATRIX viewProj = camera->GetViewProj();
    const XMVECTOR ndc = XMVector3TransformCoord(XMLoadFloat3(&worldPosition), viewProj);
    const float x = XMVectorGetX(ndc);
    const float y = XMVectorGetY(ndc);
    const float z = XMVectorGetZ(ndc);
    if (z < 0.0f || z > 1.0f || x < -1.25f || x > 1.25f || y < -1.25f || y > 1.25f)
    {
        return false;
    }

    const auto viewport = mGame->GetScreenViewport();
    outScreenPosition.x = viewport.TopLeftX + (x + 1.0f) * 0.5f * viewport.Width;
    outScreenPosition.y = viewport.TopLeftY + (1.0f - y) * 0.5f * viewport.Height;
    return true;
}

XMVECTOR DamageTextRenderer::GetColor(Type type, float alpha) const
{
    if (type == Type::Incoming)
    {
        return XMVectorSet(1.0f, 0.08f, 0.04f, alpha);
    }

    return XMVectorSet(1.0f, 0.86f, 0.10f, alpha);
}
