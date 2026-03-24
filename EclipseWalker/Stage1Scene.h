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

private:
    void BuildMonsters();
    std::vector<std::unique_ptr<Monster>> mMonsters;
    std::vector<Monster*> mMonsterPtrs;

public:
    int mSkyTexHeapIndex = 0;
    std::vector<Subset> mMapSubsets;

    // 랜턴 시스템 전용 변수들
    bool mIsOtherWorld = false; // 현재 이면세계인지 여부
    bool mFKeyPressed = false;  // 키보드 연타 방지용

    // 물리 충돌 시스템 2개
    std::unique_ptr<MapSystem> mRealMapSystem;
    std::unique_ptr<MapSystem> mOtherMapSystem;

    // 눈에 보이는 그래픽(렌더 아이템)을 껐다 켜기 위한 리스트
    std::vector<RenderItem*> mRealWorldRitems;
    std::vector<RenderItem*> mOtherWorldRitems;
};