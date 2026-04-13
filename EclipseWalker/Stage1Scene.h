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
#include "WorldTransitionEffect.h"
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <GraphicsMemory.h>
#include <DescriptorHeap.h>

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

    MapSystem* GetActiveMapSystem() { return mIsOtherWorld ? mOtherMapSystem.get() : mRealMapSystem.get(); }
    float GetDomainRadius() const { return mDomainRadius; }
    bool  GetIsDomainActive() const { return mIsDomainActive; }

private:
    void BuildMonsters();
    void InitializeChatUI();
    void PollChatMessages();
    void PollChatKeyboardInput();
    void PushChatLine(const std::wstring& line);
    void BeginChatInput();
    void EndChatInput(bool sendMessage);
    void CommitComposingText();
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
    float mDomainRadius = 0.0f;
    bool  mIsDomainActive = false;
    WorldTransitionEffect mTransitionEffect;

    bool mIsChatting = false;
    bool mEscKeyPressed = false;
    bool mEnterKeyPressed = false;
    std::wstring mChatInput;
    std::wstring mComposingText;
    std::wstring mLastCommittedComposition;
    std::array<bool, 256> mChatKeyPressed{};
    std::deque<std::wstring> mChatLines;
    std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
    std::vector<GameObject*> mOwnedObjects;
    std::vector<RenderItem*> mOwnedRenderItems;
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
