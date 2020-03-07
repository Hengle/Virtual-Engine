#pragma once
#include "../Common/d3dUtil.h"

class ShaderVariable;
class Pass;
class ComputeShaderVariable;
void DecodeShader(
	const std::string& fileName,
	std::vector<ShaderVariable>& vars,
	std::vector<Pass>& passes);
void DecodeComputeShader(
	const std::string& fileName,
	std::vector<ComputeShaderVariable>& vars,
	std::vector<Microsoft::WRL::ComPtr<ID3DBlob>>& datas);