cbuffer Camera : register(b0)
{
    row_major float4x4 mvp;
};
struct VSIn {
    float3 pos : POSITION;
};
struct VSOut {
    float4 pos : SV_Position;
};
VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), mvp);
    return o;
}
float4 PSMain(VSOut i) : SV_Target {
    return float4(1,1,1,1);
}
