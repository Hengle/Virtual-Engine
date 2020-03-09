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
#include "LogicComponent/CameraMove.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

#include "LogicComponent/World.h"
class VEngine : public D3DApp
{
public:
	std::vector<JobBucket*> buckets[2];
	StackObject<CameraMove> camMove;
	bool bucketsFlag = false;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mComputeCommandQueue;
	VEngine(HINSTANCE hInstance);
	VEngine(const VEngine& rhs) = delete;
	VEngine& operator=(const VEngine& rhs) = delete;
	~VEngine();
	void InitRenderer();
	void DisposeRenderer();

	virtual void OnPressMinimizeKey(bool minimize);
	virtual bool Initialize()override;
	virtual void OnResize()override;
	virtual bool Draw(const GameTimer& gt)override;
	void UpdateCamera(const GameTimer& gt);
	//void AnimateMaterials(const GameTimer& gt);
	//void UpdateObjectCBs(const GameTimer& gt);

	StackObject<JobSystem> pipelineJobSys;
	void BuildFrameResources();
	bool lastFrameExecute = false;
	int mCurrFrameResourceIndex = 0;
	RenderPipeline* rp;
	ObjectPtr<Camera> mainCamera;
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
	StackObject<VEngine> theApp;
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	try
	{
		
		theApp.New(hInstance);
		if (!theApp->Initialize())
			return 0;

		int value = theApp->Run();
		if (value == -1)
		{
			return 0;
		}
		theApp.Delete();
	}
	catch (DxException & e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
#else
	
	theApp.New(hInstance);
	if (!theApp->Initialize())
		return 0;

	int value = theApp->Run();
	if (value == -1)
	{
		return 0;
	}
#endif
	theApp.Delete();
}

void VEngine::OnPressMinimizeKey(bool minimize)
{

}

VEngine::VEngine(HINSTANCE hInstance)
	: D3DApp(hInstance)
{

}

void VEngine::InitRenderer()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mComputeCommandQueue)));
	directThreadCommand.New(md3dDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
	directThreadCommand->ResetCommand();
	// Reset the command list to prep for initialization commands.
	ShaderCompiler::Init(md3dDevice.Get(), pipelineJobSys.GetPtr());
	BuildFrameResources();
	mainCamera = ObjectPtr< Camera>::MakePtr(new Camera(md3dDevice.Get(), Camera::CameraRenderPath::DefaultPipeline));
	camMove.New(mainCamera);
	//BuildPSOs();
	// Execute the initialization commands.
	// Wait until initialization is complete.

	World::CreateInstance(directThreadCommand->GetCmdList(), md3dDevice.Get());

	rp = RenderPipeline::GetInstance(md3dDevice.Get(),
		directThreadCommand->GetCmdList());
	Graphics::Initialize(md3dDevice.Get(), directThreadCommand->GetCmdList());
	directThreadCommand->CloseCommand();
	ID3D12CommandList* lst = directThreadCommand->GetCmdList();
	mCommandQueue->ExecuteCommandLists(1, &lst);
	FlushCommandQueue();
	directThreadCommand.Delete();
}
void VEngine::DisposeRenderer()
{
	FrameResource::mFrameResources.clear();
	mainCamera.Destroy();
	RenderPipeline::DestroyInstance();
	ShaderCompiler::Dispose();
}

VEngine::~VEngine()
{
	mSwapChain->SetFullscreenState(false, nullptr);
	pipelineJobSys->Wait();
	camMove.Delete();
	
	if (md3dDevice != nullptr)
		FlushCommandQueue();
	DisposeRenderer();
	AssetDatabase::DestroyInstance();
	pipelineJobSys.Delete();
	World::DestroyInstance();
	PtrLink::globalEnabled = false;
	mComputeCommandQueue = nullptr;

}
bool VEngine::Initialize()
{
	PtrLink::globalEnabled = true;
	if (!D3DApp::Initialize())
		return false;
	//mSwapChain->SetFullscreenState(true, nullptr);
	ShaderID::Init();
	buckets[0].reserve(20);
	buckets[1].reserve(20);
	UINT cpuCoreCount = std::thread::hardware_concurrency() - 2;	//One for main thread & one for loading
	pipelineJobSys.New(Max<uint>(1, cpuCoreCount));
	AssetDatabase::CreateInstance();
	InitRenderer();
	return true;
}

