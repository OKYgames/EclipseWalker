#pragma once
#include "Scene.h"
#include "ChatController.h"
#include "CombatSystem.h"
#include "DamageTextRenderer.h"
#include "LanternSystem.h"
#include "MapSystem.h"
#include "UIManager.h"
#include "WorldStateController.h"
#include <DescriptorHeap.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <vector>
#include <memory>

class Monster;

class Stage2Scene : public Scene
{
public:
    Stage2Scene(EclipseWalkerGame* game)
        : Scene(game)
        , mChatController(game)
        , mCombatSystem(game)
        , mDamageTextRenderer(game)
        , mWorldStateController(game, &mLanternSystem)
    {
    }

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void OnCharInput(WPARAM charCode) override;
    virtual void OnTextInput(const std::wstring& text) override;
    virtual void OnCompositionInput(const std::wstring& text, bool isFinal) override;

    MapSystem* GetActiveMapSystem() { return mMapSystem.get(); }
    float GetDomainRadius() const { return mWorldStateController.GetDomainRadius(); }
    bool GetIsDomainActive() const { return mWorldStateController.IsDomainActive(); }
    bool IsOtherWorld() const { return mWorldStateController.IsOtherWorld(); }

private: 
    std::unique_ptr<MapSystem> mMapSystem;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
    std::vector<Monster*> mMonsterPtrs;
    GameObject* mDomainBoundaryObj = nullptr;
    GameObject* mBossPatternRadiusObj = nullptr;
    std::vector<GameObject*> mBossPatternRadiusRingObjs;
    Monster* mBoss = nullptr;
    ChatController mChatController;
    CombatSystem mCombatSystem;
    DamageTextRenderer mDamageTextRenderer;
    LanternSystem mLanternSystem;
    WorldStateController mWorldStateController;
    bool mLanternUiClickPressed = false;
    bool mDebugPositionPrintKeyPressed = false;
    bool mDebugOutgoingDamageKeyPressed = false;
    bool mDebugIncomingDamageKeyPressed = false;
    bool mShowBossHealthText = false;
    bool mHasLastPlayerHpForDamageText = false;
    bool mBossPattern150Triggered = false;
    bool mBossPattern150DamagePending = false;
    int mBossHealthTextLayer = 0;
    float mLastPlayerHpForDamageText = 0.0f;
    float mBossPatternRadiusTimer = 0.0f;
    float mBossPattern150DamageTimer = 0.0f;
    DirectX::XMFLOAT3 mBossPattern150DamageCenter = { 0.0f, 0.0f, 0.0f };
    std::unique_ptr<DirectX::DescriptorHeap> mBossHealthTextHeap;
    std::unique_ptr<DirectX::SpriteBatch> mBossHealthTextBatch;
    std::unique_ptr<DirectX::SpriteFont> mBossHealthTextFont;

    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();
    void BuildBoss();
    void LogPlayerPosition(const DirectX::XMFLOAT3& position);
    void UpdateIncomingDamageText(Player* player);
    void InitializeBossHealthText();
    void DrawBossHealthText();
    int CalculateBossHealthLayer(float currentHp, float maxHp) const;
    void UpdateBossPatternTriggers(Player* player, int currentBossLayer);
    void TriggerBossPattern150(Player* player);
    void BuildBossPatternIndicator();
    void ShowBossPatternRadiusIndicator(const DirectX::XMFLOAT3& center);
    void UpdateBossPatternIndicator(float dt);
    void UpdateBossPattern150Damage(Player* player, float dt);
    void ApplyBossPattern150Damage(Player* player);
};
