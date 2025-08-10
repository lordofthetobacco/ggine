#include "ObjLoader.h"
#include <fstream>
#include <sstream>

static inline bool ParseUInt(const char* s, size_t& i, uint32_t& out) {
    uint32_t v = 0; size_t start = i; while (s[i] >= '0' && s[i] <= '9') { v = v * 10 + (uint32_t)(s[i]-'0'); ++i; }
    if (i == start) return false; out = v; return true;
}

bool LoadObjPositionsAndIndices(const std::wstring& path, ObjMeshData& outMesh) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<uint32_t> indices;
    positions.reserve(1024);
    indices.reserve(2048);
    while (std::getline(f, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') {
            std::istringstream ss(line.substr(2));
            float x,y,z; ss >> x >> y >> z; positions.emplace_back(x,y,z);
        } else if (line[0] == 'f' && line[1] == ' ') {
            const char* s = line.c_str() + 2;
            uint32_t v[4]{}; int count = 0;
            while (*s && count < 4) {
                while (*s == ' ') ++s;
                size_t i = 0; uint32_t a=0; uint32_t b=0; uint32_t c=0; uint32_t r=0;
                const char* start = s;
                while (s[i] && s[i] != ' ') ++i;
                std::string token(start, i);
                const char* t = token.c_str(); size_t k = 0;
                if (!ParseUInt(t, k, a)) { s += i; continue; }
                if (t[k] == '/') { ++k; ParseUInt(t, k, b); if (t[k] == '/') { ++k; ParseUInt(t, k, c); }}
                v[count++] = a; s += i;
            }
            if (count >= 3) {
                indices.push_back(v[0]-1); indices.push_back(v[1]-1); indices.push_back(v[2]-1);
                if (count == 4) { indices.push_back(v[0]-1); indices.push_back(v[2]-1); indices.push_back(v[3]-1); }
            }
        }
    }
    outMesh.positions = std::move(positions);
    outMesh.indices = std::move(indices);
    return true;
}
