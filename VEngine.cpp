//***************************************************************************************
// VEngine.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************
#include "RenderComponent/Shader.h"
#include "Common/d3dApp.h"
#include "Common/MathHelper.h"
#include "RenderComponent/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "Singleton/FrameResource.h"
#include "Singleton/ShaderID.h"
#include "Common/Input.h"
#include "RenderComponent/Texture.h"
#include "Singleton/MeshLayout.h"
#include "Singleton/PSOContainer.h"
#include "Common/Camera.h"
#include "RenderComponent/Mesh.h"
#include "RenderComponent/MeshRenderer.h"
#include "Singleton/ShaderCompiler.h"
#include "RenderComponent/Skybox.h"
#include "RenderComponent/ComputeShader.h"
#include "RenderComponent/RenderTexture.h"
#include "PipelineComponent/RenderPipeline.h"
#include "Singleton/Graphics.h"
#include "Singleton/MathLib.h"
#include "JobSystem/JobInclude.h"
#include "ResourceManagement/AssetDatabase.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

#include "LogicComponent/World.h"
#include "IRendererBase.h"


class VEngine : public IRendererBase
{
public:
	std::vector<JobBucket*> buckets[2];

	bool bucketsFlag = false;
	ComPtr<ID3D12CommandQueue> mComputeCommandQueue;
	VEngine() {};
	VEngine(const VEngine& rhs) = delete;
	VEngine& operator=(const VEngine& rhs) = delete;
	void InitRenderer(D3DAppDataPack& pack);
	void DisposeRenderer();
	virtual bool Initialize(D3DAppDataPack& pack);
	virtual void OnResize(D3DAppDataPack& pack);
	virtual bool Draw(D3DAppDataPack& pack, GameTimer&);
	virtual void Dispose(D3DAppDataPack& pack);
	//void AnimateMaterials(const GameTimer& gt);
	//void UpdateObjectCBs(const GameTimer& gt);

	StackObject<JobSystem> pipelineJobSys;
	void BuildFrameResources(D3DAppDataPack& pack);
	bool lastFrameExecute = false;
	int mCurrFrameResourceIndex = 0;
	RenderPipeline* rp;
	
	float mTheta = 1.3f * XM_PI;
	float mPhi = 0.4f * XM_PI;
	float mRadius = 2.5f;
	POINT mLastMousePos;
	FrameResource* lastResource = nullptr;
	StackObject<ThreadCommand> directThreadCommand;
};
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	StackObject<D3DApp> d3dApp;
	StackObject<VEngine> renderer;
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	try
	{

		d3dApp.New(hInstance);
		renderer.New();
		if (!d3dApp->Initialize())
			return 0;
		renderer->Initialize(d3dApp->dataPack);
		int value = d3dApp->Run(renderer);
		if (value == -1)
		{
			return 0;
		}
		renderer->Dispose(d3dApp->dataPack);
		renderer.Delete();
		d3dApp.Delete();

	}
	catch (DxException & e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
#else


	d3dApp.New(hInstance);
	renderer.New();
	if (!d3dApp->Initialize())
		return 0;
	renderer->Initialize(d3dApp->dataPack);

	int value = d3dApp->Run(renderer);
	if (value == -1)
	{
		return 0;
	}
	renderer.Delete();
	d3dApp.Delete();
#endif
}

void VEngine::Dispose(D3DAppDataPack& pack)
{
	if (pack.md3dDevice != nullptr)
		pack.flushCommandQueue(pack);
	pipelineJobSys->Wait();
	
	DisposeRenderer();
	AssetDatabase::DestroyInstance();
	pipelineJobSys.Delete();
	World::DestroyInstance();
	PtrLink::globalEnabled = false;
	mComputeCommandQueue = nullptr;
}

void VEngine::InitRenderer(D3DAppDataPack& pack)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

	ThrowIfFailed(pack.md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mComputeCommandQueue)));
	directThreadCommand.New(pack.md3dDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
	directThreadCommand->ResetCommand();
	// Reset the command list to prep for initialization commands.
	ShaderCompiler::Init(pack.md3dDevice, pipelineJobSys.GetPtr());
	BuildFrameResources(pack);
	
	//BuildPSOs();
	// Execute the initialization commands.
	// Wait until initialization is complete.

	World::CreateInstance(directThreadCommand->GetCmdList(), pack.md3dDevice);

	rp = RenderPipeline::GetInstance(pack.md3dDevice,
		directThreadCommand->GetCmdList());
	Graphics::Initialize(pack.md3dDevice, directThreadCommand->GetCmdList());
	directThreadCommand->CloseCommand();
	ID3D12CommandList* lst = directThreadCommand->GetCmdList();
	pack.mCommandQueue->ExecuteCommandLists(1, &lst);
	pack.flushCommandQueue(pack);
	directThreadCommand.Delete();
}
void VEngine::DisposeRenderer()
{
	FrameResource::mFrameResources.clear();
	
	RenderPipeline::DestroyInstance();
	ShaderCompiler::Dispose();
}
bool VEngine::Initialize(D3DAppDataPack& pack)
{
	PtrLink::globalEnabled = true;
	//mSwapChain->SetFullscreenState(true, nullptr);
	ShaderID::Init();
	buckets[0].reserve(20);
	buckets[1].reserve(20);
	UINT cpuCoreCount = std::thread::hardware_concurrency() - 2;	//One for main thread & one for loading
	pipelineJobSys.New(Max<uint>(1, cpuCoreCount));
	AssetDatabase::CreateInstance(pack.md3dDevice);
	InitRenderer(pack);
	return true;
}
HINSTANCE AppInst_VEngine(const D3DAppDataPack& pack)
{
	return pack.mhAppInst;
}

