#pragma once
#include "../../Common/d3dUtil.h"
#include "../Texture.h"
#include "../RenderTexture.h"
#include "../../Singleton/PSOContainer.h"
#include "../Shader.h"
#include "../../Singleton/ShaderID.h"
#include "../../Singleton/ShaderCompiler.h"
#include "../../Common/MetaLib.h"
class RandomTexGenerator
{
private:
	StackObject<RenderTexture, true> randomTex;
public:
	RandomTexGenerator(
		uint2 size,
		uint tileCount,
		ID3D12Device* device) 
	{
		randomTex.New(device, size.x, size.y,
			RenderTextureFormat::GetColorFormat(DXGI_FORMAT_R16G16B16A16_SNORM),
			TextureDimension::Tex2D,
			1,
			1,
			RenderTextureState::Unordered_Access);
	}
};