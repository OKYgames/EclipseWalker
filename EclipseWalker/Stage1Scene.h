#pragma once
#include "Scene.h"
#include "MapSystem.h"
#include "ModelLoader.h"
#include <vector>
#include <memory>
#include <deque>
#include <array>
#include <unordered_map> 
#include "Monster.h"
#include "ChatController.h"
#include "CombatSystem.h"
#include "LanternSystem.h"
#include "PickupSystem.h"
#include "WorldStateController.h"

class InteractiveDoor;

class Stage1Scene : public Scene
{
public:
    Stage1Scene(EclipseWalkerGame* game);
    virtual ~Stage1Scene();

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    virtual void DrawOverlay() override;
    virtual void OnCharInput(WPARAM charCode) override;
    virtual void OnTextInput(const std::wstring& text) override;
    virtual void OnCompositionInput(const std::wstring& text, bool isFinal) override;

    MapSystem* GetActiveMapSystem() { return mWorldStateController.IsOtherWorld() ? mOtherMapSystem.get() : mRealMapSystem.get(); }
    float GetDomainRadius() const { return mWorldStateController.GetDomainRadius(); }
    bool  GetIsDomainActive() const { return mWorldStateController.IsDomainActive(); }
    bool  IsOtherWorld() const { return mWorldStateController.IsOtherWorld(); }

private:
    struct MonsterHealthBar
    {
        Monster* Owner = nullptr;
        GameObject* Back = nullptr;
        GameObject* Fill = nullptr;
    };

    void BuildAnimatedTestActor();
    void BuildInteractiveDoors();
    void BuildMonsters();
    void CreateMonsterHealthBar(Monster* monster);
    void UpdateMonsterHealthBars();
    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();
    void LogPlayerPositionIfMoved(const DirectX::XMFLOAT3& position);
    void UpdateMonsterAnimationDebugInput(bool hasFocus);

    void UpdateMonstersFromServer();

    std::vector<std::unique_ptr<Monster>> mMonsters;
    std::vector<Monster*> mMonsterPtrs;
    std::vector<MonsterHealthBar> mMonsterHealthBars;
    std::unordered_map<int, DirectX::XMFLOAT3>  mMonsterTargetPos;

    std::unordered_map<int, Monster*> mMonsterById;

    bool  mIsTransitioningToStage2 = false;
    float mTransitionTimer = 0.0f;

    GameObject* mDomainBoundaryObj = nullptr;
    ChatController mChatController;
    CombatSystem mCombatSystem;
    LanternSystem mLanternSystem;
    PickupSystem mPickupSystem;
    WorldStateController mWorldStateController;
    std::vector<std::unique_ptr<InteractiveDoor>> mDoors;
    bool mDoorInteractKeyPressed = false;
    bool mLanternUiClickPressed = false;
    bool mDebugMonsterIdleKeyPressed = false;
    bool mDebugMonsterDamageKeyPressed = false;
    bool mDebugMonsterDeathKeyPressed = false;
    bool mHasLastDebugPlayerPosition = false;
    DirectX::XMFLOAT3 mLastDebugPlayerPosition = { 0.0f, 0.0f, 0.0f };
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
public:
    int mSkyTexHeapIndex = 0;
    std::vector<Subset> mMapSubsets;

    std::unique_ptr<MapSystem> mRealMapSystem;
    std::unique_ptr<MapSystem> mOtherMapSystem;

    std::vector<RenderItem*> mRealWorldRitems;
    std::vector<RenderItem*> mOtherWorldRitems;
};
