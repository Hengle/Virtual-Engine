#pragma once
#include "Component.h"
class Transform;
class RenderTexture;
class DirectionalLight final : public Component
{
public:
	static const uint CascadeLevel = 4;
private:
	static ObjectPtr<DirectionalLight> current;
	uint shadowmapResolution[CascadeLevel];
	ObjectPtr<RenderTexture> shadowmaps[CascadeLevel];
	DirectionalLight(
		const ObjectPtr<Transform>& trans, ObjectPtr<Component>& ptr);

	~DirectionalLight();
public:
	static void DestroyLight()
	{
		if (current)
		{
			delete current;
		}
		current = nullptr;
	}
	static DirectionalLight* GetInstance(
		const ObjectPtr<Transform>& trans,
		uint* shadowmapResolution,
		ID3D12Device* device);
	static DirectionalLight* GetInstance()
	{
		return current;
	}
	float intensity = 1;
	float3 color = { 1,1,1 };
	float shadowDistance[CascadeLevel] = { 3,5,30,80 };
	float shadowSoftValue[CascadeLevel] = { 2.0f,1.3f,1.0f,0.5f };
	float shadowBias[CascadeLevel] = { 0.05f,0.1f, 0.15f,0.3f };
	constexpr uint GetShadowmapResolution(uint level) const
	{
#ifndef NDEBUG
		if (level >= CascadeLevel) throw "Out of Range Exception";
#endif
		return shadowmapResolution[level];
	}
	ObjectPtr<RenderTexture>& GetShadowmap(uint level)
	{
#ifndef NDEBUG
		if (level >= CascadeLevel) throw "Out of Range Exception";
#endif
		return shadowmaps[level];
	}

};