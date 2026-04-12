#pragma once
#include "Scene.h"
#include "MapSystem.h"
#include "ModelLoader.h"
#include "Monster.h"
#include "WorldTransitionEffect.h"   // ← 추가
#include <vector>
#include <memory>
#include <unordered_map>

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
    bool  GetIsDomainActive() const { return mIsDomainActive; }

private:
    void BuildMonsters();
    void UpdateMonstersFromServer();
    void DoWorldSwap();              // ← 추가: 실제 스왑 처리

    // ── 몬스터 ───────────────────────────────────────
    std::vector<std::unique_ptr<Monster>> mMonsters;
    std::vector<Monster*>                 mMonsterPtrs;
    std::unordered_map<int, DirectX::XMFLOAT3> mMonsterTargetPos;
    std::unordered_map<int, Monster*>     mMonsterById;

    // ── 스테이지2 전환 (기존 유지) ───────────────────
    bool  mIsTransitioningToStage2 = false;
    float mTransitionTimer = 0.0f;

    // ── 도메인 (기존 유지) ───────────────────────────
    GameObject* mDomainBoundaryObj = nullptr;
    float mDomainRadius = 0.0f;
    bool  mIsDomainActive = false;

    // ── 이펙트 (추가) ────────────────────────────────
    WorldTransitionEffect mTransition;

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