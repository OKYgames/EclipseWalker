#pragma once

#include "ChatController.h"
#include "MapSystem.h"
#include "Player.h"
#include "RedPortalEffect.h"
#include "Scene.h"
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <memory>
#include <string>
#include <vector>

class GameObject;
struct RenderItem;

class VillageScene : public Scene
{
public:
    VillageScene(EclipseWalkerGame* game);

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnCharInput(WPARAM charCode) override;
    virtual void OnTextInput(const std::wstring& text) override;
    virtual void OnCompositionInput(const std::wstring& text, bool isFinal) override;
    virtual void OnMouseWheel(short delta, int x, int y) override;

private:
public:
    enum class ShopCategory
    {
        Weapon,
        Armor,
        Potion
    };

    struct ShopItem
    {
        int ItemId = 0;
        ShopCategory Category = ShopCategory::Armor;
        std::wstring Name;
        std::wstring ClassRestriction;
        PlayerClass AllowedClass = PlayerClass::None;
        int RequiredLevel = 1;
        int Price = 0;
        std::string IconTextureName;
        bool Purchased = false;
    };

private:
    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();
    void LogPlayerPosition(const DirectX::XMFLOAT3& position);
    void CreateShopKeeperNpc();
    void InitializeShopTextureAssets();
    void InitializeShopUiResources();
    void InitializeShopData();
    void RebuildFilteredShopItems();
    void ClampShopScroll();
    void AdjustShopScroll(int rowDelta);
    void HandleShopMouseClick(float x, float y);
    bool TryPurchaseVisibleShopItem(int visibleRow);
    void SetShopStatusMessage(const std::wstring& message, const DirectX::XMFLOAT4& color, float durationSeconds = 1.75f);
    void DrawShopOverlay();

    std::unique_ptr<MapSystem> mMapSystem;
    std::unique_ptr<RedPortalEffect> mPortalEffect;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    RenderItem* mCloudLayerA = nullptr;
    RenderItem* mCloudLayerB = nullptr;
    ChatController mChatController;
    bool mBackKeyPressed = false;
    bool mStage1KeyPressed = false;
    bool mPortalInteractKeyPressed = false;
    bool mPrintPositionKeyPressed = false;
    bool mShopOpen = false;
    bool mShopToggleKeyPressed = false;
    bool mShopMousePressed = false;
    bool mShopScrollUpKeyPressed = false;
    bool mShopScrollDownKeyPressed = false;
    ShopCategory mSelectedShopCategory = ShopCategory::Armor;
    int mShopFirstVisibleIndex = 0;
    std::vector<ShopItem> mShopItems;
    std::vector<size_t> mFilteredShopItemIndices;
    std::wstring mShopStatusMessage;
    DirectX::XMFLOAT4 mShopStatusColor = { 0.92f, 0.92f, 0.92f, 1.0f };
    float mShopStatusRemaining = 0.0f;
    std::unique_ptr<DirectX::GraphicsMemory> mShopGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mShopFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mShopTextureBatch;
    std::unique_ptr<DirectX::SpriteBatch> mShopTextBatch;
    std::unique_ptr<DirectX::SpriteFont> mShopFont;
};
