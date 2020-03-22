#include "VirtualTexture.h"
#include "../ComputeShader.h"
#include "../../Singleton/ShaderID.h"
#include "../TransitionBarrierBuffer.h"
#include "../../Singleton/ShaderCompiler.h"
#include "../../Singleton/FrameResource.h"
#include "../../PipelineComponent/IPipelineResource.h"
#include "../../LogicComponent/World.h"
#include "../../RenderComponent/RenderCommand.h"
const uint VIRTUAL_TEXTURE_MIP_MAXLEVEL = 3;
VirtualTexture::PropertyIDs VirtualTexture::propIDs;
void VirtualTexture::PropertyIDs::Init()
{
	if (initialized) return;
	initialized = true;
	IndirectParams = ShaderID::PropertyToID("IndirectParams");
	CombineParams = ShaderID::PropertyToID("CombineParams");
	_IndirectTex = ShaderID::PropertyToID("_IndirectTex");
	CombineParams = ShaderID::PropertyToID("CombineParams");
	_CombineResultTex = ShaderID::PropertyToID("_CombineResultTex");
	_IndirectTex = ShaderID::PropertyToID("_IndirectTex");
	_CombineTex = ShaderID::PropertyToID("_CombineTex");
	_TexMipLevels = ShaderID::PropertyToID("_TexMipLevels");
	_VirtualTex = ShaderID::PropertyToID("_VirtualTex");
	_IndirectBuffer = ShaderID::PropertyToID("_IndirectBuffer");
	VirtualTextureParams = ShaderID::PropertyToID("VirtualTextureParams");
	_TextureIndexBuffer = ShaderID::PropertyToID("_TextureIndexBuffer");
	_SettingCommand = ShaderID::PropertyToID("_SettingCommand");
	SetBufferParams = ShaderID::PropertyToID("SetBufferParams");
}
class VirtualTextureFrameData : public IPipelineResource
{
public:
	std::vector<ConstBufferElement> constElement;
	StackObject< DescriptorHeap> combineDesc;
	DescriptorHeap computeDescHeap;
	StackObject<DescriptorHeap> mipHeap;
	StackObject<UploadBuffer> setterBuffer;

	VirtualTextureFrameData(ID3D12Device* device) :
		computeDescHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 3, true)
	{
		mipHeap.New(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, VIRTUAL_TEXTURE_MIP_MAXLEVEL * 10, true);
		//combineDesc.New
		combineDesc.New(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32, true);
		constElement.reserve(24);
		setterBuffer.New(device, 50, false, sizeof(int2));
	}
	~VirtualTextureFrameData()
	{
		mipHeap.Delete();
		setterBuffer.Delete();
		combineDesc.Delete();
	}
};

