#pragma once
#include "GameFramework.h"
#include "Vertices.h"       
#include "Camera.h"
#include "RenderItem.h"
#include "Material.h"
#include "FrameResource.h"
#include "Texture.h"
#include "ModelLoader.h"
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
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry(); 
    void BuildMaterials();
    void BuildRenderItems();
    void BuildPSO();
	void BuildFrameResources();


    // --- [게임 로직 헬퍼 함수들] ---
    void OnKeyboardInput(const GameTimer& gt); // 키보드 이동
    void UpdateCamera();                       // 카메라 위치 계산
    void UpdateObjectCBs(const GameTimer& gt); // 행렬 계산 및 전송
    float AspectRatio() const;                 // 화면 비율 계산
    array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
    void LoadTextures();
    void InitLights();
    void UpdateMainPassCB(const GameTimer& gt);

    // --- [입력 처리 오버라이드] ---
    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    // ---  DirectX 코어 리소스 ---
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // --- 지오메트리 및 데이터 버퍼 ---
    unordered_map<string, unique_ptr<MeshGeometry>> mGeometries;
    unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

    // 화면에 그릴 모든 아이템 목록
    vector<unique_ptr<RenderItem>> mAllRitems;

    // 플레이어가 누군지 가리키는 포인터 
    RenderItem* mPlayerItem = nullptr;
    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr; 

    // 프레임 리소스 3개 
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;


    // 2. 텍스처 저장소
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

    // 3. 텍스처 서술자 힙
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    // 맵의 "덩어리 정보"를 저장해둘 변수
    std::vector<Subset> mMapSubsets;

    // --- 3. 카메라 및 게임 플레이 변수 ---
    Camera mCamera;
    POINT mLastMousePos;
    // 캐릭터(Target) 위치
    DirectX::XMFLOAT3 mTargetPos = { 0.0f, 0.0f, 0.0f };

    // 카메라 회전 변수 (구면 좌표계)
    float mCameraTheta = 1.5f * DirectX::XM_PI; // 수평
    float mCameraPhi = 0.2f * DirectX::XM_PI;   // 수직
    float mCameraRadius = 5.0f;                 // 거리

    std::vector<GameLight> mGameLights;

   
};