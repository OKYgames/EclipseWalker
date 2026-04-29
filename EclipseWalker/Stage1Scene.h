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
#include "PickupSystem.h"
#include "WorldStateController.h"

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
    void BuildAnimatedTestActor();
    void BuildMonsters();
    void TrackOwned(GameObject* object, RenderItem* renderItem);
    void ReleaseOwnedObjects();

    void UpdateMonstersFromServer();

    std::vector<std::unique_ptr<Monster>> mMonsters;
    std::vector<Monster*> mMonsterPtrs;
    std::unordered_map<int, DirectX::XMFLOAT3>  mMonsterTargetPos;

    std::unordered_map<int, Monster*> mMonsterById;

    bool  mIsTransitioningToStage2 = false;
    float mTransitionTimer = 0.0f;

    GameObject* mDomainBoundaryObj = nullptr;
    ChatController mChatController;
    CombatSystem mCombatSystem;
    PickupSystem mPickupSystem;
    WorldStateController mWorldStateController;
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
