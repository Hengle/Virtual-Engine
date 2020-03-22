#include "Include/Sampler.cginc"

Texture2D<float4> _SplatMap[100] : register(t0, space0);
Texture2D<float4> _AlbedoMap[100] : register(t0, space1);
Texture2D<float4> _NormalMap[100] : register(t0, space2);
StructuredBuffer<uint2> _SplatIndices : register(t0, space3);
cbuffer Params : register(b0)
{
    float2 _SplatScale;
    float2 _SplatOffset;
    uint _SplatMapIndex;
       
};

struct appdata
{
	float3 vertex    : POSITION;
    float2 uv : TEXCOORD;
};

struct v2f
{
	float4 position    : SV_POSITION;
    float2 uv : TEXCOORD;
};

v2f vert(appdata v)
{
    v2f o;
    o.position = float4(v.vertex.xy, 1, 1);
    o.uv = v.uv;
    return o;
}

void frag(v2f i,
out float4 albedo : SV_TARGET0,
out float4 normalSpec : SV_TARGET1)
{
    uint2 index = _SplatIndices[_SplatMapIndex];
    i.uv = i.uv * _SplatScale + _SplatOffset;
    albedo = _AlbedoMap[index.x].SampleLevel(bilinearClampSampler, i.uv, 0);
    normalSpec = _NormalMap[index.y].SampleLevel(bilinearClampSampler, i.uv, 0);
}