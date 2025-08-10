#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>

struct ObjMeshData {
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<uint32_t> indices;
};

bool LoadObjPositionsAndIndices(const std::wstring& path, ObjMeshData& outMesh);
