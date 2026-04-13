#include "WorldStateController.h"

#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "Player.h"
#include "RenderItem.h"
#include <Windows.h>
#include <algorithm>

WorldStateController::WorldStateController(EclipseWalkerGame* game)
    : mGame(game)
{
}

void WorldStateController::Initialize(GameObject* domainBoundaryObj, std::vector<RenderItem*>* realWorldRitems, std::vector<RenderItem*>* otherWorldRitems)
{
    Reset();
    mDomainBoundaryObj = domainBoundaryObj;
    mRealWorldRitems = realWorldRitems;
    mOtherWorldRitems = otherWorldRitems;
}

void WorldStateController::Reset()
{
    mDomainBoundaryObj = nullptr;
    mRealWorldRitems = nullptr;
    mOtherWorldRitems = nullptr;
    mIsOtherWorld = false;
    mFKeyPressed = false;
    mIsDomainActive = false;
    mDomainRadius = 0.0f;
    mTransitionEffect = WorldTransitionEffect();
}

void WorldStateController::Update(const GameTimer& gt, Player* player, bool blockInput)
{
    if (!blockInput && (GetAsyncKeyState('F') & 0x8000))
    {
        if (!mFKeyPressed && !mTransitionEffect.IsActive())
        {
            mFKeyPressed = true;
            mIsDomainActive = true;
            mDomainRadius = 0.0f;

            if (mDomainBoundaryObj && mDomainBoundaryObj->Ritem)
            {
                mDomainBoundaryObj->Ritem->Visible = true;
            }

            mTransitionEffect.StartTransition();
        }
    }
    else
    {
        mFKeyPressed = false;
    }

    if (mTransitionEffect.IsActive())
    {
        auto* camera = mGame->GetCamera();
        DirectX::XMFLOAT3 camPos = camera->GetPosition3f();
        mTransitionEffect.Update(gt, camPos);
        camera->SetPosition(camPos.x, camPos.y, camPos.z);
        camera->UpdateViewMatrix();

        if (mTransitionEffect.NeedsWorldSwitch())
        {
            mIsOtherWorld = !mIsOtherWorld;

            if (mRealWorldRitems)
            {
                for (auto* ri : *mRealWorldRitems)
                {
                    ri->Visible = !mIsOtherWorld;
                }
            }

            if (mOtherWorldRitems)
            {
                for (auto* ri : *mOtherWorldRitems)
                {
                    ri->Visible = mIsOtherWorld;
                }
            }

            mTransitionEffect.ResetWorldSwitch();
        }
    }

    if (mIsDomainActive && player && mDomainBoundaryObj && mDomainBoundaryObj->Ritem)
    {
        const float shellScale = mTransitionEffect.GetShellScale();
        const float flashAmount = mTransitionEffect.GetFlashAmount();
        const float targetRadius = 2.0f + (shellScale * 16.0f);

        if (flashAmount > 0.75f)
        {
            mDomainRadius = 80.0f;
        }
        else
        {
            mDomainRadius += (targetRadius - mDomainRadius) * std::min(1.0f, gt.DeltaTime() * 12.0f);
        }

        if (!mTransitionEffect.IsActive())
        {
            mIsDomainActive = false;
            mDomainBoundaryObj->Ritem->Visible = false;
            mDomainRadius = 0.0f;
        }

        if (mDomainBoundaryObj->Ritem->Visible)
        {
            const DirectX::XMFLOAT3 pos = player->GetPosition();
            mDomainBoundaryObj->SetPosition(pos.x, pos.y, pos.z);
            mDomainBoundaryObj->SetScale(mDomainRadius, mDomainRadius, mDomainRadius);
            mDomainBoundaryObj->Update();
        }
    }
}
