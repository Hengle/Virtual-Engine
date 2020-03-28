#pragma once
#include "../Common/d3dUtil.h"
#include "../Common/MObject.h"
namespace neb
{
	class CJsonObject;
}
class Transform;
class AssetReference;
class Scene : public MObject
{
	friend class AssetReference;
private:
	std::vector<ObjectPtr<Transform>> loadedTransforms;
	std::string guid;
protected:
	Scene(const std::string& guid, neb::CJsonObject& jsonObj, ID3D12Device* device);
	virtual ~Scene();
public:
	void DestroyScene();
	
};