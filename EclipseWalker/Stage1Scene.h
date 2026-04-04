#pragma once
#include "Scene.h"
#include "MapSystem.h"
#include "ModelLoader.h"
#include <vector>
#include <memory>
#include "Monster.h"


class Stage1Scene : public Scene
{
public:
    Stage1Scene(EclipseWalkerGame* game);
    virtual ~Stage1Scene();

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

    MapSystem* GetActiveMapSystem() { return mIsOtherWorld ? mOtherMapSystem.get() : mRealMapSystem.get(); }
    float GetDomainRadius() const { return mDomainRadius; }
    bool GetIsDomainActive() const { return mIsDomainActive; }

private:
    void BuildMonsters();
    std::vector<std::unique_ptr<Monster>> mMonsters;
    std::vector<Monster*> mMonsterPtrs;

    bool mIsTransitioningToStage2 = false; 
    float mTransitionTimer = 0.0f;         

    GameObject* mDomainBoundaryObj = nullptr; 
    float mDomainRadius = 0.0f;              
    bool  mIsDomainActive = false;

public:
    int mSkyTexHeapIndex = 0;
    std::vector<Subset> mMapSubsets;

    bool mIsOtherWorld = false; 
    bool mFKeyPressed = false;  

 
    std::unique_ptr<MapSystem> mRealMapSystem;
    std::unique_ptr<MapSystem> mOtherMapSystem;

    std::vector<RenderItem*> mRealWorldRitems;
    std::vector<RenderItem*> mOtherWorldRitems;
};