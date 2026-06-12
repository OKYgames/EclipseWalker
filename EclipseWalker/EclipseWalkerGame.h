#pragma once
#include "NetworkManager.h"
#include "GameFramework.h"      
#include "Vertices.h"
#include "GameObject.h"
#include "Camera.h"
#include "ModelLoader.h"

#include "RenderItem.h"
#include "Material.h"
#include "Texture.h"
#include "FrameResource.h"
#include "ResourceManager.h" 
#include "Renderer.h"        
#include "Player.h"
#include "Mage.h"
#include "Warrior.h"
#include "Archer.h"
#include "UIManager.h"
#include "d3dUtil.h"
#include "SocketAttachmentSystem.h"

#include <unordered_map>

class Scene;

using namespace std;

class EclipseWalkerGame : public GameFramework
{
public:
    EclipseWalkerGame(HINSTANCE hInstance);
    ~EclipseWalkerGame();

    virtual bool Initialize() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // =========================================================
    // 씬(Scene) 관리 인터페이스
    // =========================================================
    void ChangeScene(std::unique_ptr<Scene> newScene);

    // 각 씬(Scene)들이 엔진 시스템과 자원에 접근할 수 있도록 Getter 제공
    ResourceManager* GetResources() const { return mResources.get(); }
    Renderer* GetRenderer()  const { return mRenderer.get(); }
    Camera* GetCamera() { return &mCamera; }
    ID3D12Device* GetDevice()    const { return md3dDevice.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return mCommandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return mCommandQueue.Get(); }
    D3D12_VIEWPORT GetScreenViewport() const { return mScreenViewport; }
	Player* GetPlayer()  const { return mPlayer.get(); }
    UIManager* GetUIManager() const { return mUIManager.get(); }
    HWND GetMainWindowHandle() const { return mhMainWnd; }
    PlayerClass GetSelectedPlayerClass() const { return mSelectedPlayerClass; }
    void SetSelectedPlayerClass(PlayerClass playerClass);
    void RefreshPlayerForSelectedClass();

    // 씬에서 오브젝트를 등록할 수 있도록 리스트 접근 허용
    vector<unique_ptr<RenderItem>>& GetRitems() { return mAllRitems; }
    vector<unique_ptr<GameObject>>& GetGameObjects() { return mGameObjects; }

    void FlushCommandQueue()
    {
        mCurrentFence++;
        mCommandQueue->Signal(mFence.Get(), mCurrentFence);
        if (mFence->GetCompletedValue() < mCurrentFence)
        {
            HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            mFence->SetEventOnCompletion(mCurrentFence, eventHandle);
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    // =========================================================
    // 3단계 리소스 관리 함수
    // =========================================================
    void LoadCoreResources();         // 1단계: 코어 리소스 (폰트, UI 등)
    void LoadSharedGameResources();   // 2단계: 인게임 공통 리소스 (플레이어, 불꽃)
    void UnloadSharedGameResources(); // 인게임 공통 리소스 해제 
    void BuildDescriptorHeaps();
    void CreateFire(float x, float y, float z, float scale = 1.0f);
    void BuildPlayerEquipment(GameObject* parentObject, GameObject*& outWeaponObject, GameObject*& outShieldObject);
    void ClearSocketAttachments();
    void ApplyCharacterSelectLighting(const DirectX::XMFLOAT3& focusPosition);
    void SetMirrorBreakEffect(float progress);
    void ClearMirrorBreakEffect();
    void ResetLights() {
        mGameLights.clear();    
        InitLights();           
        mCurrentLightIndex = 1; 
    }

    void UpdateRemotePlayers(); // 매 프레임 남의 캐릭터 위치를 갱신할 함수 (서버싸개가 추가)

protected:
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    void BuildFrameResources();
    void InitLights();
    std::unique_ptr<Player> CreatePlayerForSelectedClass() const;

    // --- [인게임 공통 리소스 생성 헬퍼] ---
    void BuildPlayer();
    void BuildPlayerWeapon();
    void BuildMirrorBreakResources();
    void BuildMirrorBreakQuad();
    void HideRemotePlayer(int playerId);
    void UpdateWeaponSocketDebug(const GameTimer& gt);
    void ApplySwordSocketDebug();
    void LogSwordSocketDebug() const;


    // --- [게임 로직 헬퍼 함수들] ---
    void OnKeyboardInput(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateSkinnedCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateShadowPassCB(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateUIPassCB(const GameTimer& gt);
    float AspectRatio() const;
    bool ShouldDrawMirrorBreakEffect() const;
    D3D12_CPU_DESCRIPTOR_HANDLE MirrorBreakRenderTargetView() const;

    // --- [입력 처리 오버라이드] ---
    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    // --- [엔진 시스템 & 씬 관리자] ---
    std::unique_ptr<ResourceManager> mResources;
    std::unique_ptr<Renderer>        mRenderer;
    std::unique_ptr<Scene>           mCurrentScene; 

    bool mIsSharedResourcesLoaded = false; // 공통 리소스 중복 로드 방지 플래그

    // --- [글로벌 게임 데이터 (모든 씬 공유)] ---
    vector<unique_ptr<RenderItem>> mAllRitems;
    vector<std::unique_ptr<GameObject>> mGameObjects;

    // 인게임 공통 객체
    GameObject* mPlayerObject = nullptr;
    GameObject* mPlayerWeaponObject = nullptr;
    GameObject* mPlayerShieldObject = nullptr;
    DirectX::XMFLOAT3 mDebugSwordSocketPosition = { 0.3504f, 0.1006f, 0.0685f };
    DirectX::XMFLOAT3 mDebugSwordSocketRotation = { 3.0769f, 1.3175f, -1.0446f };
    float mWeaponSocketDebugLogTimer = 0.0f;
    bool mWeaponSocketDebugPrintWasDown = false;
    std::unique_ptr<Player> mPlayer;
    SocketAttachmentSystem mSocketAttachmentSystem;
    PlayerClass mSelectedPlayerClass = PlayerClass::Mage;
    std::vector<GameLight> mGameLights;
    std::unique_ptr<UIManager> mUIManager;
    RenderItem* mMirrorBreakRitem = nullptr;
    std::unique_ptr<GameObject> mMirrorBreakObject;
    Microsoft::WRL::ComPtr<ID3D12Resource> mMirrorBreakSceneColor;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mMirrorBreakRtvHeap;
    D3D12_RESOURCE_STATES mMirrorBreakSceneColorState = D3D12_RESOURCE_STATE_COMMON;
    bool mMirrorBreakEffectActive = false;
    float mMirrorBreakEffectProgress = 0.0f;
    int mCurrentLightIndex = 1;

    // 프레임 리소스
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    Camera mCamera;
    POINT mLastMousePos;

    std::unordered_map<int, GameObject*> mRemotePlayerObjects;
    std::unordered_map<int, GameObject*> mRemotePlayerWeaponObjects;
    std::unordered_map<int, GameObject*> mRemotePlayerShieldObjects;
    std::unordered_map<int, int> mRemotePlayerAnimationStates;
    std::unordered_map<int, unsigned long long> mRemotePlayerAttackEndTicks;
    int mPendingImeCharSkips = 0;

};
