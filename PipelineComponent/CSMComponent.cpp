#include "CSMComponent.h"
#include "PrepareComponent.h"
#include "LightingComponent.h"
#include "../RenderComponent/Light.h"
#include "CameraData/CameraTransformData.h"
#include "RenderPipeline.h"
#include "../Singleton/MathLib.h"
#include "../RenderComponent/ComputeShader.h"
#include "../Singleton/ShaderCompiler.h"
#include "../Singleton/ShaderID.h"
#include "../LogicComponent/Transform.h"
#include "../LogicComponent/DirectionalLight.h"
#include "../RenderComponent/GRPRenderManager.h"
#include "../LogicComponent/World.h"
#include "../Singleton/Graphics.h"
#include "../Singleton/PSOContainer.h"
#include "CameraData/LightCBuffer.h"
namespace CSM
{
	PrepareComponent* prepareComp;
	LightingComponent* lightComp;
	const float zDepth = 500;
	uint ProjectionShadowParams;
	std::unique_ptr<PSOContainer> container;
}
using namespace CSM;
using namespace Math;
struct ShadowmapData
{
	Matrix4 vpMatrix;
	Vector4 position;
	float size;
};
void GetCascadeShadowmapMatrices(
	const Vector3& sunRight,
	const Vector3& sunUp,
	const Vector3& sunForward,
	const Vector3& cameraRight,
	const Vector3& cameraUp,
	const Vector3& cameraForward,
	const Vector3& cameraPosition,
	float fov,
	float aspect,
	float* distances,
	uint distanceCount,
	ShadowmapData* results,
	Vector4* lastPositions,
	uint* shadowResolutions)
{
	
	Matrix4 sunLocalToWorld = transpose(GetTransformMatrix(
		sunRight,
		sunUp,
		sunForward,
		Vector3(0, 0, 0)
	));

	Vector3 corners[8];
	Vector3* lastCorners = corners;
	Vector3* nextCorners = corners + 4;
	MathLib::GetCameraNearPlanePoints(
		cameraRight,
		cameraUp,
		cameraForward,
		cameraPosition,
		fov,
		aspect, distances[0], nextCorners);
	for (uint i = 0; i < distanceCount; ++i)
	{
		{
			Vector3* swaper = lastCorners;
			lastCorners = nextCorners;
			nextCorners = swaper;
		}
		MathLib::GetCameraNearPlanePoints(
			cameraRight,
			cameraUp,
			cameraForward,
			cameraPosition,
			fov,
			aspect, distances[i + 1], nextCorners);
		float farDist = distance(nextCorners[0], nextCorners[3]);
		float crossDist = distance(lastCorners[0], nextCorners[3]);
		float maxDist = Max(farDist, crossDist);
		Matrix4 projMatrix = XMMatrixOrthographicLH(maxDist, maxDist, 0, -zDepth);
		Matrix4 sunWorldToLocal = inverse(sunLocalToWorld);
		Vector4 minBoundingPoint, maxBoundingPoint;
		{
			minBoundingPoint = mul(sunWorldToLocal, Vector4(corners[0], 1));
			maxBoundingPoint = minBoundingPoint;
		}
		for (uint j = 1; j < 8; ++j)
		{
			Vector4 pointLocalPos = mul(sunWorldToLocal, Vector4(corners[j], 1));
			minBoundingPoint = Min(pointLocalPos, minBoundingPoint);
			maxBoundingPoint = Max(pointLocalPos, maxBoundingPoint);
		}
		Vector4 localPosition = (minBoundingPoint + maxBoundingPoint) * 0.5f;
		localPosition.SetZ(maxBoundingPoint.GetZ());
		Vector4 positionMovement = localPosition - lastPositions[i];
		float pixelLength = (maxDist) / shadowResolutions[i];
		positionMovement = floor(positionMovement / pixelLength) * pixelLength;
		localPosition = lastPositions[i] + positionMovement;
		localPosition.SetW(1);
		Vector3 position = mul(sunLocalToWorld, localPosition);
		lastPositions[i] = localPosition;
		Matrix4 viewMat = inverse(GetTransformMatrix(
			sunRight,
			sunUp,
			sunForward,
			position));
		results[i] =
		{
			mul(viewMat, projMatrix),
			position,
			maxDist * 0.5f
		};
	}
}
class CSMLastDirectLightPosition : public IPipelineResource
{
public:
	UploadBuffer cullBuffer;
	UploadBuffer csmParamBuffer;
	Vector4 sunPositions[DirectionalLight::CascadeLevel];
	CSMLastDirectLightPosition(ID3D12Device* device) :
		cullBuffer(device, DirectionalLight::CascadeLevel * 3, true, sizeof(GRPRenderManager::CullData)),
		csmParamBuffer(device, DirectionalLight::CascadeLevel * 3, true, sizeof(ShadowmapDrawParam))
	{
		memset(sunPositions, 0, sizeof(Vector3) * DirectionalLight::CascadeLevel);
	}
};
class CSMRunnable
{
public:

