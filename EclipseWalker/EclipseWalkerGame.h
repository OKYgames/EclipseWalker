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
#include <DirectXColors.h>
#include <algorithm>
#include <vector>
#include <memory>
#include <unordered_map>

using namespace std;

class EclipseWalkerGame : public GameFramework
{
public:
    EclipseWalkerGame(HINSTANCE hInstance);
    ~EclipseWalkerGame();

    virtual bool Initialize() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

protected:
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    // --- [초기화 헬퍼 함수들] ---
    void LoadTextures();       // 매니저에게 로딩 명령
    void BuildMaterials();     // 매니저에게 재질 생성 명령
    void BuildShapeGeometry(); // 매니저에게 메쉬 저장 명령

    void BuildRenderItems();    // GameObject와 RenderItem 연결
    void BuildFrameResources(); // 프레임 버퍼 생성
    void InitLights();          // 조명 설정


    // --- [게임 로직 헬퍼 함수들] ---
    void OnKeyboardInput(const GameTimer& gt);
    void UpdateCamera();
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateShadowPassCB(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    void CreateSRV(Texture* tex, D3D12_CPU_DESCRIPTOR_HANDLE hDescriptor);
    float AspectRatio() const;

    // --- [입력 처리 오버라이드] ---
    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    // --- [엔진 시스템] ---
    std::unique_ptr<ResourceManager> mResources; // 자원 관리자
    std::unique_ptr<Renderer>        mRenderer;  // 렌더링 담당자 

    // --- [게임 데이터] ---
    vector<unique_ptr<RenderItem>> mAllRitems;         // 그리기 정보 리스트
    vector<std::unique_ptr<GameObject>> mGameObjects;  // 게임 객체 리스트 (관리용)

    // 플레이어
    RenderItem* mPlayerItem = nullptr;
    GameObject* mPlayerObject = nullptr;
    

    // 프레임 리소스
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    // 텍스처 서술자 힙
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    // 맵 데이터
    std::vector<Subset> mMapSubsets;

    // --[카메라 및 게임 상태] -- 
    Camera mCamera;
    POINT mLastMousePos;

    // 플레이어(Target) 위치
    DirectX::XMFLOAT3 mTargetPos = { 0.0f, 0.0f, 0.0f };

    // 카메라 회전 변수 (구면 좌표계)
    float mCameraTheta = 1.5f * DirectX::XM_PI; // 수평
    float mCameraPhi = 0.2f * DirectX::XM_PI;   // 수직
    float mCameraRadius = 5.0f;                 // 거리

    std::vector<GameLight> mGameLights;

   
};