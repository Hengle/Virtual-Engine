#include "Include/Sampler.cginc"
#include "Include/D3D12.hlsl"
#include "Include/StdLib.cginc"
#include "Include/ACES.hlsl"
#include "Include/Colors.hlsl"
#include "Include/Sampling.hlsl"
Texture2D _MainTex : register(t0, space0);
Texture2D _BloomTex : register(t1, space0);
Texture2D _AutoExposureTex : register(t2, space0);

cbuffer Params : register(b0)
{
    float4 _MainTex_TexelSize;
    float4 _ColorIntensity;
    float4 _Threshold; // x: threshold value (linear), y: threshold - knee, z: knee * 2, w: 0.25 / knee
    float4 _Params;
    float _SampleScale;
};

struct appdata
{
	float3 vertex    : POSITION;
    float2 texcoord : TEXCOORD;
};

struct v2f
{
	float4 position    : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

v2f vert(appdata v)
{
    v2f o;
    o.position = float4(v.vertex.xy, 1, 1);
    o.texcoord = v.texcoord;
    return o;
}

 float4 Prefilter(float4 color, float2 uv)
        {
            float autoExposure = SAMPLE_TEXTURE2D(_AutoExposureTex, pointClampSampler, uv).r;
            color *= autoExposure;
            color = min(_Params.x, color); // clamp to max
            color = QuadraticThreshold(color, _Threshold.x, _Threshold.yzw);
            return color;
        }

        float4 FragPrefilter13(v2f i) : SV_Target
        {
            float4 color = DownsampleBox13Tap(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy);
            return Prefilter(SafeHDR(color), i.texcoord);
        }

        float4 FragPrefilter4(v2f i) : SV_Target
        {
            float4 color = DownsampleBox4Tap(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy);
            return Prefilter(SafeHDR(color), i.texcoord);
        }

        // ----------------------------------------------------------------------------------------
        // Downsample

        float4 FragDownsample13(v2f i) : SV_Target
        {
            float4 color = DownsampleBox13Tap(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy);
            return color;
        }

        float4 FragDownsample4(v2f i) : SV_Target
        {
            float4 color = DownsampleBox4Tap(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy);
            return color;
        }

float4 Combine(float4 bloom, float2 uv)
        {
            float4 color = SAMPLE_TEXTURE2D(_BloomTex, bilinearClampSampler, uv);
            return bloom + color;
        }

        float4 FragUpsampleTent(v2f i) : SV_Target
        {
            float4 bloom = UpsampleTent(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy, _SampleScale);
            return KillNaN(Combine(bloom, i.texcoord));
        }

        float4 FragUpsampleBox(v2f i) : SV_Target
        {
            float4 bloom = UpsampleBox(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy, _SampleScale);
            return KillNaN(Combine(bloom, i.texcoord));
        }

        // ----------------------------------------------------------------------------------------
        // Debug overlays

        float4 FragDebugOverlayThreshold(v2f i) : SV_Target
        {
            float4 color = SAMPLE_TEXTURE2D(_MainTex, bilinearClampSampler, i.texcoord);
            return float4(Prefilter(SafeHDR(color), i.texcoord).rgb, 1.0);
        }

        float4 FragDebugOverlayTent(v2f i) : SV_Target
        {
            float4 bloom = UpsampleTent(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy, _SampleScale);
            return float4(bloom.rgb * _ColorIntensity.w * _ColorIntensity.rgb, 1.0);
        }

        float4 FragDebugOverlayBox(v2f i) : SV_Target
        {
            float4 bloom = UpsampleBox(TEXTURE2D_PARAM(_MainTex, bilinearClampSampler), i.texcoord, _MainTex_TexelSize.xy, _SampleScale);
            return float4(bloom.rgb * _ColorIntensity.w * _ColorIntensity.rgb, 1.0);
        }
        /*
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragPrefilter13

            ENDHLSL
        }

        // 1: Prefilter 4 taps
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragPrefilter4

            ENDHLSL
        }

        // 2: Downsample 13 taps
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragDownsample13

            ENDHLSL
        }

        // 3: Downsample 4 taps
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragDownsample4

            ENDHLSL
        }

        // 4: Upsample tent filter
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragUpsampleTent

            ENDHLSL
        }

        // 5: Upsample box filter
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragUpsampleBox

            ENDHLSL
        }

        // 6: Debug overlay (threshold)
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragDebugOverlayThreshold

            ENDHLSL
        }

        // 7: Debug overlay (tent filter)
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragDebugOverlayTent

            ENDHLSL
        }

        // 8: Debug overlay (box filter)
        Pass
        {
            HLSLPROGRAM

                #pragma vertex VertDefault
                #pragma fragment FragDebugOverlayBox

            ENDHLSL
        }
        */