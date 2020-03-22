#ifndef __VIRTUAL_TEXTURE_SAMPLER_INCLUDE__
#define __VIRTUAL_TEXTURE_SAMPLER_INCLUDE__

#define UINT_TO_UNORM 1.5259021896696422e-05

VTResult GetResult(float2 pixel, uint index, float mip)
{
    VTResult r;
    VTIndices indcs = _IndirectBuffer[index];
    [unroll]
    for(uint i = 0; i < VT_COUNT; ++i)
    {
        r.results[i] = (indcs.indices[i] >= 0) ? _VirtualTex[indcs.indices[i]].Load(uint3(pixel, mip)) : float4(1,0,0,0);
    }
    return r;
}

VTResult GetLocalResult(float2 localUV, uint index, float mip)
{
    VTResult r;
    VTIndices indcs = _IndirectBuffer[index];
    [unroll]
    for(uint i = 0; i < VT_COUNT; ++i)
    {
        r.results[i] = (indcs.indices[i] >= 0) ? _VirtualTex[indcs.indices[i]].SampleLevel(bilinearClampSampler, localUV, mip) : float4(1,0,0,0);
    }
    return r;
}

VTResult GetIndResult( VTIndices indcs, float2 pixel, float mip)
{
    VTResult r;
    [unroll]
    for(uint i = 0; i < VT_COUNT; ++i)
    {
        r.results[i] = (indcs.indices[i] >= 0) ? _VirtualTex[indcs.indices[i]].Load(uint3(pixel, mip)) : float4(1,0,0,0);
    }
    return r;
}

VTResult GetSearchResult(float2 localUV, int2 chunk, float mip, uint2 indirectTexelSize, float2 lastOffset, float lastScale)
{
    chunk = max(0, chunk);
    chunk %= indirectTexelSize;
    uint4 indirectInt = _IndirectTex[chunk];
    float3 indirect = indirectInt.xyz * UINT_TO_UNORM;
    localUV = (localUV - lastOffset) / lastScale;
    float2 pixel = localUV * indirect.z + indirect.xy;
    uint index = indirectInt.w;
    VTResult r;
    VTIndices indcs = _IndirectBuffer[index];
    [unroll]
    for(uint i = 0; i < VT_COUNT; ++i)
    {
        r.results[i] = (indcs.indices[i] >= 0) ? _VirtualTex[indcs.indices[i]].SampleLevel(pointClampSampler, pixel, mip) : float4(1,0,0,0);
    }
    return r;
}

VTResult LerpVT(VTResult a, VTResult b, float w)
{
    VTResult r;
    [unroll]
    for(uint i = 0; i < VT_COUNT; ++i)
    {
        r.results[i] = lerp(a.results[i], b.results[i], w);
    }
    return r;
}

VTResult SampleBilinearWithArgs(
    int2 chunk, float2 realNormalizedUV, uint indirectIndex, float2 originSize, float mip, uint2 indirectTexelSize, float2 realAbsoluteUV, float2 fracedAbsoluteUV, float2 nextUV,  bool2 nextTest, bool2 nextTest1, float2 lastOffset, float lastScale )
{
    VTResult xOriginYNext, xNextYOrigin, xNextYNext;
    uint2 nextChunk = nextTest ? chunk + 1 : chunk;
    nextUV = nextTest ? (nextUV - originSize) : nextUV;
    nextChunk = nextTest1 ? nextChunk - 1 : nextChunk;
    nextUV = nextTest1 ? (nextUV + originSize) : nextUV;
        float2 nextLocalUV = nextUV / originSize;
    float2 lerpWeight = abs(fracedAbsoluteUV - 0.5);
    VTResult originPart = GetResult(realAbsoluteUV, indirectIndex, mip);
    xOriginYNext = GetSearchResult(float2(realNormalizedUV.x, nextLocalUV.y), uint2(chunk.x, nextChunk.y), mip, indirectTexelSize, lastOffset, lastScale);
    xNextYOrigin = GetSearchResult(float2(nextLocalUV.x, realNormalizedUV.y), uint2(nextChunk.x, chunk.y), mip, indirectTexelSize, lastOffset, lastScale);
    xNextYNext =  GetSearchResult(nextLocalUV, nextChunk, mip, indirectTexelSize, lastOffset, lastScale);
    
    VTResult xOrigin = LerpVT(originPart, xOriginYNext, lerpWeight.y);
    VTResult xNext = LerpVT(xNextYOrigin, xNextYNext, lerpWeight.y);
    return LerpVT(xOrigin, xNext, lerpWeight.x);
}

void GetSamplerData(
    float2 realNormalizedUV,
    float2 originSize,
    float mip,
    out  float2 realAbsoluteUV,
    out float2 fracedAbsoluteUV,
    out float2 nextUV,
    out bool2 nextTest,
    out bool2 nextTest1)  {
    originSize = originSize / pow(2, mip);
    realAbsoluteUV = realNormalizedUV * originSize;
    fracedAbsoluteUV = frac(realAbsoluteUV);
    bool2 valueCrossHalf = fracedAbsoluteUV > 0.5;
    nextUV = realAbsoluteUV + (valueCrossHalf ? 1 : -1);
    nextTest = nextUV > (originSize - 0.5);
    nextTest1 = nextUV < 0;
    nextTest = nextTest || nextTest1;

}

