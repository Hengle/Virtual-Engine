#pragma once
#include "../Common/d3dUtil.h"
#include "Shader.h"
#include "ComputeShader.h"
class ShaderIO
{
public:
	static void DecodeShader(
		const std::string& fileName,
		std::vector<ShaderVariable>& vars,
		std::vector<Pass>& passes);
	static void DecodeComputeShader(
		const std::string& fileName,
		std::vector<ComputeShaderVariable>& vars,
		std::vector<Microsoft::WRL::ComPtr<ID3DBlob>>& datas);
};