class VTInitMission : public RenderCommand
{
	UploadBuffer ubuffer;
	ComputeShader* settingShader;
	uint _IndirectTex;
	uint CombineParams;
	RenderTexture* indirectTex;
	D3D12_RESOURCE_STATES* indirectState;
	void* vtPtr;
public:
	VTInitMission(ID3D12Device* device, ComputeShader* sha, uint _IndirectTex, uint CombineParams, RenderTexture* indTex, D3D12_RESOURCE_STATES* indicmdrectState, void* vtPtr) :
		ubuffer(device, 1, true, sizeof(uint2)),
		vtPtr(vtPtr),
		indirectState(indicmdrectState),
		settingShader(sha),
		_IndirectTex(_IndirectTex),
		indirectTex(indTex),
		CombineParams(CombineParams)
	{

	}
	virtual void operator()(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		FrameResource* resource,
		TransitionBarrierBuffer* barrierBuffer)
	{
		VirtualTextureFrameData* frameData = (VirtualTextureFrameData*)resource->GetResource(vtPtr, [&]()->VirtualTextureFrameData*
			{
				return new VirtualTextureFrameData(device);
			});
		ubuffer.ReleaseAfterFlush(resource);
		indirectTex->BindUAVToHeap(&frameData->computeDescHeap, 0, device, 0);
		settingShader->BindRootSignature(commandList, &frameData->computeDescHeap);
		settingShader->SetResource(commandList, _IndirectTex, &frameData->computeDescHeap, 0);
		settingShader->SetResource(commandList, CombineParams, &ubuffer, 0);
		uint2* ptr = (uint2*)ubuffer.GetMappedDataPtr(0);
		*ptr = { indirectTex->GetWidth(), indirectTex->GetHeight() };
		const uint2 dispatchCount = { (7 + ptr->x) / 8, (7 + ptr->x) / 8 };
		if (*indirectState != indirectTex->GetWriteState())
		{
			barrierBuffer->AddCommand(*indirectState, indirectTex->GetWriteState(), indirectTex->GetResource());
			*indirectState = indirectTex->GetWriteState();
		}
		barrierBuffer->ExecuteCommand(commandList);
		settingShader->Dispatch(commandList, 3, dispatchCount.x, dispatchCount.y, 1);
		if (*indirectState != indirectTex->GetReadState())
		{
			barrierBuffer->AddCommand(*indirectState, indirectTex->GetReadState(), indirectTex->GetResource());
			*indirectState = indirectTex->GetReadState();
		}

	}
};

void VirtualTexture::SetTextureProperty(
	Shader* targetShader,
	ID3D12GraphicsCommandList* commandList)
{
	targetShader->SetResource(commandList, propIDs._IndirectTex, World::GetInstance()->GetGlobalDescHeap(), indirectTex->GetGlobalDescIndex());
	targetShader->SetResource(commandList, propIDs.VirtualTextureParams, renderConstDataBuffer, 0);
	targetShader->SetStructuredBufferByAddress(
		commandList,
		propIDs._IndirectBuffer,
		textureIndexBuffer->GetAddress(0, 0));
	targetShader->SetResource(commandList, propIDs._VirtualTex, World::GetInstance()->GetGlobalDescHeap(), 0);
}

