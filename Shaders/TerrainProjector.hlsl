#include "Include/Sampler.cginc"

Texture2D<float4> _MainTex[2] : register(t0, space0);
Texture2D<uint4> _IndexMap[2] : register(t0, space1);
StructuredBuffer<uint2> _DescriptorIndexBuffer : register(t0, space2);
Texture2D<float2> _RGTex[2] : register(t0, space3);
cbuffer Params : register(b0)
{
    float2 _IndexMapSize;
    float2 _SplatScale;
    float2 _SplatOffset;
    float _VTSize;
    uint _SplatMapIndex;
    uint _IndexMapIndex;
    uint _DescriptorCount;
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

void SampleVT( float2 uv,  uint4 indirectIndex, float4 splatValue, out float4 albedo, out float4 normal)
{
    albedo = 0;
    normal = 0;
    float mipLevel = max(0, log2(_VTSize));
    uint2 index = _DescriptorIndexBuffer[indirectIndex.x];
    if(index.x < _DescriptorCount && index.y < _DescriptorCount)
    {
        albedo += splatValue.x * _MainTex[index.x].SampleLevel(trilinearWrapSampler, uv, mipLevel);
        normal += splatValue.x * _MainTex[index.y].SampleLevel(trilinearWrapSampler, uv, mipLevel);
    }
    index = _DescriptorIndexBuffer[indirectIndex.y];
    if(index.x < _DescriptorCount && index.y < _DescriptorCount)
    {
        albedo += splatValue.y * _MainTex[index.x].SampleLevel(trilinearWrapSampler, uv, mipLevel);
        normal += splatValue.y * _MainTex[index.y].SampleLevel(trilinearWrapSampler, uv, mipLevel);
    }
     index = _DescriptorIndexBuffer[indirectIndex.z];
    if(index.x < _DescriptorCount && index.y < _DescriptorCount)
    {
        albedo += splatValue.z * _MainTex[index.x].SampleLevel(trilinearWrapSampler, uv, mipLevel);
        normal += splatValue.z * _MainTex[index.y].SampleLevel(trilinearWrapSampler, uv, mipLevel);
    }
     index = _DescriptorIndexBuffer[indirectIndex.w];
    if(index.x < _DescriptorCount && index.y < _DescriptorCount)
    {
        albedo += splatValue.w * _MainTex[index.x].SampleLevel(trilinearWrapSampler, uv, mipLevel);
        normal += splatValue.w * _MainTex[index.y].SampleLevel(trilinearWrapSampler, uv, mipLevel);
    }
    
}

float2 GetUVOffset(uint2 chunkUV, float2 localUV, Texture2D<float2> randomTex, Texture2D<float4> scaleOffsetTexture, float2 scaleOffsetTexelSize, float sampleScale)
{
    float2 sampleUV = chunkUV * scaleOffsetTexelSize.xy + (localUV + randomTex.SampleLevel(bilinearWrapSampler, localUV, 0)) * scaleOffsetTexelSize.xy;
    sampleUV *= sampleScale;
    float4 scaleOffset = scaleOffsetTexture.SampleLevel(bilinearWrapSampler, sampleUV, 0);
    return localUV * scaleOffset.xy + scaleOffset.zw;
}

void frag(v2f i,
out float4 albedo : SV_TARGET0,
out float4 normalSpec : SV_TARGET1)
{
  
    normalSpec = 0.5;
   
    float2 splatUV = i.uv * _SplatScale + _SplatOffset;
    float2 vtUV = i.uv * _VTSize;
    float mipLevel = max(0, log2(_VTSize));
    albedo = _MainTex[_SplatMapIndex].SampleLevel(bilinearWrapSampler, vtUV, mipLevel);
    normalSpec = 0;
    /* uint2 indexMapUV = splatUV * _IndexMapSize;
    uint4 indirectIndex = _IndexMap[_IndexMapIndex].Load(uint3(indexMapUV, 0));
    float4 splatValue = _MainTex[_SplatMapIndex].SampleLevel(bilinearClampSampler, splatUV, 0);
    SampleVT(vtUV, indirectIndex, splatValue, albedo, normalSpec);*/
}
