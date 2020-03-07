#include "ITexture.h"
#include "../LogicComponent/World.h"
ITexture::ITexture() : MObject() {
	srvDescID = World::GetInstance()->GetDescHeapIndexFromPool();
}
ITexture::~ITexture()
{
	World* wd = World::GetInstance();
	if (wd)
	{
		wd->ReturnDescHeapIndexToPool(srvDescID);
	}
}