VirtualTexture::VirtualTexture(
	ID3D12Device* device,
	uint indirectSize,
	uint chunkSize,
	DXGI_FORMAT* formats,
	uint formatCount,
	uint chunkCount) : 
	indirectSize(indirectSize),
	textureCapacity(chunkCount),
	texelSize(chunkSize)
{
	propIDs.Init();
	psoContainer.New(DXGI_FORMAT_UNKNOWN, formatCount, formats);
	settingShader = ShaderCompiler::GetComputeShader("VirtualTextureSetter");
	cbufferPool.New(Max<size_t>(sizeof(IndirectParams), sizeof(CombineParams)), 256);
	size_t fullSize = RenderTexture::GetSizeFromProperty(
		device,
		indirectSize,
		indirectSize,
		RenderTextureFormat::GetColorFormat(DXGI_FORMAT_R16G16B16A16_UINT),
		TextureDimension::Tex2D,
		1,
		1,
		RenderTextureState::Unordered_Access);//Indirect Size
	//computeDescHeap.New(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DESC_HEAP_COUNT * frameResourceCount, true);
	for (uint i = 0; i < formatCount; ++i)
	{
		fullSize += RenderTexture::GetSizeFromProperty(
			device,
			chunkSize,
			chunkSize,
			RenderTextureFormat::GetColorFormat(formats[i]),
			TextureDimension::Tex2D,
			1,
			VIRTUAL_TEXTURE_MIP_MAXLEVEL,
			RenderTextureState::Generic_Read) * chunkCount;
	}
	texHeap.New(device, fullSize);
	size_t offsetCounter = 0;
	indirectTex.New(
		device,
		indirectSize,
		indirectSize,
		RenderTextureFormat::GetColorFormat(DXGI_FORMAT_R16G16B16A16_UINT),
		TextureDimension::Tex2D,
		1,
		1,
		RenderTextureState::Unordered_Access,
		texHeap,
		0);
	renderConstDataBuffer.New(device, 1, false, sizeof(RenderConstSettings));
	RenderConstSettings* constSettings = (RenderConstSettings*)renderConstDataBuffer->GetMappedDataPtr(0);
	constSettings->_MaxMipLevel = VIRTUAL_TEXTURE_MIP_MAXLEVEL - 1;
	constSettings->_ChunkTexelSize = { (float)texelSize, (float)texelSize };
	constSettings->_IndirectTexelSize = { indirectSize ,indirectSize };
	indirectCurrentState = indirectTex->GetInitState();
	offsetCounter += indirectTex->GetResourceSize();
	textureChunks.resize(formatCount);
	textureIndexBuffer.New(
		device, &StructuredBufferElement::Get(sizeof(uint), chunkCount * formatCount), 1, false, true
	);
	VTUpdateCommand command;
	command.type = VTUpdateCommand::UpdateCommandType_ClearBuffer;
	commandBuffer.AddCommand(command);
	for (uint i = 0; i < formatCount; ++i)
	{
		textureChunks[i].resize(chunkCount);
		for (uint j = 0; j < chunkCount; ++j)
		{
			auto& a = textureChunks[i][j];
			a.renderTexture.New(
				device,
				chunkSize,
				chunkSize,
				RenderTextureFormat::GetColorFormat(formats[i]),
				TextureDimension::Tex2D,
				1,
				VIRTUAL_TEXTURE_MIP_MAXLEVEL,
				RenderTextureState::Generic_Read,
				texHeap, offsetCounter);
			offsetCounter += a.renderTexture->GetResourceSize();
			a.heapID = -1;
		}
	}
	indexPool.resize(chunkCount);
#pragma loop(hint_parallel(0))
	for (uint i = 0; i < chunkCount; ++i)
	{
		indexPool[i] = i;
	}
	VTInitMission* initMission = new VTInitMission(
		device,
		settingShader,
		propIDs._IndirectTex,
		propIDs.CombineParams,
		indirectTex,
		&indirectCurrentState,
		this);
	RenderCommand::AddCommand(initMission);
}

VirtualTexture::TextureBlock::TextureBlock(const TextureBlock& block)
{
	initialized = true;
	renderTexture.New(*block.renderTexture);
}
VirtualTexture::TextureBlock::~TextureBlock()
{
	if (initialized) renderTexture.Delete();
}

VirtualTexture::~VirtualTexture()
{
	for (auto ite = FrameResource::mFrameResources.begin(); ite != FrameResource::mFrameResources.end(); ++ite)
	{
		if (*ite)
		{
			(*ite)->DisposeResource(this);
		}
	}
	textureIndexBuffer.Delete();
	psoContainer.Delete();
	indirectTex.Delete();
	texHeap.Delete();
	cbufferPool.Delete();
	renderConstDataBuffer.Delete();
}
bool VirtualTexture::InternalCreate(uint2 index, uint size, VirtualChunk& result)
{
	if (indexPool.empty()) return false;
	auto lastIte = indexPool.end() - 1;
	result.index = *lastIte;
	indexPool.erase(lastIte);
	result.size = size;

	for (uint i = 0; i < textureChunks.size(); ++i)
	{
		auto& a = textureChunks[i][result.index];
		a.heapID = a.renderTexture->GetGlobalDescIndex();
		VTUpdateCommand setBfCmd;
		setBfCmd.type = VTUpdateCommand::UpdateCommandType_SetBuffer;
		setBfCmd.setBufferIndex = { (int32_t)(result.index * textureChunks.size() + i), a.heapID };
		commandBuffer.AddCommand(setBfCmd);
	}
	IndirectParams str;
	str._Var = { index.x, index.y, size, result.index };
	str._IndirectTexelSize = { indirectTex->GetWidth(), indirectTex->GetHeight() };
	VTUpdateCommand command;
	command.type = VTUpdateCommand::UpdateCommandType_Print;
	command.indirectParams = str;
	commandBuffer.AddCommand(command);
	return true;
}

