#pragma once
#include "IMaterial.h"
class ITexture;
class UploadBuffer;
class AssetReference;
class FrameResource;
namespace neb
{
	class CJsonObject;
}
struct StandardPBRMaterial
{
	float2 uvScale = { 1,1 };
	float2 uvOffset = { 0,0 };
	//align
	float3 albedo = { 1,1,1 };
	float metallic = 0.5f;
	float3 emission = { 0,0,0 };
	float smoothness = 0.5f;
	//align
	float occlusion = 1;
	int albedoTexIndex = -1;
	int specularTexIndex = -1;
	int normalTexIndex = -1;
	//align
	int emissionTexIndex = -1;
	float3 __align;
};
class PBRMaterial;
class PBRMaterialManager
{
	friend class PBRMaterial;
private:
	std::unique_ptr<UploadBuffer> ubuffer;
	std::vector<uint> removedIndex;
	uint capacity;
	uint size = 0;
	uint AddNewMaterial(ID3D12Device* device, PBRMaterial* mat, StandardPBRMaterial* ptr);
	void RemoveMaterial(PBRMaterial* mat);
public:
	PBRMaterialManager(ID3D12Device* device, uint initSize);
	UploadBuffer* GetMaterialBuffer() const noexcept
	{
		return ubuffer.get();
	}
};
class PBRMaterial : public IMaterial
{
	friend class PBRMaterialManager;
	friend class AssetReference;
private:
	StandardPBRMaterial matData;
	uint matIndex;
	PBRMaterialManager* manager;
	ObjectPtr<ITexture> albedoTex;
	ObjectPtr<ITexture> smoTex;
	ObjectPtr<ITexture> normalTex;
	ObjectPtr<ITexture> emissionTex;
	static uint albedoTexIndex;
	static uint smoTexIndex;
	static uint normalTexIndex;
	static uint emissionTexIndex;
	static bool initialized;
	static void Initialize();
public:
	PBRMaterial(ID3D12Device* device, PBRMaterialManager* manager, const neb::CJsonObject& path, bool preloadResource = false);
	StandardPBRMaterial* GetDataPtr()
	{
		return &matData;
	}
	uint GetMaterialIndex() const { return matIndex; }
	void UpdateMaterialToBuffer();
	void SetAlbedoTexture(const ObjectPtr<ITexture>& tex);
	void SetSMOTexture(const ObjectPtr<ITexture>& tex);
	void SetNormalTexture(const ObjectPtr<ITexture>& tex);
	void SetEmissionTexture(const ObjectPtr<ITexture>& tex);
	
	PBRMaterial(ID3D12Device* device, PBRMaterialManager* manager);
	~PBRMaterial();
	virtual void SetTexture(uint propertyID, const ObjectPtr<ITexture>& tex);
	virtual ITexture* GetTexture(uint propertyID);
};