VTResult SampleBilinear(int2 chunk, float2 realNormalizedUV, uint indirectIndex, float2 originSize, float mip, uint2 indirectTexelSize, float2 lastOffset, float lastScale)
{
    originSize = originSize / pow(2, mip);
    float2 realAbsoluteUV = realNormalizedUV * originSize;
    float2 fracedAbsoluteUV = frac(realAbsoluteUV);
    bool2 valueCrossHalf = fracedAbsoluteUV > 0.5;
    float2 nextUV = realAbsoluteUV + (valueCrossHalf ? 1 : -1);
    bool2 nextTest = nextUV > (originSize - 0.5);//Larger Than Border
    bool2 nextTest1 = nextUV < 0;//Less Than Border
    nextTest = nextTest || nextTest1;
    //Sample Others
    
    
    if(!nextTest.x && !nextTest.y)
    {
        return GetLocalResult(realNormalizedUV, indirectIndex, mip);
    }
    else
    {
    VTResult xOriginYNext, xNextYOrigin, xNextYNext;
    uint2 nextChunk = nextTest ? chunk + 1 : chunk;
    nextUV = nextTest ? (nextUV - originSize) : nextUV;
    nextChunk = nextTest1 ? nextChunk - 1 : nextChunk;
    nextUV = nextTest1 ? (nextUV + originSize) : nextUV;
        float2 nextLocalUV = nextUV / originSize;
    float2 lerpWeight = abs(fracedAbsoluteUV - 0.5);
    VTResult originPart = GetResult(realAbsoluteUV, indirectIndex, mip);
    xOriginYNext = GetSearchResult(float2(realNormalizedUV.x, nextLocalUV.y), uint2(chunk.x, nextChunk.y), mip, indirectTexelSize, lastOffset, lastScale);
    xNextYOrigin = GetSearchResult(float2(nextLocalUV.x, realNormalizedUV.y), uint2(nextChunk.x, chunk.y), mip, indirectTexelSize, lastOffset, lastScale);
    xNextYNext =  GetSearchResult(nextLocalUV, nextChunk, mip, indirectTexelSize, lastOffset, lastScale);
    VTResult xOrigin = LerpVT(originPart, xOriginYNext, lerpWeight.y);
    VTResult xNext = LerpVT(xNextYOrigin, xNextYNext, lerpWeight.y);
    return LerpVT(xOrigin, xNext, lerpWeight.x);
    }
}

float2 GetVirtualTextureDebugUV(int2 chunk, float2 localUV, uint2 indirectTexelSize)
{
    chunk = max(chunk, 0);
    chunk %= indirectTexelSize;
    uint4 indirectInt = _IndirectTex[chunk];
    float3 indirect = indirectInt.xyz * UINT_TO_UNORM;
    float2 realNormalizedUV = localUV * indirect.z + indirect.xy;
    return realNormalizedUV;
}

VTResult SampleTrilinear(int2 chunk, float2 localUV, float2 originSize, float maxMip, uint2 indirectTexelSize)
{
    chunk = max(chunk, 0);
    chunk %= indirectTexelSize;
    uint4 indirectInt = _IndirectTex[chunk];
    float3 indirect = indirectInt.xyz * UINT_TO_UNORM;
    float2 realNormalizedUV = localUV * indirect.z + indirect.xy;
    float2 realAbsoluteUV = realNormalizedUV * originSize;
    float2 chunkFloat = (float2)chunk;
    float4 dd = abs(float4(ddx(realAbsoluteUV), ddy(realAbsoluteUV)) + float4(ddx(chunkFloat) * originSize, ddy(chunkFloat) * originSize));
    dd.xy = max(dd.xy, dd.zw);
    dd.x = max(dd.x, dd.y);
    float mip = max(0.5 * log2(dd.x), 0);
    mip = min(mip, maxMip);
    float downMip = floor(mip);
    float upMip = ceil(mip);
    float2 fracedAbsoluteUV, nextUV;
    bool2 nextTest, nextTest1;
    GetSamplerData(realNormalizedUV, originSize, upMip, realAbsoluteUV, fracedAbsoluteUV, nextUV, nextTest, nextTest1);
    
    if(!nextTest.x && !nextTest.y)
    {
        VTResult r;
        VTIndices indcs = _IndirectBuffer[indirectInt.w];
        [unroll]
        for(uint i = 0; i < VT_COUNT; ++i)
        {
            r.results[i] = (indcs.indices[i] >= 0) ? _VirtualTex[indcs.indices[i]].Sample(anisotropicClampSampler, realNormalizedUV) : float4(1,0,0,0);
        }
        return r;
    }
    else
    {
        VTResult upResult = SampleBilinearWithArgs(chunk, realNormalizedUV, indirectInt.w, originSize, upMip, indirectTexelSize, realAbsoluteUV, fracedAbsoluteUV, nextUV, nextTest, nextTest1, indirect.xy, indirect.z);
        VTResult downResult = SampleBilinear(chunk, realNormalizedUV, indirectInt.w, originSize, downMip, indirectTexelSize, indirect.xy, indirect.z);
        return downResult;
        return LerpVT(downResult, upResult, frac(mip));
    }
}

VTResult DefaultTrilinearSampler(int2 chunk, float2 localUV)
{
    uint4 indirectInt = _IndirectTex[chunk];
    float3 indirect = indirectInt.xyz * UINT_TO_UNORM;
    float2 realNormalizedUV = localUV * indirect.z + indirect.xy;
    VTResult r;
    VTIndices indcs = _IndirectBuffer[indirectInt.w];
    [unroll]
    for(uint i = 0; i < VT_COUNT; ++i)
    {
        r.results[i] = (indcs.indices[i] >= 0) ? _VirtualTex[indcs.indices[i]].Sample(anisotropicClampSampler, realNormalizedUV) : float4(1,0,0,0);
    }
    return r;
}

#endif