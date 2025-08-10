#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>

struct RendererInitParams {
    HWND hwnd;
    UINT width;
    UINT height;
};

struct CameraCBData { DirectX::XMFLOAT4X4 mvp; };

class Renderer {
public:
    bool initialize(const RendererInitParams& params);
    void resize(UINT width, UINT height);
    void beginFrame();
    void drawMesh(const D3D12_VERTEX_BUFFER_VIEW& vbv, UINT vertexCount, const CameraCBData& camera);
    void endFrame(bool vsync, bool allowTearing);

    ID3D12Device* getDevice() const { return device.Get(); }
    ID3D12GraphicsCommandList* getCmdList() const { return commandList.Get(); }
    UINT getCurrentFrameIndex() const { return currentFrameIndex; }

private:
    static constexpr UINT kFrameCount = 2;

    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
    HANDLE frameLatencyWaitableObject{};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
    UINT rtvDescriptorSize{};
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> imguiSrvHeap;

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraCBs[kFrameCount];
    uint8_t* cameraCBMapped[kFrameCount]{};

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    UINT64 fenceValues[kFrameCount]{};
    HANDLE fenceEvent{};

    UINT currentFrameIndex{};
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissorRect{};

    bool tearingSupported{false};

    bool createDeviceAndSwapchain(HWND hwnd, UINT w, UINT h);
    void createRenderTargets();
    void createDepthResources(UINT w, UINT h);
    bool createPipeline();
};
