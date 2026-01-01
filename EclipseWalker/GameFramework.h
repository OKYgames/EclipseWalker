#pragma once
#include "d3dUtil.h"
#include "GameTimer.h" 

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class GameFramework
{
protected:
    GameFramework(HINSTANCE hInstance);
    virtual ~GameFramework();
    D3D12_CPU_DESCRIPTOR_HANDLE mDepthStencilBufferHandle()
    {
        return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    }
public:
    static GameFramework* GetApp();

    int Run();
    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    
    virtual void OnResize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;
    virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
    virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

    bool InitMainWindow();
    bool InitDirect3D();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateRtvAndDsvDescriptorHeaps();

    void CalculateFrameStats();

protected:
    static GameFramework* mApp;

    HINSTANCE mhAppInst = nullptr;
    HWND      mhMainWnd = nullptr;
    bool      mAppPaused = false;
    bool      mMinimized = false;
    bool      mMaximized = false;
    bool      mResizing = false;

    bool      m4xMsaaState = false;
    UINT      m4xMsaaQuality = 0;

    GameTimer mTimer; 

    ComPtr<IDXGIFactory4> mdxgiFactory;
    ComPtr<IDXGISwapChain> mSwapChain;
    ComPtr<ID3D12Device> md3dDevice;

    ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence = 0;

    ComPtr<ID3D12CommandQueue> mCommandQueue;
    ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;

    static const int SwapChainBufferCount = 2;
    int mCurrBackBuffer = 0;
    ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> mDepthStencilBuffer;

    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    D3D12_VIEWPORT mScreenViewport;
    D3D12_RECT mScissorRect;

    UINT mRtvDescriptorSize = 0;
    UINT mDsvDescriptorSize = 0;
    UINT mCbvSrvUavDescriptorSize = 0;

    int mClientWidth = 1280;
    int mClientHeight = 720;
};
