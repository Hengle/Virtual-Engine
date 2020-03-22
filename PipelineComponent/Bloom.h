#pragma once
#include "../Common/d3dUtil.h"
#include "../RenderComponent/ComputeShader.h"
#include "../Singleton/ShaderCompiler.h"
#include "../Singleton/ShaderID.h"
#include "../RenderComponent/UploadBuffer.h"
#include "../Common/MetaLib.h"
#include "../RenderComponent/RenderTexture.h"
#include "../Common/Camera.h"
#include "../RenderComponent/DescriptorHeap.h"
#include "../RenderComponent/StructuredBuffer.h"
#include "../Singleton/FrameResource.h"
#include "../Singleton/Graphics.h"
#include "../RenderComponent/TextureHeap.h"
#include "../RenderComponent/Texture.h"
#include "../Singleton/PSOContainer.h"
struct BloomCBuffers
{
	float4 _BloomTex_TexelSize;
	float4 _Bloom_DirtTileOffset;
	float4 _Bloom_Settings;
	float4 _Bloom_Color;
	uint _Use_BloomDirt;
	uint _Is_Bloom_FastMode;
	float2 __align;
};
class Bloom
{
public:
	const float intensitySetting = 1;
	const float threshold = 1;
	const float softKnee = 0.5f;
	const float clampValue = 65472;
	const float diffusion = 7;
	const float anamorphicRatio = 0;
	const int k_MaxPyramidSize = 16;
	const float3 color = { 1,1,1 };
	uint _BloomTex;
	uint _AutoExposureTex;
	uint Params;
	Texture* dirtTex = nullptr;
	const float  dirtIntensity = 0;
	Shader* bloomShader;
	StackObject<PSOContainer> bloomPSO;
	struct Level
	{
		StackObject<RenderTexture> down;
		StackObject<RenderTexture> up;
		Level(const Level& l)
		{
			down.New(*l.down);
			up.New(*l.up);
		}
		Level() {}
	};
	struct CBufferParams
	{
		float4 _MainTex_TexelSize;
		float4 _ColorIntensity;
		float4 _Threshold; // x: threshold value (linear), y: threshold - knee, z: knee * 2, w: 0.25 / knee
		float4 _Params;
		float _SampleScale;
	};
	class BloomFrameResource : public IPipelineResource
	{
	public:
		StackObject<DescriptorHeap> descHeap;
		StackObject<UploadBuffer> ubuffer;
		uint mIterations;
		BloomFrameResource(ID3D12Device* device, uint iterations) :
			mIterations(iterations)
		{
			descHeap.New(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, iterations * 3 + 1, true);
			ubuffer.New(device, iterations * 2, true, sizeof(CBufferParams));
		}
		void Update(ID3D12Device* device, uint iterations)
		{
			if (mIterations != iterations)
			{
				mIterations = iterations;
				descHeap->Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, iterations * 3 + 1, true);
				ubuffer->Create(device, iterations * 2, true, sizeof(CBufferParams));
			}
		}
		~BloomFrameResource()
		{
			descHeap.Delete();
			ubuffer.Delete();
		}
	};
	class BloomCameraResource : public IPipelineResource
	{
	public:
		StackObject<TextureHeap> heap;
		std::vector<Level> mipRTs;

		uint mWidth = 0, mHeight = 0;
		uint mIterations = 0;
		BloomCameraResource()
		{
			mipRTs.reserve(20);
			heap.New();
		}

		void UpdateCheck(
			size_t newChunkCount, ID3D12Device* device)
		{
			if (heap->GetChunkSize() >= newChunkCount) return;
			heap.Delete();
			heap.New(device, newChunkCount);
		}
		void UpdateScreenSize(
			ID3D12Device* device, uint width, uint height, uint iterations, float anamorphicRatio)
		{
			const uint pixelSize = 8;
			if (mWidth != width || mHeight != height || iterations != mIterations)
			{
				mWidth = width;
				mIterations = iterations;
				mHeight = height;
				for (auto i = mipRTs.begin(); i != mipRTs.end(); ++i)
				{
					i->up.Delete();
					i->down.Delete();
				}
				mipRTs.clear();
				size_t poolSize = 0;
				
				D3D12_RESOURCE_DESC texDesc;
				ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
				texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				texDesc.Alignment = 0;
				texDesc.DepthOrArraySize = 1;
				texDesc.MipLevels = 1;
				texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				texDesc.SampleDesc.Count = 1;
				texDesc.SampleDesc.Quality = 0;
				texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				
				for (uint i = 0; i < iterations; ++i)
				{
					RenderTextureFormat format;
					format.usage = RenderTextureUsage::ColorBuffer;
					format.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
					size_t rtSize = RenderTexture::GetSizeFromProperty(
						device, width, height,
						format, TextureDimension::Tex2D,
						1, 1,
						RenderTextureState::Generic_Read);
					texDesc.Width = width;
					texDesc.Height = height;
					poolSize += rtSize * 2;
					width = Max<uint>(width / 2, 1);
					height = Max<uint>(height / 2, 1);
				}
				UpdateCheck(poolSize, device);
				width = mWidth;
				height = mHeight;
				mipRTs.resize(iterations);
				size_t currentOffset = 0;
				for (uint i = 0; i < iterations; ++i)
				{
					Level& l = mipRTs[i];
					RenderTextureFormat format;
					format.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
					format.usage = RenderTextureUsage::ColorBuffer;
					l.down.New(device, width, height, format, TextureDimension::Tex2D, 1, 1, RenderTextureState::Generic_Read, heap, currentOffset);
					currentOffset += l.down->GetResourceSize();
					l.up.New(device, width, height, format, TextureDimension::Tex2D, 1, 1, RenderTextureState::Generic_Read, heap, currentOffset);
					currentOffset += l.up->GetResourceSize();
					width = Max<uint>(width / 2, 1);
					height = Max<uint>(height / 2, 1);
				}
			}
		}
		~BloomCameraResource()
		{
			for (auto i = mipRTs.begin(); i != mipRTs.end(); ++i)
			{
				i->up.Delete();
				i->down.Delete();
			}
			heap.Delete();
		}
	};
	Bloom(ID3D12Device* device)
	{
		bloomShader = ShaderCompiler::GetShader("Bloom");
		_BloomTex = ShaderID::PropertyToID("_BloomTex");
		_AutoExposureTex = ShaderID::PropertyToID("_AutoExposureTex");
		Params = ShaderID::PropertyToID("Params");
		DXGI_FORMAT rtFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		bloomPSO.New(DXGI_FORMAT_UNKNOWN, 1, &rtFormat);
	}
	void Render(
		ID3D12GraphicsCommandList* commandList,
		ID3D12Device* device,
		uint width, uint height,
		RenderTexture* source,
		Camera* cam, FrameResource* res,
		RenderTexture* autoExposure,
		BloomCBuffers& resultSettings,
		Texture*& resultDirtTex,
		RenderTexture*& bloomTex,
		TransitionBarrierBuffer* transitionBuffer)
	{
		BloomCameraResource* camRes = (BloomCameraResource*)cam->GetResource(this, []()->BloomCameraResource*
			{
				return new BloomCameraResource;
			});

		float ratio = MathHelper::Clamp<float>(anamorphicRatio, -1, 1);
		float rw = ratio < 0 ? -ratio : 0;
		float rh = ratio > 0 ? ratio : 0;
		uint tw = (uint)(width / (2.0f - rw));
		uint th = (uint)(height / (2.0f - rh));
		uint s = Max(tw, th);
		float logs = log2(s) + Min<float>(diffusion, 10) - 10;
		uint logs_i = (uint)(logs);
		uint iterations = MathHelper::Clamp<uint>(logs_i, 1, k_MaxPyramidSize);
		BloomFrameResource* frameRes = (BloomFrameResource*)res->GetPerCameraResource(this, cam, [&]()->BloomFrameResource*
			{
				return new BloomFrameResource(device, iterations);
			});
		frameRes->Update(device, iterations);
		float sampleScale = 0.5f + logs - logs_i;
		CBufferParams cbufferParams;
		cbufferParams._ColorIntensity = { color.x, color.y, color.z, 1 };
		cbufferParams._SampleScale = sampleScale;
		float lthresh = pow(threshold, 2.2);
		float knee = lthresh * softKnee + 1e-5f;
		float4 thresholdVec = { lthresh, lthresh - knee, knee * 2.0f, 0.25f / knee };
		cbufferParams._Threshold = thresholdVec;
		float lclamp = pow(clampValue, 2.2);
		cbufferParams._Params = { lclamp, 0, 0, 0 };

		camRes->UpdateScreenSize(device, tw, th, iterations, anamorphicRatio);
		uint descHeapIndex = 0;
		uint ubufferIndex = 0;
		RenderTexture* lastDown = source;
		bloomShader->BindRootSignature(commandList, frameRes->descHeap);
		autoExposure->BindSRVToHeap(frameRes->descHeap, descHeapIndex, device);
		bloomShader->SetResource(commandList, _AutoExposureTex, frameRes->descHeap, descHeapIndex);
		descHeapIndex++;
		uint qualityOffset = 0;//qualityOffset = 1 for fast mode!

		for (uint i = 0; i < iterations; ++i)
		{
			int pass = i == 0
				? 0 + qualityOffset
				: 2 + qualityOffset;
			lastDown->BindSRVToHeap(frameRes->descHeap, descHeapIndex, device);
			bloomShader->SetResource(commandList, ShaderID::GetMainTex(), frameRes->descHeap, descHeapIndex);
			descHeapIndex++;
			cbufferParams._MainTex_TexelSize =
			{
				1.0f / lastDown->GetWidth(),
				1.0f / lastDown->GetHeight(),
				(float)lastDown->GetWidth(),
				(float)lastDown->GetHeight()
			};
			frameRes->ubuffer->CopyData(ubufferIndex, &cbufferParams);
			bloomShader->SetResource(commandList, Params, frameRes->ubuffer, ubufferIndex);
			ubufferIndex++;

			
			RenderTexture* mipDown = camRes->mipRTs[i].down;
			//RenderTexture* mipUp = camRes->mipRTs[i].up;
			transitionBuffer->AddCommand(mipDown->GetReadState(), mipDown->GetWriteState(), mipDown->GetResource());
			transitionBuffer->ExecuteCommand(commandList);
			Graphics::Blit(
				commandList, device,
				&mipDown->GetColorDescriptor(0, 0), 1,
				nullptr, bloomPSO, 0, mipDown->GetWidth(), mipDown->GetHeight(), bloomShader, pass);
			transitionBuffer->AddCommand(mipDown->GetWriteState(), mipDown->GetReadState(), mipDown->GetResource());
			lastDown = mipDown;
		}
		RenderTexture* lastUp = camRes->mipRTs[iterations - 1].down;
		for (int i = (int)iterations - 2; i >= 0; i--)
		{
			RenderTexture* mipDown = camRes->mipRTs[i].down;
			RenderTexture* mipUp = camRes->mipRTs[i].up;
			mipDown->BindSRVToHeap(frameRes->descHeap, descHeapIndex, device);
			bloomShader->SetResource(commandList, _BloomTex, frameRes->descHeap, descHeapIndex);
			descHeapIndex++;
			lastUp->BindSRVToHeap(frameRes->descHeap, descHeapIndex, device);
			bloomShader->SetResource(commandList, ShaderID::GetMainTex(), frameRes->descHeap, descHeapIndex);
			descHeapIndex++;
			cbufferParams._MainTex_TexelSize =
			{
				1.0f / lastUp->GetWidth(),
				1.0f / lastUp->GetHeight(),
				(float)lastUp->GetWidth(),
				(float)lastUp->GetHeight()
			};
			frameRes->ubuffer->CopyData(ubufferIndex, &cbufferParams);
			bloomShader->SetResource(commandList, Params, frameRes->ubuffer, ubufferIndex);
			ubufferIndex++;
			transitionBuffer->AddCommand(mipUp->GetReadState(), mipUp->GetWriteState(), mipUp->GetResource());
			transitionBuffer->ExecuteCommand(commandList);
			Graphics::Blit(
				commandList, device,
				&mipUp->GetColorDescriptor(0, 0), 1, nullptr,
				bloomPSO, 0,
				mipUp->GetWidth(), mipUp->GetHeight(),
				bloomShader, 4 + qualityOffset);
			transitionBuffer->AddCommand(mipUp->GetWriteState(), mipUp->GetReadState(), mipUp->GetResource());
			lastUp = mipUp;
		}
		Vector3 linearColor = pow((Vector3)color, 2.2f);
		float intensity = exp2(intensitySetting / 10.0f) - 1.0f;
		float4 shaderSettings = { sampleScale, intensity, dirtIntensity, (float)iterations };
		float dirtRatio = 1;
		if (dirtTex)
		{
			dirtRatio = (float)dirtTex->GetWidth() / (float)dirtTex->GetHeight();
		}
		float screenRatio = (float)width / (float)height;
		float4 dirtTileOffset = { 1,1,0,0 };

		if (dirtRatio > screenRatio)
		{
			dirtTileOffset.x = screenRatio / dirtRatio;
			dirtTileOffset.z = (1.0f - dirtTileOffset.x) * 0.5f;
		}
		else if (screenRatio > dirtRatio)
		{
			dirtTileOffset.y = dirtRatio / screenRatio;
			dirtTileOffset.w = (1.0f - dirtTileOffset.y) * 0.5f;
		}
		resultSettings._Is_Bloom_FastMode = qualityOffset > 0;
		resultSettings._Use_BloomDirt = (dirtTex != nullptr);
		resultSettings._Bloom_Color = linearColor;
		resultSettings._Bloom_DirtTileOffset = dirtTileOffset;
		resultSettings._Bloom_Settings = shaderSettings;
		resultDirtTex = dirtTex;
		bloomTex = lastUp;
		resultSettings._BloomTex_TexelSize =
		{
			1.0f / bloomTex->GetWidth(),
			1.0f / bloomTex->GetHeight(),
			(float)bloomTex->GetWidth(),
			(float)bloomTex->GetHeight()
		};
	}
	~Bloom()
	{
		bloomPSO.Delete();
	}
};