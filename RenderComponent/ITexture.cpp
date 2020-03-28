#include "ITexture.h"
#include "../LogicComponent/World.h"
#include "../Singleton/FrameResource.h"
ITexture::ITexture() : MObject() {
	srvDescID = World::GetInstance()->GetDescHeapIndexFromPool();
}
ITexture::~ITexture()
{
	ReleaseAfterFrame();
	World* wd = World::GetInstance();
	if (wd)
	{
		wd->ReturnDescHeapIndexToPool(srvDescID);
	}
}

void ITexture::ReleaseAfterFrame()
{
	for (auto ite = FrameResource::mFrameResources.begin(); ite != FrameResource::mFrameResources.end(); ++ite)
	{
		if (*ite) FrameResource::ReleaseResourceAfterFlush(Resource, ite->get());
	}
}