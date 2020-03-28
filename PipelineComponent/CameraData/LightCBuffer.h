#pragma once
#include "../../Common/d3dUtil.h"
struct ShadowmapDrawParam
{
	float4x4 _ShadowmapVP;
	float3 _LightPos;
};

struct LightCullCBuffer
{
	float4x4 _InvVP;
	float4 _CameraNearPos;
	float4 _CameraFarPos;
	float4 _ZBufferParams;
	float3 _CameraForward;
	uint _LightCount;
	float3 _SunColor;
	uint _SunEnabled;
	float3 _SunDir;
	uint _SunShadowEnabled;
	uint4 _ShadowmapIndices;
	float4 _CascadeDistance;
	float4x4 _ShadowMatrix[4];
	float4 _ShadowSoftValue;
	float4 _ShadowOffset;
	uint _ReflectionProbeCount;
};