	Camera* cam;
	FrameResource* res;
	ThreadCommand* tCmd;
	CSMComponent* selfPtr;
	ID3D12Device* device;
	World* world;
	uint frameIndex;
	void operator()()
	{
		tCmd->ResetCommand();
		auto commandList = tCmd->GetCmdList();
		DirectionalLight* dLight = DirectionalLight::GetInstance();
		if (dLight)
		{
			float dist[DirectionalLight::CascadeLevel + 1];
			ShadowmapData vpMatrices[DirectionalLight::CascadeLevel];
			dist[0] = cam->GetNearZ();
			memcpy(dist + 1, &dLight->shadowDistance, sizeof(float) * DirectionalLight::CascadeLevel);
			Vector3 sunRight = dLight->transform->GetRight();
			Vector3 sunUp = dLight->transform->GetUp();
			Vector3 sunForward = dLight->transform->GetForward();
			sunForward = -sunForward;
			CSMLastDirectLightPosition* lastLightData = (CSMLastDirectLightPosition*)cam->GetResource(selfPtr, [&]()->CSMLastDirectLightPosition*
				{
					return new CSMLastDirectLightPosition(device);
				});
			uint shadowReses[DirectionalLight::CascadeLevel] =
			{
				dLight->GetShadowmapResolution(0),
				dLight->GetShadowmapResolution(1),
				dLight->GetShadowmapResolution(2),
				dLight->GetShadowmapResolution(3)
			};
			GetCascadeShadowmapMatrices(
				sunRight,
				sunUp,
				sunForward,
				cam->GetRight(),
				cam->GetUp(),
				cam->GetLook(),
				cam->GetPosition(),
				cam->GetFovY(),
				cam->GetAspect(),
				dist,
				DirectionalLight::CascadeLevel,
				vpMatrices,
				lastLightData->sunPositions,
				shadowReses);
			LightCameraData* camData = (LightCameraData*)cam->GetResource(lightComp, [&]()->LightCameraData*
				{
					throw "Should Not Be Here!";
					return nullptr;
				});

			LightCullCBuffer& cb = *(LightCullCBuffer*)camData->lightCBuffer.GetMappedDataPtr(frameIndex);
			cb._CascadeDistance = {
				dLight->shadowDistance[0],
				dLight->shadowDistance[1],
				dLight->shadowDistance[2],
				dLight->shadowDistance[3]
			};
			cb._ShadowmapIndices = {
				dLight->GetShadowmap(0)->GetGlobalDescIndex(),
				dLight->GetShadowmap(1)->GetGlobalDescIndex(),
				dLight->GetShadowmap(2)->GetGlobalDescIndex(),
				dLight->GetShadowmap(3)->GetGlobalDescIndex()
			};
			cb._ShadowSoftValue =
			{
				dLight->shadowSoftValue[0] / dLight->GetShadowmapResolution(0),
				dLight->shadowSoftValue[1] / dLight->GetShadowmapResolution(1),
				dLight->shadowSoftValue[2] / dLight->GetShadowmapResolution(2),
				dLight->shadowSoftValue[3] / dLight->GetShadowmapResolution(3)
			};
			cb._ShadowOffset =
			{
				dLight->shadowBias[0] / zDepth,
				dLight->shadowBias[1] / zDepth,
				dLight->shadowBias[2] / zDepth,
				dLight->shadowBias[3] / zDepth
			};
			Vector4 frustumPlanes[6];
			Vector3 frustumPoints[8];
			float4 frustumPInFloat[6];
			Vector3 frustumMinPoint, frustumMaxPoint;
			for (uint i = 0; i < DirectionalLight::CascadeLevel; ++i)
			{
				auto shadowmap = dLight->GetShadowmap(i);
				tCmd->GetBarrierBuffer()->AddCommand(shadowmap->GetReadState(), shadowmap->GetWriteState(), shadowmap->GetResource());
			}
			tCmd->GetBarrierBuffer()->ExecuteCommand(commandList);
			for (uint i = 0; i < DirectionalLight::CascadeLevel; ++i)
			{
				auto& m = vpMatrices[i];
				MathLib::GetOrthoCamFrustumPlanes(sunRight, sunUp, sunForward, m.position, m.size, m.size, -zDepth, 0, frustumPlanes);
				MathLib::GetOrthoCamFrustumPoints(sunRight, sunUp, sunForward, m.position, m.size, m.size, -zDepth, 0, frustumPoints);
				for (uint j = 0; j < 6; ++j)
				{
					frustumPInFloat[j] = frustumPlanes[j];
				}
				auto shadowmap = dLight->GetShadowmap(i);

				frustumMinPoint = frustumPoints[0];
				frustumMaxPoint = frustumMinPoint;
				for (uint j = 1; j < 8; ++j)
				{
					frustumMinPoint = Min(frustumMinPoint, frustumPoints[j]);
					frustumMaxPoint = Max(frustumMaxPoint, frustumPoints[j]);
				}

				commandList->OMSetRenderTargets(
					0, nullptr, false, &shadowmap->GetColorDescriptor(0, 0));
				shadowmap->ClearRenderTarget(commandList, 0, 0, 0);
				shadowmap->SetViewport(commandList);
				ShadowmapDrawParam* drawParams = (ShadowmapDrawParam*)lastLightData->csmParamBuffer.GetMappedDataPtr(i + DirectionalLight::CascadeLevel * frameIndex);
				world->GetGRPRenderManager()->GetShader()->BindRootSignature(commandList);
				drawParams->_LightPos = { 0,0,0 };
				drawParams->_ShadowmapVP = m.vpMatrix;
				cb._ShadowMatrix[i] = drawParams->_ShadowmapVP;

				world->GetGRPRenderManager()->Culling(
					commandList,
					*tCmd->GetBarrierBuffer(),
					device,
					res,
					{ &lastLightData->cullBuffer, i + DirectionalLight::CascadeLevel * frameIndex },
					frustumPInFloat,
					frustumMinPoint,
					frustumMaxPoint,
					*(float4x4*)nullptr,
					*(float4x4*)nullptr,
					0,
					false);
				tCmd->GetBarrierBuffer()->ExecuteCommand(commandList);
				auto v = i + DirectionalLight::CascadeLevel * frameIndex;
				world->GetGRPRenderManager()->DrawCommand(
					commandList,
					device,
					2,
					ProjectionShadowParams,
					{ &lastLightData->csmParamBuffer, i + DirectionalLight::CascadeLevel * frameIndex },
					container.get(),
					0);
				//TODO
				//Write Shader
			}
			for (uint i = 0; i < DirectionalLight::CascadeLevel; ++i)
			{
				auto shadowmap = dLight->GetShadowmap(i);
				tCmd->GetBarrierBuffer()->AddCommand(shadowmap->GetWriteState(), shadowmap->GetReadState(), shadowmap->GetResource());
			}
		}
		tCmd->CloseCommand();
	}
};

void CSMComponent::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	prepareComp = RenderPipeline::GetComponent<PrepareComponent>();
	lightComp = RenderPipeline::GetComponent<LightingComponent>();
	SetCPUDepending<LightingComponent>();
	ProjectionShadowParams = ShaderID::PropertyToID("ProjectionShadowParams");
	container = std::unique_ptr<PSOContainer>(new PSOContainer(DXGI_FORMAT_D32_FLOAT, 0, nullptr));
}
void CSMComponent::Dispose()
{
	container = nullptr;
}
std::vector<TemporalResourceCommand>& CSMComponent::SendRenderTextureRequire(EventData& evt)
{
	return tempResources;
}
void CSMComponent::RenderEvent(EventData& data, ThreadCommand* commandList)
{
	ScheduleJob< CSMRunnable>(
		{
			data.camera,
			data.resource,
			commandList,
			this,
			data.device,
			data.world,
			data.ringFrameIndex
		});
}