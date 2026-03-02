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

    // ==========================================
    // 1. UI용 단색 재질(Material) 3개 생성
    // ==========================================
    // (이름, 인덱스, 텍스처, 노말, 거칠기, 금속성, 색상, 프레넬, 러프니스)
    res->CreateMaterial("UI_BgMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.2f, 0.2f, 0.2f, 0.8f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f); // 배경(회색)

    res->CreateMaterial("UI_HpMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.8f, 0.1f, 0.1f, 1.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f); // 체력(빨강)

    res->CreateMaterial("UI_MpMat", res->mMaterials.size(), "white", "", "", "",
        DirectX::XMFLOAT4(0.1f, 0.4f, 0.9f, 1.0f), DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f), 0.5f); // 마나(파랑)

    // ==========================================
    // 2. HP 바 조립 (배경 + 빨간 피)
    // ==========================================
    // [HP 배경]
    auto hpBgRitem = std::make_unique<RenderItem>();
    hpBgRitem->Geo = res->mGeometries["quadGeo"].get();
    hpBgRitem->Mat = res->GetMaterial("UI_BgMat");
    hpBgRitem->ObjCBIndex = ritems.size();

    auto hpBgObj = std::make_unique<GameObject>();
    hpBgObj->SetScale(0.4f, 0.04f, 1.0f);
    hpBgObj->SetPosition(-0.7f, 0.8f, 0.0f); // 화면 좌상단 (X, Y)
    hpBgObj->Ritem = hpBgRitem.get(); hpBgObj->Update();

    ritems.push_back(std::move(hpBgRitem));
    mUIObjects.push_back(std::move(hpBgObj));

    // [HP 채우기 (빨강)]
    auto hpFillRitem = std::make_unique<RenderItem>();
    hpFillRitem->Geo = res->mGeometries["quadGeo"].get();
    hpFillRitem->Mat = res->GetMaterial("UI_HpMat");
    hpFillRitem->ObjCBIndex = ritems.size();

    auto hpFillObj = std::make_unique<GameObject>();
    hpFillObj->SetScale(0.4f, 0.04f, 1.0f);
    hpFillObj->SetPosition(-0.7f, 0.8f, -0.01f); // 배경보다 살짝 앞(-Z)에 배치
    hpFillObj->Ritem = hpFillRitem.get(); hpFillObj->Update();

    mHpBarFill = hpFillObj.get(); // 포인터 저장!
    ritems.push_back(std::move(hpFillRitem));
    mUIObjects.push_back(std::move(hpFillObj));

    // ==========================================
    // 3. MP 바 조립 (배경 + 파란 마나)
    // ==========================================
    // [MP 배경]
    auto mpBgRitem = std::make_unique<RenderItem>();
    mpBgRitem->Geo = res->mGeometries["quadGeo"].get();
    mpBgRitem->Mat = res->GetMaterial("UI_BgMat");
    mpBgRitem->ObjCBIndex = ritems.size();

    auto mpBgObj = std::make_unique<GameObject>();
    mpBgObj->SetScale(0.3f, 0.03f, 1.0f); // 마나바는 체력바보다 조금 짧고 얇게!
    mpBgObj->SetPosition(-0.75f, 0.72f, 0.0f); // 체력바 바로 아래에 배치
    mpBgObj->Ritem = mpBgRitem.get(); mpBgObj->Update();

    ritems.push_back(std::move(mpBgRitem));
    mUIObjects.push_back(std::move(mpBgObj));

    // [MP 채우기 (파랑)]
    auto mpFillRitem = std::make_unique<RenderItem>();
    mpFillRitem->Geo = res->mGeometries["quadGeo"].get();
    mpFillRitem->Mat = res->GetMaterial("UI_MpMat");
    mpFillRitem->ObjCBIndex = ritems.size();

    auto mpFillObj = std::make_unique<GameObject>();
    mpFillObj->SetScale(0.3f, 0.03f, 1.0f);
    mpFillObj->SetPosition(-0.75f, 0.72f, -0.01f);
    mpFillObj->Ritem = mpFillRitem.get(); mpFillObj->Update();

    mMpBarFill = mpFillObj.get(); 
    ritems.push_back(std::move(mpFillRitem));
    mUIObjects.push_back(std::move(mpFillObj));
}

void UIManager::Update(float currentHp, float maxHp, float currentMp, float maxMp)
{
    // 1. 비율 계산 (0.0 ~ 1.0)
    float hpRatio = maxHp > 0.0f ? (currentHp / maxHp) : 0.0f;
    float mpRatio = maxMp > 0.0f ? (currentMp / maxMp) : 0.0f;

    // 2. HP 바 갱신 (왼쪽 정렬)
    if (mHpBarFill)
    {
        float maxWidth = 0.4f;
        float currentWidth = maxWidth * hpRatio;
        mHpBarFill->SetScale(currentWidth, 0.04f, 1.0f);
        float offset = (maxWidth - currentWidth) / 2.0f;
        mHpBarFill->SetPosition(-0.7f - offset, 0.8f, -0.01f);
        mHpBarFill->Update();
    }

    // 3. MP 바 갱신 (왼쪽 정렬)
    if (mMpBarFill)
    {
        float maxWidth = 0.3f;
        float currentWidth = maxWidth * mpRatio;
        mMpBarFill->SetScale(currentWidth, 0.03f, 1.0f);

        float offset = (maxWidth - currentWidth) / 2.0f;
        mMpBarFill->SetPosition(-0.75f - offset, 0.72f, -0.01f);
        mMpBarFill->Update();
    }
}