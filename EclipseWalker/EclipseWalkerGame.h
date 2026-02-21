#pragma once
#include "GameFramework.h"
#include "Vertices.h"        
#include "Camera.h"
#include "RenderItem.h"
#include "Material.h"
#include "FrameResource.h"
#include "Texture.h"
#include "ModelLoader.h"
#include "GameObject.h"
#include "ResourceManager.h" 
#include "Renderer.h"        
#include "Player.h"

#include "d3dUtil.h"

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

    // 씬에서 오브젝트를 등록할 수 있도록 리스트 접근 허용
    vector<unique_ptr<RenderItem>>& GetRitems() { return mAllRitems; }
    vector<unique_ptr<GameObject>>& GetGameObjects() { return mGameObjects; }

    // =========================================================
    // 3단계 리소스 관리 함수
    // =========================================================
    void LoadCoreResources();         // 1단계: 코어 리소스 (폰트, UI 등)
    void LoadSharedGameResources();   // 2단계: 인게임 공통 리소스 (플레이어, 불꽃)
    void UnloadSharedGameResources(); // 인게임 공통 리소스 해제 (메인화면으로 갈 때)

protected:
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    // --- [초기화 헬퍼 함수들] ---
    // void LoadTextures();      // [삭제/이동] 씬(Scene) 내부로 이동!
    // void BuildMaterials();    // [삭제/이동] 씬(Scene) 내부로 이동!
    // void BuildShapeGeometry();// [삭제/이동] 씬(Scene) 내부로 이동!

    void BuildDescriptorHeaps(); // 힙 생성은 씬 진입 시마다 호출될 수 있으니 유지
    void BuildFrameResources();
    void InitLights();

    // --- [인게임 공통 리소스 생성 헬퍼] ---
    void BuildPlayer();
    void CreateFire(float x, float y, float z, float scale = 1.0f);

    // --- [게임 로직 헬퍼 함수들] ---
    void OnKeyboardInput(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateShadowPassCB(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    float AspectRatio() const;

    // --- [입력 처리 오버라이드] ---
    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    // --- [엔진 시스템 & 씬 관리자] ---
    std::unique_ptr<ResourceManager> mResources;
    std::unique_ptr<Renderer>        mRenderer;
    std::unique_ptr<Scene>           mCurrentScene; // 현재 돌아가고 있는 씬

    bool mIsSharedResourcesLoaded = false; // 공통 리소스 중복 로드 방지 플래그

    // --- [글로벌 게임 데이터 (모든 씬 공유)] ---
    vector<unique_ptr<RenderItem>> mAllRitems;
    vector<std::unique_ptr<GameObject>> mGameObjects;

    // 인게임 공통 객체
    GameObject* mPlayerObject = nullptr;
    std::unique_ptr<Player> mPlayer;
    std::vector<GameLight> mGameLights;
    int mCurrentLightIndex = 3;

    // 프레임 리소스
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    Camera mCamera;
    POINT mLastMousePos;

    // =========================================================
    // 아래 변수들은 나중에 Stage1Scene으로 옮겨야함
    // =========================================================
    int mSkyTexHeapIndex = 0;
    // std::vector<Subset> mMapSubsets;       // Stage1Scene으로 이동
    // std::unique_ptr<MapSystem> mMapSystem; // Stage1Scene으로 이동
};