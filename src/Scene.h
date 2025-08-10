#pragma once
#include <vector>
#include <string>
#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>

struct Transform {
    DirectX::XMFLOAT3 position{0,0,0};
    DirectX::XMFLOAT3 rotationEuler{0,0,0};
    DirectX::XMFLOAT3 scale{1,1,1};
};

struct MeshObject {
    std::wstring name;
    Transform transform;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    UINT vertexCount{0};
};

struct LightObject {
    std::wstring name;
    Transform transform;
    DirectX::XMFLOAT3 color{1,1,1};
    float intensity{1.0f};
};

struct Scene {
    std::vector<MeshObject> meshes;
    std::vector<LightObject> lights;
    int selectedMesh{-1};
    int selectedLight{-1};
};
