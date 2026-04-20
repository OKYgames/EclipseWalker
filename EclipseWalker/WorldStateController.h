#pragma once

#include "WorldTransitionEffect.h"
#include <vector>

class EclipseWalkerGame;
class GameObject;
class Player;
struct RenderItem;

class WorldStateController
{
public:
    explicit WorldStateController(EclipseWalkerGame* game);

    void Initialize(GameObject* domainBoundaryObj, std::vector<RenderItem*>* realWorldRitems, std::vector<RenderItem*>* otherWorldRitems);
    void Reset();
    void Update(const GameTimer& gt, Player* player, bool blockInput);

    bool IsOtherWorld() const { return mIsOtherWorld; }
    bool IsDomainActive() const { return mIsDomainActive; }
    float GetDomainRadius() const { return mDomainRadius; }
    bool IsTransitionActive() const { return mTransitionEffect.IsActive(); }

private:
    EclipseWalkerGame* mGame = nullptr;
    GameObject* mDomainBoundaryObj = nullptr;
    std::vector<RenderItem*>* mRealWorldRitems = nullptr;
    std::vector<RenderItem*>* mOtherWorldRitems = nullptr;

    bool mIsOtherWorld = false;
    bool mFKeyPressed = false;
    bool mIsDomainActive = false;
    float mDomainRadius = 0.0f;
    WorldTransitionEffect mTransitionEffect;
};
