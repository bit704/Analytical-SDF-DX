Texture2D g_Tex : register(t0);
SamplerState g_SamLinear : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4 g_Time;
    float4 g_Resolution;
    int4 g_Frame;
    float4x4 g_Camera;
    int4 g_Switch;
    float4 g_Factor;
}

struct VertexIn
{
    float3 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float2 tex : TEXCOORD;
};


