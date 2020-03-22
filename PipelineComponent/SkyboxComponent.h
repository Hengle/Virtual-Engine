#pragma once
#include "../Singleton/PSOContainer.h"
#include "PipelineComponent.h"
class Texture;
class SkyboxComponent : public PipelineComponent
{
	friend class SkyboxRunnable;
protected:
	std::vector<TemporalResourceCommand> tempRT;
	virtual CommandListType GetCommandListType() {
		return CommandListType_Graphics;
	}
	virtual std::vector<TemporalResourceCommand>& SendRenderTextureRequire(EventData& evt);
	virtual void RenderEvent(EventData& data, ThreadCommand* commandList);
	ObjectPtr<Texture> skyboxTex;
public:
	static UINT _AllLight;
	static UINT _LightIndexBuffer;
	static UINT LightCullCBuffer;
	static UINT TextureIndices;
	static uint _GreyTex;
	static uint _IntegerTex;
	static uint _Cubemap;
	RenderTexture* preintTexture;
	StackObject<PSOContainer> preintContainer;
	virtual void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	virtual void Dispose();
};

