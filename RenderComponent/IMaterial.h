#pragma once
#include "../Common/MObject.h"
#include "../Common/d3dUtil.h"
class ITexture;
class IMaterial : public MObject
{
public:
	virtual void SetTexture(uint propertyID, const ObjectPtr<ITexture>& tex) = 0;
	virtual ITexture* GetTexture(uint propertyID) = 0;
};