#include "UIManager.h"
#include "EclipseWalkerGame.h"

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

    // 1. 재질 생성 및 투명도 켜기
    res->CreateMaterial("UI_BgMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.2f, 0.2f, 0.2f, 0.8f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_HpMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.8f, 0.1f, 0.1f, 1.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_MpMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.1f, 0.4f, 0.9f, 1.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);

    if (auto mat = res->GetMaterial("UI_BgMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_HpMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_MpMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    auto setupRitem = [&](RenderItem* ritem) {
        ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = ritem->Geo->DrawArgs["quad"].IndexCount;
        ritem->StartIndexLocation = ritem->Geo->DrawArgs["quad"].StartIndexLocation;
        ritem->BaseVertexLocation = ritem->Geo->DrawArgs["quad"].BaseVertexLocation;
        };

    // =======================================================
    float leftEdgeX = -1.f;
    float hpBgScaleX = 0.4f;
    float mpBgScaleX = 0.3f;

    // 위치 고정 공식 
    float hpBgCenterX = leftEdgeX + hpBgScaleX;
    float mpBgCenterX = leftEdgeX + mpBgScaleX;
    // =======================================================

    // [HP 배경]
    auto hpBgRitem = std::make_unique<RenderItem>();
    hpBgRitem->Geo = res->mGeometries["quadGeo"].get();
    hpBgRitem->Mat = res->GetMaterial("UI_BgMat");
    hpBgRitem->ObjCBIndex = ritems.size();
    setupRitem(hpBgRitem.get());

    auto hpBgObj = std::make_unique<GameObject>();
    hpBgObj->SetScale(hpBgScaleX, 0.04f, 1.0f);
    hpBgObj->SetPosition(hpBgCenterX, 0.8f, 0.1f);
    hpBgObj->Ritem = hpBgRitem.get(); hpBgObj->Update();
    ritems.push_back(std::move(hpBgRitem));
    mUIObjects.push_back(std::move(hpBgObj));

    // [HP 채우기]
    auto hpFillRitem = std::make_unique<RenderItem>();
    hpFillRitem->Geo = res->mGeometries["quadGeo"].get();
    hpFillRitem->Mat = res->GetMaterial("UI_HpMat");
    hpFillRitem->ObjCBIndex = ritems.size();
    setupRitem(hpFillRitem.get());

    auto hpFillObj = std::make_unique<GameObject>();
    hpFillObj->SetScale(hpBgScaleX, 0.04f, 1.0f);
    hpFillObj->SetPosition(hpBgCenterX, 0.8f, 0.05f);
    hpFillObj->Ritem = hpFillRitem.get(); hpFillObj->Update();
    mHpBarFill = hpFillObj.get();
    ritems.push_back(std::move(hpFillRitem));
    mUIObjects.push_back(std::move(hpFillObj));

    // [MP 배경]
    auto mpBgRitem = std::make_unique<RenderItem>();
    mpBgRitem->Geo = res->mGeometries["quadGeo"].get();
    mpBgRitem->Mat = res->GetMaterial("UI_BgMat");
    mpBgRitem->ObjCBIndex = ritems.size();
    setupRitem(mpBgRitem.get());

    auto mpBgObj = std::make_unique<GameObject>();
    mpBgObj->SetScale(mpBgScaleX, 0.03f, 1.0f);
    mpBgObj->SetPosition(mpBgCenterX, 0.72f, 0.1f);
    mpBgObj->Ritem = mpBgRitem.get(); mpBgObj->Update();
    ritems.push_back(std::move(mpBgRitem));
    mUIObjects.push_back(std::move(mpBgObj));

    // [MP 채우기]
    auto mpFillRitem = std::make_unique<RenderItem>();
    mpFillRitem->Geo = res->mGeometries["quadGeo"].get();
    mpFillRitem->Mat = res->GetMaterial("UI_MpMat");
    mpFillRitem->ObjCBIndex = ritems.size();
    setupRitem(mpFillRitem.get());

    auto mpFillObj = std::make_unique<GameObject>();
    mpFillObj->SetScale(mpBgScaleX, 0.03f, 1.0f);
    mpFillObj->SetPosition(mpBgCenterX, 0.72f, 0.05f);
    mpFillObj->Ritem = mpFillRitem.get(); mpFillObj->Update();
    mMpBarFill = mpFillObj.get();
    ritems.push_back(std::move(mpFillRitem));
    mUIObjects.push_back(std::move(mpFillObj));
}

void UIManager::Update(float currentHp, float maxHp, float currentMp, float maxMp)
{
    float hpRatio = maxHp > 0.0f ? (currentHp / maxHp) : 0.0f;
    float mpRatio = maxMp > 0.0f ? (currentMp / maxMp) : 0.0f;

    float leftEdgeX = -1.f;

    // [HP 업데이트]
    if (mHpBarFill)
    {
        float hpMaxScale = 0.4f;
        float currentHpScale = hpMaxScale * hpRatio;

        mHpBarFill->SetScale(currentHpScale, 0.04f, 1.0f);

        float currentHpCenterX = leftEdgeX + currentHpScale;
        mHpBarFill->SetPosition(currentHpCenterX, 0.8f, 0.05f);
    }

    // [MP 업데이트]
    if (mMpBarFill)
    {
        float mpMaxScale = 0.3f;
        float currentMpScale = mpMaxScale * mpRatio;

        mMpBarFill->SetScale(currentMpScale, 0.03f, 1.0f);
        float currentMpCenterX = leftEdgeX + currentMpScale;
        mMpBarFill->SetPosition(currentMpCenterX, 0.72f, 0.05f);
    }

    for (auto& obj : mUIObjects)
    {
        obj->Update();
    }
}