bool VirtualTexture::CreateChunk(uint2 index, uint size, VirtualChunk& result)
{
	auto ite = chunkPoses.find(index);
	if (ite != chunkPoses.end())
	{
		if (ite->second.size != size)
		{
			ite->second.size = size;
			IndirectParams str;
			str._Var = { index.x, index.y, size, ite->second.index };
			str._IndirectTexelSize = { indirectTex->GetWidth(), indirectTex->GetHeight() };
			VTUpdateCommand command;
			command.type = VTUpdateCommand::UpdateCommandType_Print;
			command.indirectParams = str;
			commandBuffer.AddCommand(command);
		}
		result = ite->second;
		return true;
	}
	if (InternalCreate(index, size, result))
	{
		chunkPoses.insert_or_assign(index, result);
		return true;
	}
	return false;
}

void VirtualTexture::VTUpdateCommandBuffer::AddCommand(const VTUpdateCommand& command)
{
	count[command.type]++;
	updateCommand.push_back(command);
}
void VirtualTexture::VTUpdateCommandBuffer::ClearCommand()
{
	updateCommand.clear();
}


bool VirtualTexture::GetChunk(uint2 index, uint size, VirtualChunk& result)
{
	auto ite = chunkPoses.find(index);
	if (ite != chunkPoses.end())
	{
		if (ite->second.size != size)
		{
			return false;
		}
		result = ite->second;
		return true;
	}
	return false;
}

bool VirtualTexture::GetChunk(uint2 index, VirtualChunk& result)
{
	auto ite = chunkPoses.find(index);
	if (ite != chunkPoses.end())
	{
		result = ite->second;
		return true;
	}
	return false;
}

void VirtualTexture::ReturnChunk(uint2 index, bool sendBufferCommand)
{
	auto ite = chunkPoses.find(index);
	if (ite != chunkPoses.end())
	{

		indexPool.push_back(ite->second.index);
		if (sendBufferCommand)
		{
			VTUpdateCommand cmd;
			cmd.type = VTUpdateCommand::UpdateCommandType_SetBuffer;
			for (uint i = 0; i < textureChunks.size(); ++i)
			{
				cmd.setBufferIndex = {
					(int32_t)(ite->second.index * textureChunks.size() + i),
					-1
				};
				commandBuffer.AddCommand(cmd);
			}
		}
		chunkPoses.erase(ite);
	}
}

bool VirtualTexture::GenerateMip(uint2 index)
{
	auto ite = chunkPoses.find(index);
	if (ite == chunkPoses.end()) return false;
	VTUpdateCommand updateCmd;
	updateCmd.type = VTUpdateCommand::UpdateCommandType_GenerateMip;
	updateCmd.generateMipTarget = ite->second.index;
	commandBuffer.AddCommand(updateCmd);
	return true;

}

bool VirtualTexture::CombineUpdate(uint2 index, uint targetSize)
{
	if (indexPool.empty()) return false;
	VirtualChunk chunk[4];
	uint sonSize = targetSize / 2;
	if (!GetChunk(index, sonSize, chunk[0]))
		return false;
	if (!GetChunk({ index.x + sonSize, index.y }, sonSize, chunk[1]))
		return false;
	if (!GetChunk({ index.x, index.y + sonSize }, sonSize, chunk[2]))
		return false;
	if (!GetChunk({ index.x + sonSize, index.y + sonSize }, sonSize, chunk[3]))
		return false;
	VirtualChunk combinedChunk;
	InternalCreate(index, targetSize, combinedChunk);
	VTUpdateCommand cmd;
	cmd.type = VTUpdateCommand::UpdateCommandType_Combine;
	cmd.combineCommand.combineParams._TexelSize = { texelSize, texelSize };
	for (auto ite = textureChunks.begin(); ite != textureChunks.end(); ++ite)
	{
		for (uint i = 0; i < 4; ++i)
		{
			cmd.combineCommand.combineSource[i] = (*ite)[chunk[i].index].renderTexture;
		}
		cmd.combineCommand.combineDest = (*ite)[combinedChunk.index].renderTexture;
		commandBuffer.AddCommand(cmd);
	}
	ReturnChunk(index, false);
	ReturnChunk({ index.x + sonSize, index.y }, false);
	ReturnChunk({ index.x, index.y + sonSize }, false);
	ReturnChunk({ index.x + sonSize, index.y + sonSize }, false);
	chunkPoses.insert_or_assign(index, combinedChunk);
	return true;
}

