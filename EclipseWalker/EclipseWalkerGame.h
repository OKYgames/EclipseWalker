#pragma once

#include "GameFramework.h"
#include "Vertices.h"       
#include "Camera.h"
#include "RenderItem.h"
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

    // 1. 메인 프레임워크 오버라이드 (초기화 및 메시지 처리)
    virtual bool Initialize() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

protected:
    // 2. 게임 루프 오버라이드 (창크기 변경, 업데이트, 그리기)
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    // --- [초기화 헬퍼 함수들] ---
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry(); 
    void BuildRenderItems();
    void BuildPSO();
    void BuildConstantBuffer();

    // --- [게임 로직 헬퍼 함수들] ---
    void OnKeyboardInput(const GameTimer& gt); // 키보드 이동
    void UpdateCamera();                       // 카메라 위치 계산
    void UpdateObjectCBs(const GameTimer& gt); // 행렬 계산 및 전송
    float AspectRatio() const;                 // 화면 비율 계산

    // --- [입력 처리 오버라이드] ---
    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
    // --- 1. DirectX 코어 리소스 ---
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // --- 2. 지오메트리 및 데이터 버퍼 ---
    unordered_map<string, unique_ptr<MeshGeometry>> mGeometries;

    // 화면에 그릴 모든 아이템 목록
    vector<unique_ptr<RenderItem>> mAllRitems;

    // "플레이어"가 누군지 가리키는 포인터 (나중에 조종하려고)
    RenderItem* mPlayerItem = nullptr;
    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr; 
    Microsoft::WRL::ComPtr<ID3D12Resource> mObjectCB = nullptr; // 상수 버퍼
    BYTE* mMappedData = nullptr; // 매핑된 메모리 주소

    // --- 3. 카메라 및 게임 플레이 변수 ---
    Camera mCamera;
    POINT mLastMousePos;
    // 캐릭터(Target) 위치
    DirectX::XMFLOAT3 mTargetPos = { 0.0f, 0.0f, 0.0f };

    // 카메라 회전 변수 (구면 좌표계)
    float mCameraTheta = 1.5f * DirectX::XM_PI; // 수평
    float mCameraPhi = 0.2f * DirectX::XM_PI;   // 수직
    float mCameraRadius = 5.0f;                 // 거리

    bool mIsWPressed = false;
    bool mIsAPressed = false;
    bool mIsSPressed = false;
    bool mIsDPressed = false;
};