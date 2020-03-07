#include "PBRMaterial.h"
#include "../ResourceManagement/AssetDatabase.h"
#include "../ResourceManagement/AssetReference.h"
#include "../Singleton/FrameResource.h"
#include "UploadBuffer.h"
#include "ITexture.h"
#include "../CJsonObject/CJsonObject.hpp"
#include "../Singleton/ShaderID.h"
uint PBRMaterial::albedoTexIndex = 0;
uint PBRMaterial::smoTexIndex = 0;
uint PBRMaterial::normalTexIndex = 0;
uint PBRMaterial::emissionTexIndex = 0;
bool PBRMaterial::initialized = false;
void PBRMaterial::Initialize()
{
	if (initialized) return;
	initialized = true;

	albedoTexIndex = ShaderID::GetMainTex();
	smoTexIndex = ShaderID::PropertyToID("_SMOTexture");
	normalTexIndex = ShaderID::PropertyToID("_BumpMap");
	emissionTexIndex = ShaderID::PropertyToID("_EmissionTex");
}
using namespace neb;
void PBRMaterial::UpdateMaterialToBuffer()
{
	manager->ubuffer->CopyData(matIndex, &matData);
}
void PBRMaterial::SetAlbedoTexture(const ObjectPtr<ITexture>& tex)
{
	albedoTex = tex;
	matData.albedoTexIndex = tex ? tex->GetGlobalDescIndex() : -1;
}
void PBRMaterial::SetSMOTexture(const ObjectPtr<ITexture>& tex)
{
	smoTex = tex;
	matData.specularTexIndex = tex ? tex->GetGlobalDescIndex() : -1;
}
void PBRMaterial::SetNormalTexture(const ObjectPtr<ITexture>& tex)
{
	normalTex = tex;
	matData.normalTexIndex = tex ? tex->GetGlobalDescIndex() : -1;
}
void PBRMaterial::SetEmissionTexture(const ObjectPtr<ITexture>& tex)
{
	emissionTex = tex;
	matData.emissionTexIndex = tex ? tex->GetGlobalDescIndex() : -1;
}

PBRMaterialManager::PBRMaterialManager(ID3D12Device* device, uint initSize) :
	ubuffer(std::unique_ptr<UploadBuffer>(new UploadBuffer(device, initSize, false, sizeof(StandardPBRMaterial)))),
	capacity(max<uint>(initSize, 3))
{
	removedIndex.reserve(initSize);
}

PBRMaterial::PBRMaterial(ID3D12Device* device, PBRMaterialManager* manager)
{
	Initialize();
	matIndex = manager->AddNewMaterial(device, this, &matData);
	UpdateMaterialToBuffer();
}

PBRMaterial::PBRMaterial(ID3D12Device* device, PBRMaterialManager* manager, const CJsonObject& jsonObj, bool preloadResource) :
	manager(manager)
{
	std::string s;
	float f = 0;
	if (jsonObj.Get("uvScale", s))
	{
		ReadStringToVector<float2, 2>(s.data(), s.length(), &matData.uvScale);
	}
	if (jsonObj.Get("uvOffset", s))
	{
		ReadStringToVector<float2, 2>(s.data(), s.length(), &matData.uvOffset);
	}
	if (jsonObj.Get("albedo", s))
	{
		ReadStringToVector<float3, 3>(s.data(), s.length(), &matData.albedo);
	}
	if (jsonObj.Get("emission", s))
	{
		ReadStringToVector<float3, 3>(s.data(), s.length(), &matData.emission);
	}
	if (jsonObj.Get("metallic", f))
	{
		matData.metallic = f;
	}
	if (jsonObj.Get("smoothness", f))
	{
		matData.smoothness = f;
	}
	if (jsonObj.Get("occlusion", f))
	{
		matData.occlusion = f;
	}
	ObjectPtr<MObject> (*func)(ID3D12Device*, const std::string&, AssetLoadType);
	if (preloadResource)
	{
		func = [](ID3D12Device* device, const std::string& s, AssetLoadType loadType)->ObjectPtr<MObject>
		{
			return AssetReference::SyncLoad(device, s, AssetLoadType::Texture2D);
		};
	}
	else
	{
		func = [](ID3D12Device*, const std::string& s, AssetLoadType)->ObjectPtr<MObject>
		{
			return AssetDatabase::GetInstance()->GetLoadedObject(s);
		};
	}
	ObjectPtr<MObject> obj = func(device, s, AssetLoadType::Texture2D);
	if (jsonObj.Get("albedoTexIndex", s))
	{
		SetAlbedoTexture(obj.CastTo<ITexture>());
	}
	if (jsonObj.Get("specularTexIndex", s))
	{
		SetSMOTexture(obj.CastTo<ITexture>());
	}
	if (jsonObj.Get("normalTexIndex", s))
	{
		SetNormalTexture(obj.CastTo<ITexture>());
	}
	if (jsonObj.Get("emissionTexIndex", s))
	{
		SetEmissionTexture(obj.CastTo<ITexture>());
	}


	matIndex = manager->AddNewMaterial(device, this, &matData);
	UpdateMaterialToBuffer();
}

PBRMaterial::~PBRMaterial()
{
	if (manager)
		manager->RemoveMaterial(this);
}

void PBRMaterialManager::RemoveMaterial(PBRMaterial* mat)
{
	removedIndex.push_back(mat->matIndex);
}


uint PBRMaterialManager::AddNewMaterial(ID3D12Device* device, PBRMaterial* mat, StandardPBRMaterial* ptr)
{
	if (removedIndex.empty())
	{
		if (size >= capacity)
		{
			uint newCapacity = capacity * 1.5;
			UploadBuffer* newUBuffer = new UploadBuffer(device, newCapacity, false, sizeof(StandardPBRMaterial));
			newUBuffer->CopyFrom(ubuffer.get(), 0, 0, capacity);
			ubuffer = std::unique_ptr<UploadBuffer>(newUBuffer);
			capacity = newCapacity;
		}
		uint index = size++;
		ubuffer->CopyData(index, ptr);
		return index;
	}
	else
	{
		auto lastIte = removedIndex.end() - 1;
		uint index = *lastIte;
		removedIndex.erase(lastIte);
		ubuffer->CopyData(index, ptr);
		return index;
	}
}

void PBRMaterial::SetTexture(uint propertyID, const ObjectPtr<ITexture>& tex)
{
	if (propertyID == albedoTexIndex)
		SetAlbedoTexture(tex);
	else if (propertyID == normalTexIndex)
		SetNormalTexture(tex);
	else if (propertyID == emissionTexIndex)
		SetEmissionTexture(tex);
	else if (propertyID == smoTexIndex)
		SetSMOTexture(tex);
}

ITexture* PBRMaterial::GetTexture(uint propertyID)
{
	if (propertyID == albedoTexIndex)
		return albedoTex;
	else if (propertyID == normalTexIndex)
		return normalTex;
	else if (propertyID == emissionTexIndex)
		return emissionTex;
	else if (propertyID == smoTexIndex)
		return smoTex;
	return nullptr;
}