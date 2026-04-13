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
    res->CreateMaterial("UI_ChatLogMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.05f, 0.07f, 0.09f, 0.72f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    res->CreateMaterial("UI_ChatInputMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.14f, 0.16f, 0.2f, 0.88f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);

    if (auto mat = res->GetMaterial("UI_BgMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_HpMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_MpMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ChatLogMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }
    if (auto mat = res->GetMaterial("UI_ChatInputMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    mChatLogMat = res->GetMaterial("UI_ChatLogMat");
    mChatInputMat = res->GetMaterial("UI_ChatInputMat");

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

    auto chatLogRitem = std::make_unique<RenderItem>();
    chatLogRitem->Geo = res->mGeometries["quadGeo"].get();
    chatLogRitem->Mat = res->GetMaterial("UI_ChatLogMat");
    chatLogRitem->ObjCBIndex = ritems.size();
    setupRitem(chatLogRitem.get());

    auto chatLogObj = std::make_unique<GameObject>();
    chatLogObj->SetScale(0.38f, 0.18f, 1.0f);
    chatLogObj->SetPosition(-0.62f, -0.6f, 0.11f);
    chatLogObj->Ritem = chatLogRitem.get();
    chatLogObj->Update();
    mChatLogBg = chatLogObj.get();
    ritems.push_back(std::move(chatLogRitem));
    mUIObjects.push_back(std::move(chatLogObj));

    auto chatInputRitem = std::make_unique<RenderItem>();
    chatInputRitem->Geo = res->mGeometries["quadGeo"].get();
    chatInputRitem->Mat = res->GetMaterial("UI_ChatInputMat");
    chatInputRitem->ObjCBIndex = ritems.size();
    setupRitem(chatInputRitem.get());

    auto chatInputObj = std::make_unique<GameObject>();
    chatInputObj->SetScale(0.38f, 0.055f, 1.0f);
    chatInputObj->SetPosition(-0.62f, -0.87f, 0.11f);
    chatInputObj->Ritem = chatInputRitem.get();
    chatInputObj->Update();
    mChatInputBg = chatInputObj.get();
    ritems.push_back(std::move(chatInputRitem));
    mUIObjects.push_back(std::move(chatInputObj));

    // 이펙트용 재질 2개 생성
    res->CreateMaterial("UI_FlashMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_FlashMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    res->CreateMaterial("UI_ScreenBgMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.95f, 0.9f, 0.72f, 0.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f);
    if (auto mat = res->GetMaterial("UI_ScreenBgMat")) { mat->IsTransparent = 1; mat->NumFramesDirty = 3; }

    auto bgRitem = std::make_unique<RenderItem>();
    bgRitem->Geo = res->mGeometries["quadGeo"].get();
    bgRitem->Mat = res->GetMaterial("UI_ScreenBgMat");
    bgRitem->ObjCBIndex = ritems.size();
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
    flashRitem->ObjCBIndex = ritems.size();

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

void UIManager::InitializeEffect(Material* flashMat, Material* bgMat, GameObject* flashObj, GameObject* screenBgObj)
{
    mFlashMat = flashMat;
    mBgMat = bgMat;
    mFlashObj = flashObj;
    mScreenBgObj = screenBgObj;

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

    if (mFlashMat) {
        mFlashMat->DiffuseAlbedo = XMFLOAT4(1.0f, 0.95f, 0.82f, 0.0f);
        mFlashMat->NumFramesDirty = 3;
    }

    if (mBgMat) {
        mBgMat->DiffuseAlbedo = XMFLOAT4(0.95f, 0.9f, 0.72f, 0.0f);
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

void UIManager::UpdateEffect(float dt)
{
    if (!mIsFlashActive) return;

    mCurrentTime += dt;
    float bgAlpha = 0.0f;
    float flashAlpha = 0.0f;

    if (mCurrentTime < 0.22f)
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
        mBgMat->DiffuseAlbedo.w = bgAlpha;
        mBgMat->NumFramesDirty = 3;
    }

    if (mFlashMat) {
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
