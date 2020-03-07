#include "Include/D3D12.hlsl"
#include "Include/Sampler.cginc"
#include "Include/StdLib.cginc"
#include "Include/ACES.hlsl"
#include "Include/Colors.hlsl"
#include "Include/Sampling.hlsl"
#include "Include/Distortion.cginc"
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
Texture2D<float4> _MainTex : register(t0, space0);
Texture3D<float4> _Lut3D : register(t1, space0);
Texture2D<float> _ExposureTex : register(t2, space0);
Texture2D _BloomTex : register(t3, space0);
Texture2D _Bloom_DirtTex : register(t4, space0);
cbuffer Params : register(b0)
{
    //Bloom
    float4 _BloomTex_TexelSize;
    float4 _Bloom_DirtTileOffset;
	float4 _Bloom_Settings;
	float4 _Bloom_Color;
	uint _Use_BloomDirt;
	uint _Is_Bloom_FastMode;
	float2 __align;
    
    float4 _Lut3DParam;
    float4 _Distortion_Amount;
    float4 _Distortion_CenterScale;
    float4 _MainTex_TexelSize;
    float _ChromaticAberration_Amount;
};
#define MAX_CHROMATIC_SAMPLES 16
v2f vert(appdata v)
{
    v2f o;
    o.position = float4(v.vertex.xy, 1, 1);
    o.uv = v.uv;
    return o;
}


float4 frag(v2f i) : SV_TARGET
{
    float2 uv = i.uv;
    float exposure = _ExposureTex.SampleLevel(pointClampSampler, uv, 0);
    bool weight = dot(abs(uv - 0.5) , 0.5) <= dot(_MainTex_TexelSize.xy, 0.5);
    float2 distortedUV = DistortCheck(uv, weight, _Distortion_CenterScale, _Distortion_Amount);
   //Chromatic
   float2 coords = 2.0 * uv - 1.0;
			float2 end = uv - coords * dot(coords, coords) * _ChromaticAberration_Amount;

			float2 diff = end - uv;
			int samples = clamp(int(length(_MainTex_TexelSize.zw * diff / 2.0)), 3, MAX_CHROMATIC_SAMPLES);
			float2 delta = diff / samples;
			float2 pos = uv;
			float4 sum = (0.0).xxxx, filterSum = (0.0).xxxx;

			for (int i = 0; i < samples; i++)
			{
				float t = (i + 0.5) / samples;
				float4 s = _MainTex.SampleLevel(bilinearClampSampler, DistortCheck(pos,weight, _Distortion_CenterScale, _Distortion_Amount), 0);
				float3 filterDiff = abs(float3(
                    t,
                    t - 0.5,
                    t - 1
                ));
                float4 filter = float4(1 - saturate(filterDiff * 2), 1);
				sum += s * filter;
				filterSum += filter;
				pos += delta;
			}

	float4 color = sum / filterSum;
    //Color Grading
    color *= exposure;
    //Bloom
    {
        float4 bloom = 0;
        if(_Is_Bloom_FastMode == 0)
        {
            bloom = UpsampleTent(TEXTURE2D_PARAM(_BloomTex, bilinearClampSampler), distortedUV, _BloomTex_TexelSize.xy, _Bloom_Settings.x);
        }
        else
        {
            bloom = UpsampleBox(TEXTURE2D_PARAM(_BloomTex, bilinearClampSampler), distortedUV, _BloomTex_TexelSize.xy, _Bloom_Settings.x);
        }
        float4 dirt = 0;
        if(_Use_BloomDirt)
        {
            dirt = float4(SAMPLE_TEXTURE2D(_Bloom_DirtTex, bilinearClampSampler, distortedUV * _Bloom_DirtTileOffset.xy + _Bloom_DirtTileOffset.zw).rgb, 0.0);
        }
        bloom *= _Bloom_Settings.y;
	    dirt *= _Bloom_Settings.z;
	    color += bloom * float4(_Bloom_Color.xyz, 1.0);
	    color += dirt * bloom;
    }
        //Color Grading
    float3 colorLutSpace = saturate(LinearToLogC(color.rgb));
	color.rgb = ApplyLut3D(_Lut3D, bilinearClampSampler, colorLutSpace, _Lut3DParam.xy);
    color = KillNaN(color);

    return color;
}