#include "TerrainDrawer.h"
#include "../../Singleton/ShaderID.h"
#include "../../Singleton/PSOContainer.h"
#include "../Shader.h"
#include "../../PipelineComponent/IPipelineResource.h"
#include "../../Singleton/FrameResource.h"
#include "../TransitionBarrierBuffer.h"
#include "../UploadBuffer.h"
#include "../RenderCommand.h"
#include "../../Common/GeometryGenerator.h"
#include "VirtualTexture.h"
TerrainDrawer::PropID TerrainDrawer::propID;
void Terrain_BuildShapeGeometry(GeometryGenerator::MeshData& box, StackObject<Mesh>& bMesh, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, FrameResource* res);
void TerrainDrawer::PropID::Initialize()
{
	if (initialized) return;
	initialized = true;
	_ObjectDataBuffer = ShaderID::PropertyToID("_ObjectDataBuffer");
}
class TerrainDrawer_Initializer : public RenderCommand
{
	UploadBuffer ubuffer;
	StructuredBuffer* sbuffer;
	StackObject<Mesh>* mesh;
	float2 size;
public:
	TerrainDrawer_Initializer(ID3D12Device* device, uint ele, uint stride, StructuredBuffer* sbuffer, void* data, StackObject<Mesh>* mesh, float2 size) :
		ubuffer(device, ele, false, stride),
		sbuffer(sbuffer),
		mesh(mesh),
		size(size)
	{
		ubuffer.CopyDatas(0, ele, data);
	}
	virtual void operator()(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		FrameResource* resource,
		TransitionBarrierBuffer* barrierBuffer)
	{
		GeometryGenerator geo;
		Terrain_BuildShapeGeometry(
			geo.CreatePlane(size.x, size.y),
			*mesh,
			device,
			commandList,
			resource
		);
		ubuffer.ReleaseAfterFlush(resource);
		barrierBuffer->AddCommand(D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST, sbuffer->GetResource());
		barrierBuffer->ExecuteCommand(commandList);
		commandList->CopyBufferRegion(sbuffer->GetResource(), 0, ubuffer.Resource(), 0, ubuffer.GetElementCount() * ubuffer.GetStride());
		barrierBuffer->AddCommand(D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, sbuffer->GetResource());
	}
};
TerrainDrawer::TerrainDrawer(
	ID3D12Device* device,
	uint2 drawCount,
	double2 perObjectSize) : drawCount(drawCount)
{
	propID.Initialize();
	objectBuffer.New(device, &StructuredBufferElement::Get(sizeof(ObjectData), drawCount.x * drawCount.y), 1, false, true);
	std::vector<ObjectData> placedObjectData(drawCount.x * drawCount.y);
	double2 fullSize = perObjectSize * double2(drawCount.x, drawCount.y);
	double2 start = fullSize * -0.5;
	for (uint y = 0, index = 0; y < drawCount.y; ++y)
		for (uint x = 0; x < drawCount.x; ++x)
		{
			double2 worldPos = start + double2(x + 0.5, y + 0.5) * perObjectSize;
			placedObjectData[index] =
			{
				float2(worldPos.x, worldPos.y),//worldPosition
				float2(perObjectSize.x, perObjectSize.y),//worldScale
				uint2(x, y)
			};
			index++;
		}
	TerrainDrawer_Initializer* initializer = new TerrainDrawer_Initializer(
		device,
		drawCount.x * drawCount.y,
		sizeof(ObjectData),
		objectBuffer,
		placedObjectData.data(),
		&planeMesh,
		float2(perObjectSize.x * 0.5f, perObjectSize.y * 0.5f));
	RenderCommand::AddCommand(
		initializer
	);
}

TerrainDrawer::~TerrainDrawer()
{
	planeMesh.Delete();
	objectBuffer.Delete();
}

void TerrainDrawer::Draw(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	Shader* shader,
	uint pass,
	PSOContainer* psoContainer,
	uint containerIndex,
	ConstBufferElement cameraBuffer,
	FrameResource* frameResource,
	VirtualTexture* vt)
{
	PSODescriptor desc;
	desc.meshLayoutIndex = planeMesh->GetLayoutIndex();
	desc.shaderPass = pass;
	desc.shaderPtr = shader;
	ID3D12PipelineState* pso = psoContainer->GetState(desc, device, 0);
	if(vt) vt->SetTextureProperty(shader, commandList);
	commandList->SetPipelineState(pso);
	shader->SetResource(commandList, ShaderID::GetPerCameraBufferID(), cameraBuffer.buffer, cameraBuffer.element);
	shader->SetStructuredBufferByAddress(commandList, propID._ObjectDataBuffer, objectBuffer->GetAddress(0, 0));
	commandList->IASetVertexBuffers(0, 1, &planeMesh->VertexBufferView());
	commandList->IASetIndexBuffer(&planeMesh->IndexBufferView());
	commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawIndexedInstanced(planeMesh->GetIndexCount(), objectBuffer->GetElementCount(0), 0, 0, 0);
}


void Terrain_BuildShapeGeometry(GeometryGenerator::MeshData& box, StackObject<Mesh>& bMesh, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, FrameResource* res)
{
	std::vector<XMFLOAT3> positions(box.Vertices.size());
	std::vector<XMFLOAT3> normals(box.Vertices.size());
	std::vector<XMFLOAT2> uvs(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		positions[i] = box.Vertices[i].Position;
		normals[i] = box.Vertices[i].Normal;
		uvs[i] = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();
	bMesh.New(
		box.Vertices.size(),
		positions.data(),
		normals.data(),
		nullptr,
		nullptr,
		uvs.data(),
		nullptr,
		nullptr,
		nullptr,
		device,
		DXGI_FORMAT_R16_UINT,
		indices.size(),
		indices.data()
	);

}