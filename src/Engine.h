#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "Scene.h"

class Engine {
public:
    Engine(UINT width, UINT height);
    bool initialize(HWND windowHandle);
    void resize(UINT width, UINT height);
    void render();
    void update(double dt);
    void waitForGpu();
    ~Engine();

private:
    static constexpr UINT kFrameCount = 2;
    void moveToNextFrame();
    void createSwapChain();
    void createRenderTargets();
    void createDepthResources();
    bool createPipeline();
    bool initImGui();
    bool createCubeObject(const std::wstring& name);
    bool createObjObject(const std::wstring& path, const std::wstring& name);
    void createLightObject(const std::wstring& name);
    bool uploadVerticesToBuffer(const std::vector<DirectX::XMFLOAT3>& vertices, Microsoft::WRL::ComPtr<ID3D12Resource>& outVB, D3D12_VERTEX_BUFFER_VIEW& outVBV);
    void setFullscreen(bool enable);
    void setWindowClientSize(UINT width, UINT height);
    std::vector<uint8_t> readFileBytes(const std::wstring& path);
    UINT clientWidth; UINT clientHeight; HWND hwnd;
    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
    HANDLE frameLatencyWaitableObject{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
    UINT rtvDescriptorSize; Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[kFrameCount]; Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> imguiSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    struct alignas(256) CameraCB { DirectX::XMFLOAT4X4 mvp; };
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraCBs[kFrameCount];
    uint8_t* cameraCBMapped[kFrameCount]{};
    Microsoft::WRL::ComPtr<ID3D12Fence> fence; UINT64 fenceValues[kFrameCount]{}; HANDLE fenceEvent{};
    UINT currentFrameIndex; D3D12_VIEWPORT viewport; D3D12_RECT scissorRect;
    std::wstring cameraName{L"Camera"}; DirectX::XMFLOAT3 cameraPosition{0.0f,0.0f,-3.5f}; float cameraYaw{0.0f}; float cameraPitch{0.0f}; POINT lastMouse{};
    bool tearingSupported{false}; bool enableTearing{false}; bool enableVsync{true};
    std::vector<float> frameTimeMs; size_t frameTimeWriteIdx{0}; LARGE_INTEGER perfFreq{}; LARGE_INTEGER lastCounter{}; bool timingInitialized{false};
    Scene scene; enum class SelectionKind { None, Camera, Mesh, Light }; SelectionKind selectionKind{SelectionKind::Mesh}; int selectedIndex{0};
    bool isFullscreen{false}; DWORD windowStyle{0}; RECT windowRect{0,0,0,0};
};