void VEngine::OnResize()
{
	for (uint i = 0; i < FrameResource::mFrameResources.size(); ++i)
	{
		FrameResource::mFrameResources[i]->commandBuffer->Clear();
	}
	D3DApp::OnResize();
	lastFrameExecute = false;
}

std::vector<Camera*> cam(1);
bool VEngine::Draw(const GameTimer& gt)
{
	if (mClientHeight < 1 || mClientWidth < 1) return true;
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	lastResource = FrameResource::mCurrFrameResource;
	FrameResource::mCurrFrameResource = FrameResource::mFrameResources[mCurrFrameResourceIndex].get();
	//	FrameResource::mCurrFrameResource->UpdateBeforeFrame(mFence.Get());
	std::vector <JobBucket*>& bucketArray = buckets[bucketsFlag];
	bucketsFlag = !bucketsFlag;
	JobBucket* mainLogicBucket = pipelineJobSys->GetJobBucket();
	bucketArray.push_back(mainLogicBucket);
	JobHandle cameraUpdateJob = mainLogicBucket->GetTask([&]()->void
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
			camMove->Run(gt.DeltaTime());
			mainCamera->SetLens(0.333333 * MathHelper::Pi, AspectRatio(), 0.2, 100);
			AssetDatabase::GetInstance()->MainThreadUpdate();
			World::GetInstance()->Update(FrameResource::mCurrFrameResource, md3dDevice.Get());

		}, nullptr, 0);
	//Rendering
	cam[0] = mainCamera.operator->();
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
	RenderPipelineData data;
	data.device = md3dDevice.Get();
	data.commandQueue = mCommandQueue.Get();
	data.backBufferHandle = CurrentBackBufferView();
	data.backBufferResource = CurrentBackBuffer();
	data.lastResource = lastResource;
	data.resource = FrameResource::mCurrFrameResource;
	data.allCameras = &cam;
	data.fence = mFence.Get();
	data.fenceIndex = &mCurrentFence;
	data.ringFrameIndex = mCurrFrameResourceIndex;
	data.executeLastFrame = lastFrameExecute;
	data.world = World::GetInstance();
	data.world->windowWidth = mClientWidth;
	data.world->windowHeight = mClientHeight;
	data.deltaTime = gt.DeltaTime();
	data.time = gt.TotalTime();
	rp->PrepareRendering(data, pipelineJobSys.GetPtr(), bucketArray);
	pipelineJobSys->Wait();//Last Frame's Logic Stop Here
	HRESULT crashResult = md3dDevice->GetDeviceRemovedReason();
	if (crashResult != S_OK)
	{
		return false;
	}
	Input::UpdateFrame(int2(-1, -1));//Update Input Buffer
//	SetCursorPos(mClientWidth / 2, mClientHeight / 2);
	data.resource->UpdateBeforeFrame(data.fence);//Flush CommandQueue
	pipelineJobSys->ExecuteBucket(bucketArray.data(), bucketArray.size());					//Execute Tasks

	rp->ExecuteRendering(data);
	HRESULT presentResult = mSwapChain->Present(0, 0);
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

void VEngine::UpdateCamera(const GameTimer& gt)
{
	mRadius = 1;
	// Convert Spherical to Cartesian coordinates.
	XMFLOAT3 mEyePos;
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);
	XMFLOAT3 target = { 0,0,0 };
	XMFLOAT3 up = { 0,1,0 };
	mainCamera->LookAt(target, mEyePos, up);

}

void VEngine::BuildFrameResources()
{
	FrameResource::mFrameResources.resize(gNumFrameResources);
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		FrameResource::mFrameResources[i] = std::unique_ptr<FrameResource>(new FrameResource(md3dDevice.Get(), 1, 1));
		FrameResource::mFrameResources[i]->commandBuffer.New(mCommandQueue.Get(), mComputeCommandQueue.Get());
	}
}