#include "Include/Sampler.cginc"
#define VT_COUNT 1
struct VTIndices
{
    int indices[VT_COUNT];
};
struct VTResult
{
    float4 results[VT_COUNT];
};
struct ObjectData
{
	float2 worldPosition;
	float2 worldScale;
	uint2 uvIndex;
};
Texture2D<float4> _VirtualTex[100] : register(t0, space0);
Texture2D<uint4> _IndirectTex : register(t0, space1);
StructuredBuffer<VTIndices> _IndirectBuffer : register(t1, space1);
StructuredBuffer<ObjectData> _ObjectDataBuffer : register(t2, space1);
#include "include/VirtualSampler.cginc"

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 uv    : TEXCOORD;
    float4 tangent : TANGENT;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float4 tangent : TANGENT;
	float2 uv      : TEXCOORD;
    float4 lastProjPos : TEXCOORD1;
    float4 currentProjPos : TEXCOORD2;
    uint2 vtIndex : TEXCOORD3;
};

cbuffer Per_Camera_Buffer : register(b0)
{
    float4x4 _WorldToCamera;
    float4x4 _InverseWorldToCamera;
    float4x4 _Proj;
    float4x4 _InvProj;
    float4x4 _VP;
    float4x4 _InvVP;
    float4x4 _NonJitterVP;
    float4x4 _NonJitterInverseVP;
    float4x4 _LastVP;
    float4x4 _InverseLastVP;
	float4 _RandomSeed;
    float3 worldSpaceCameraPos;
    float _NearZ;
    float _FarZ;
};

cbuffer VirtualTextureParams : register(b1)
{
    float2 _ChunkTexelSize;
    uint2 _IndirectTexelSize;
    uint _MaxMipLevel;
}

VertexOut VS(VertexIn vin, uint instanceID : SV_INSTANCEID)
{
	VertexOut vout;
	ObjectData objData = _ObjectDataBuffer[instanceID];
    // Transform to world space.
    vout.PosW = vin.PosL;
    vout.PosW.xz *= objData.worldScale;
    vout.PosW.xz += objData.worldPosition;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = vin.NormalL;

    // Transform to homogeneous clip space.
    vout.PosH = mul(_VP, float4(vout.PosW, 1));
    vout.currentProjPos = mul(_NonJitterVP, float4(vout.PosW, 1));
    vout.lastProjPos = mul(_LastVP, float4(vout.PosW, 1.0f));
    vout.uv.xy = vin.uv.xy;
    vout.vtIndex = objData.uvIndex;
    vout.tangent.xyz = vin.tangent.xyz;
    vout.tangent.w = vin.tangent.w;
    
    return vout;
}

void PS(VertexOut i, 
        out float4 albedo : SV_TARGET0,
        out float4 specular : SV_TARGET1,
        out float4 normalTex : SV_TARGET2,
        out float2 motionVectorTex : SV_TARGET3,
        out float4 emissionRT : SV_TARGET4)
{
    albedo = 0;
    specular = 0;
    normalTex = float4(0,0,1, 1);
    float2 lastScreenUV = (i.lastProjPos.xy / i.lastProjPos.w) * float2(0.5, 0.5) + 0.5;
   float2 screenUV = (i.currentProjPos.xy / i.currentProjPos.w) * float2(0.5, 0.5) + 0.5;
   motionVectorTex = screenUV - lastScreenUV;
   float2 uv = i.uv.xy;
 //  emissionRT = float4(0.1 * (i.vtIndex + uv), 0.5, 1);
    emissionRT = float4(GetVirtualTextureDebugUV(i.vtIndex, uv, _IndirectTexelSize), 0.5, 1);
   //emissionRT = SampleTrilinear(chunk, fracUV, _ChunkTexelSize, _MaxMipLevel, _IndirectTexelSize).results[0];
   //emissionRT = 0.5;
   albedo = emissionRT;
}

/*
float4 vert_depth(VertexIN vin, uint instanceID : SV_INSTANCEID) : SV_POSITION
{
	ObjectData objData = _ObjectDataBuffer[instanceID];
    // Transform to world space.
    float3 PosW = vin.PosL;
    PosW.xz *= objData.worldScale;
    PosW.xz += objData.worldPosition;
    return mul(_VP, float4(vout.PosW, 1));
}

void frag_depth(){}*/