void VirtualTexture::VTUpdateCommand::operator=(const VTUpdateCommand& command)
{
	if (executorInitialized)
	{
		executorInitialized = false;
		executor.Delete();
	}
	type = command.type;
	switch (command.type)
	{
	case VTUpdateCommand::UpdateCommandType_Executor:
		type = command.type;
		executorInitialized = true;
		executor.New(*command.executor);
		break;
	default:
		memcpy(this, &command, sizeof(VTUpdateCommand));
		break;
	}
}

VirtualTexture::VTUpdateCommand::VTUpdateCommand(const VTUpdateCommand& command)
{

	switch (command.type)
	{
	case VTUpdateCommand::UpdateCommandType_Executor:
		type = command.type;
		executorInitialized = true;
		executor.New(*command.executor);
		break;
	default:
		memcpy(this, &command, sizeof(VTUpdateCommand));
		break;
	}
}

VirtualTexture::VTUpdateCommand::~VTUpdateCommand()
{
	if (executorInitialized)
		executor.Delete();
}

void VirtualTexture::ExecuteUpdate(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	FrameResource* currentResource,
	TransitionBarrierBuffer* barrierBuffer)
{
	VirtualTextureFrameData* frameData = (VirtualTextureFrameData*)currentResource->GetResource(this, [&]()->VirtualTextureFrameData*
		{
			return new VirtualTextureFrameData(device);
		});

	for (auto ite = frameData->constElement.begin(); ite != frameData->constElement.end(); ++ite)
	{
		cbufferPool->Return(*ite);
	}
	frameData->constElement.clear();
	uint combineCommandCount = commandBuffer.GetCommandCount(VTUpdateCommand::UpdateCommandType_Combine);
	if (frameData->combineDesc->Size() < combineCommandCount * 5)
	{
		uint newSize = Max<uint>(combineCommandCount * 5, frameData->combineDesc->Size() * 2);
		frameData->combineDesc.Delete();
		frameData->combineDesc.New(
			device,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			newSize,
			true);
	}
	uint combineDescCurrentIndex = 0;
	uint mipDescCurrentIndex = 0;
	uint mipCmmandCount = commandBuffer.GetCommandCount(VTUpdateCommand::UpdateCommandType_GenerateMip);
	if (frameData->mipHeap->Size() < mipCmmandCount * VIRTUAL_TEXTURE_MIP_MAXLEVEL * textureChunks.size())
	{
		uint newSize = Max<uint>(mipCmmandCount * VIRTUAL_TEXTURE_MIP_MAXLEVEL * textureChunks.size(), frameData->combineDesc->Size() * 2);
		frameData->mipHeap.Delete();
		frameData->mipHeap.New(
			device,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			newSize,
			true);
	}
	settingShader->BindRootSignature(commandList);
	indirectTex->BindUAVToHeap(&frameData->computeDescHeap, 0, device, 0);
	VirtualTextureRenderArgs arg = {
				device,
				commandList,
				currentResource,
				barrierBuffer
	};

	for (auto ite = commandBuffer.updateCommand.begin(); ite != commandBuffer.updateCommand.end(); ++ite)
	{
		switch (ite->type)
		{
		case VTUpdateCommand::UpdateCommandType_Print:
		{
			ConstBufferElement ele = cbufferPool->Get(device);
			frameData->constElement.push_back(ele);
			IndirectParams* param = (IndirectParams*)ele.buffer->GetMappedDataPtr(ele.element);
			memcpy(param, &ite->indirectParams, sizeof(IndirectParams));

			frameData->computeDescHeap.SetDescriptorHeap(commandList);

			settingShader->SetResource(commandList, propIDs.IndirectParams, ele.buffer, ele.element);
			settingShader->SetResource(commandList, propIDs._IndirectTex, &frameData->computeDescHeap, 0);

			const uint dispatchCount = (7 + ite->indirectParams._Var.z) / 8;
			if (indirectCurrentState != indirectTex->GetWriteState())
			{
				barrierBuffer->AddCommand(indirectCurrentState, indirectTex->GetWriteState(), indirectTex->GetResource());
				indirectCurrentState = indirectTex->GetWriteState();
			}
			barrierBuffer->ExecuteCommand(commandList);
			settingShader->Dispatch(commandList, 0, dispatchCount, dispatchCount, 1);
			if (indirectCurrentState != indirectTex->GetReadState())
			{
				barrierBuffer->AddCommand(indirectCurrentState, indirectTex->GetReadState(), indirectTex->GetResource());
				indirectCurrentState = indirectTex->GetReadState();
			}
		}
		break;
		case VTUpdateCommand::UpdateCommandType_Combine:
		{
			frameData->combineDesc->SetDescriptorHeap(commandList);
			settingShader->SetResource(commandList, propIDs._CombineTex, frameData->combineDesc, combineDescCurrentIndex);
			auto func = [&](uint i) {
				auto& ptr = ite->combineCommand.combineSource[i];
				ptr->BindSRVToHeap(frameData->combineDesc, combineDescCurrentIndex++, device);
			};
			InnerLoop<decltype(func), 4>(func);
			ite->combineCommand.combineDest->BindUAVToHeap(frameData->combineDesc, combineDescCurrentIndex++, device, 0);
			barrierBuffer->AddCommand(ite->combineCommand.combineDest->GetReadState(), RenderTexture::GetState(RenderTextureState::Unordered_Access), ite->combineCommand.combineDest->GetResource());

			ConstBufferElement ele = cbufferPool->Get(device);
			frameData->constElement.push_back(ele);
			CombineParams* param = (CombineParams*)ele.buffer->GetMappedDataPtr(ele.element);
			memcpy(param, &ite->combineCommand.combineParams, sizeof(CombineParams));

			settingShader->SetResource(commandList, propIDs._CombineResultTex, frameData->combineDesc, combineDescCurrentIndex - 1);
			settingShader->SetResource(commandList, propIDs.CombineParams, ele.buffer, ele.element);
			barrierBuffer->ExecuteCommand(commandList);
			const uint dispatchCount = (texelSize + 15) / 16;
			settingShader->Dispatch(commandList, 1, dispatchCount, dispatchCount, 1);
			barrierBuffer->AddCommand(RenderTexture::GetState(RenderTextureState::Unordered_Access), ite->combineCommand.combineDest->GetReadState(), ite->combineCommand.combineDest->GetResource());
		}
		break;
		case VTUpdateCommand::UpdateCommandType_GenerateMip:
		{
			frameData->mipHeap->SetDescriptorHeap(commandList);
			for (auto iit = textureChunks.begin(); iit != textureChunks.end(); ++iit)
			{
				settingShader->SetResource(commandList, propIDs._TexMipLevels, frameData->mipHeap, mipDescCurrentIndex);
				RenderTexture* rt = (*iit)[ite->generateMipTarget].renderTexture;
				for (uint i = 0; i < VIRTUAL_TEXTURE_MIP_MAXLEVEL; ++i)
				{
					rt->BindUAVToHeap(frameData->mipHeap, mipDescCurrentIndex++, device, i);
				}
				barrierBuffer->AddCommand(rt->GetReadState(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, rt->GetResource());
				barrierBuffer->ExecuteCommand(commandList);
				const uint dispatchCount = texelSize / 32;
				settingShader->Dispatch(commandList, 2, dispatchCount, dispatchCount, 1);
				barrierBuffer->AddCommand(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, rt->GetReadState(), rt->GetResource());
			}
		}
		break;
		case VTUpdateCommand::UpdateCommandType_Executor:
		{
			VirtualChunk ck;
			if (GetChunk(
				ite->executor->index,
				ite->executor->size,
				ck))
			{
				arg.startIndex = ite->executor->index;
				arg.size = ite->executor->size;
				arg.renderTextureID = ck.index;
				ite->executor->executor(arg);
			}
		}
		break;
		case VTUpdateCommand::UpdateCommandType_ClearBuffer:
		{
			settingShader->SetStructuredBufferByAddress(commandList, propIDs._TextureIndexBuffer, textureIndexBuffer->GetAddress(0, 0));
			ConstBufferElement ele = cbufferPool->Get(device);
			frameData->constElement.push_back(ele);
			uint* texelSize = (uint*)ele.buffer->GetMappedDataPtr(ele.element);
			settingShader->SetResource(commandList, propIDs.SetBufferParams, ele.buffer, ele.element);
			const uint bufferSize = textureIndexBuffer->GetElementCount(0);
			*texelSize = bufferSize;
			const uint dispatchCount = (bufferSize + 63) / 64;
			barrierBuffer->AddCommand(
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				textureIndexBuffer->GetResource()
			);
			barrierBuffer->ExecuteCommand(commandList);
			settingShader->Dispatch(commandList, 4, dispatchCount, 1, 1);
			barrierBuffer->AddCommand(
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				textureIndexBuffer->GetResource()
			);
		}
		break;
		case VTUpdateCommand::UpdateCommandType_SetBuffer:
		{
			bufferSetCommand.push_back(ite->setBufferIndex);
		}
		break;
		}
	}
	commandBuffer.ClearCommand();
	if (!bufferSetCommand.empty())
	{
		if (frameData->setterBuffer->GetElementCount() < bufferSetCommand.size())
		{
			uint maxSize = Max<uint>(frameData->setterBuffer->GetElementCount(), bufferSetCommand.size() * 2);
			frameData->setterBuffer.Delete();
			frameData->setterBuffer.New(device, maxSize, false, sizeof(int2));
		}
		frameData->setterBuffer->CopyDatas(0, bufferSetCommand.size(), bufferSetCommand.data());
		ConstBufferElement ele = cbufferPool->Get(device);
		frameData->constElement.push_back(ele);
		uint* bufferSize = (uint*)ele.buffer->GetMappedDataPtr(ele.element);
		*bufferSize = bufferSetCommand.size();
		const uint dispatchCount = (*bufferSize + 63) / 64;
		settingShader->SetStructuredBufferByAddress(commandList, propIDs._TextureIndexBuffer, textureIndexBuffer->GetAddress(0, 0));
		settingShader->SetResource(commandList, propIDs.SetBufferParams, ele.buffer, ele.element);
		settingShader->SetStructuredBufferByAddress(commandList, propIDs._SettingCommand, frameData->setterBuffer->GetAddress(0));
		barrierBuffer->AddCommand(
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			textureIndexBuffer->GetResource()
		);
		barrierBuffer->ExecuteCommand(commandList);
		settingShader->Dispatch(commandList, 5, dispatchCount, 1, 1);
		barrierBuffer->AddCommand(
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			textureIndexBuffer->GetResource()
		);
		bufferSetCommand.clear();
	}
}