float AspectRatio_VEngine(const D3DAppDataPack& pack)
{
	return static_cast<float>(pack.mClientWidth) / pack.mClientHeight;
}
void VEngine::OnResize(D3DAppDataPack& pack)
{
	for (uint i = 0; i < FrameResource::mFrameResources.size(); ++i)
	{
		FrameResource::mFrameResources[i]->commandBuffer->Clear();
	}
	lastFrameExecute = false;
}

ID3D12Resource* CurrentBackBuffer_VEngine(const D3DAppDataPack& dataPack)
{
	return dataPack.mSwapChainBuffer[dataPack.mCurrBackBuffer];
}

D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView_VEngine(const D3DAppDataPack& dataPack)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		dataPack.mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		dataPack.mCurrBackBuffer,
		dataPack.mRtvDescriptorSize);
}
bool VEngine::Draw(D3DAppDataPack& pack, GameTimer& timer)
{
	if (pack.mClientHeight < 1 || pack.mClientWidth < 1) return true;
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	lastResource = FrameResource::mCurrFrameResource;
	FrameResource::mCurrFrameResource = FrameResource::mFrameResources[mCurrFrameResourceIndex].get();
	//	FrameResource::mCurrFrameResource->UpdateBeforeFrame(mFence);
	std::vector <JobBucket*>& bucketArray = buckets[bucketsFlag];
	bucketsFlag = !bucketsFlag;
	JobBucket* mainLogicBucket = pipelineJobSys->GetJobBucket();
	bucketArray.push_back(mainLogicBucket);
	World::GetInstance()->PrepareUpdateJob(mainLogicBucket, FrameResource::mCurrFrameResource, pack.md3dDevice, timer, int2(pack.mClientWidth, pack.mClientHeight));
	JobHandle cameraUpdateJob = mainLogicBucket->GetTask(nullptr, 0, [&]()->void
		{
			/*if (Input::isLeftMousePressing())
			{
				int2 mouse = Input::MouseMovement();
				// Make each pixel correspond to a quarter of a degree.
				float dx = XMConvertToRadians(0.25f*static_cast<float>(mouse.x));
				float dy = XMConvertToRadians(0.25f*static_cast<float>(mouse.y));

				// Update angles based on input to orbit camera around box.
				mTheta -= dx;
				mPhi += dy;

				// Restrict the angle mPhi.
				mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
			}
			UpdateCamera(gt);
			*/
			
			AssetDatabase::GetInstance()->MainThreadUpdate();
			World::GetInstance()->Update(FrameResource::mCurrFrameResource, pack.md3dDevice, timer, int2(pack.mClientWidth, pack.mClientHeight));

		});
	//Rendering
	pack.windowInfo = const_cast<wchar_t*>(World::GetInstance()->windowInfo.empty() ? nullptr : World::GetInstance()->windowInfo.c_str());
	pack.mCurrBackBuffer = (pack.mCurrBackBuffer + 1) % pack.SwapChainBufferCount;
	RenderPipelineData data;
	data.device = pack.md3dDevice;
	data.commandQueue = pack.mCommandQueue;
	data.backBufferHandle = CurrentBackBufferView_VEngine(pack);
	data.backBufferResource = CurrentBackBuffer_VEngine(pack);
	data.lastResource = lastResource;
	data.resource = FrameResource::mCurrFrameResource;
	data.allCameras = &World::GetInstance()->GetCameras();
	data.fence = pack.mFence;
	data.fenceIndex = &pack.mCurrentFence;
	data.ringFrameIndex = mCurrFrameResourceIndex;
	data.executeLastFrame = lastFrameExecute;
	data.world = World::GetInstance();
	data.world->windowWidth = pack.mClientWidth;
	data.world->windowHeight = pack.mClientHeight;
	data.deltaTime = timer.DeltaTime();
	data.time = timer.TotalTime();
	rp->PrepareRendering(data, pipelineJobSys.GetPtr(), bucketArray);
	pipelineJobSys->Wait();//Last Frame's Logic Stop Here
	HRESULT crashResult = pack.md3dDevice->GetDeviceRemovedReason();
	if (crashResult != S_OK)
	{
		return false;
	}
	Input::UpdateFrame(int2(-1, -1));//Update Input Buffer
//	SetCursorPos(mClientWidth / 2, mClientHeight / 2);
	data.resource->UpdateBeforeFrame(data.fence);//Flush CommandQueue
	pipelineJobSys->ExecuteBucket(bucketArray.data(), bucketArray.size());					//Execute Tasks

	rp->ExecuteRendering(data);
	HRESULT presentResult = pack.mSwapChain->Present(0, 0);
#if defined(DEBUG) | defined(_DEBUG)
	ThrowHResult(presentResult, PresentFunction);
#endif
	if (presentResult != S_OK)
	{
		return false;
	}
	std::vector <JobBucket*>& lastBucketArray = buckets[bucketsFlag];
	for (auto ite = lastBucketArray.begin(); ite != lastBucketArray.end(); ++ite)
	{
		pipelineJobSys->ReleaseJobBucket(*ite);
	}
	lastBucketArray.clear();
	lastFrameExecute = true;
	return true;
}

void VEngine::BuildFrameResources(D3DAppDataPack& pack)
{
	FrameResource::mFrameResources.resize(gNumFrameResources);
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		FrameResource::mFrameResources[i] = std::unique_ptr<FrameResource>(new FrameResource(pack.md3dDevice, 1, 1));
		FrameResource::mFrameResources[i]->commandBuffer.New(pack.mCommandQueue, mComputeCommandQueue.Get());
	}
}