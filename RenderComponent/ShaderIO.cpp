#include "ShaderIO.h"
#include <fstream>


template <typename T>
void DragData(std::ifstream& ifs, T& data)
{
	ifs.read((char*)&data, sizeof(T));
}
template <>
void DragData<std::string>(std::ifstream& ifs, std::string& str)
{
	uint32_t length = 0;
	DragData<uint32_t>(ifs, length);
	str.clear();
	str.resize(length);
	ifs.read(str.data(), length);
}
void ShaderIO::DecodeShader(
	const std::string& fileName,
	std::vector<ShaderVariable>& vars,
	std::vector<Pass>& passes)
{
	vars.clear();
	passes.clear();
	uint varSize = 0;
	std::ifstream ifs(fileName, std::ios::binary);
	if (!ifs) return;
	DragData<uint>(ifs, varSize);
	vars.resize(varSize);
	for (auto i = vars.begin(); i != vars.end(); ++i)
	{
		DragData<std::string>(ifs, i->name);
		DragData<ShaderVariableType>(ifs, i->type);
		DragData<uint>(ifs, i->tableSize);
		DragData<uint>(ifs, i->registerPos);
		DragData<uint>(ifs, i->space);
	}
	uint functionCount = 0;
	DragData<uint>(ifs, functionCount);
	std::vector<Microsoft::WRL::ComPtr<ID3DBlob>> functions(functionCount);
	for (uint i = 0; i < functionCount; ++i)
	{
		uint64_t codeSize = 0;
		DragData(ifs, codeSize);
		if(codeSize > 0)
		{
			D3DCreateBlob(codeSize, &functions[i]);
			ifs.read((char*)functions[i]->GetBufferPointer(), codeSize);
		}
	}
	uint passSize = 0;
	DragData<uint>(ifs, passSize);
	passes.resize(passSize);
	for (auto i = passes.begin(); i != passes.end(); ++i)
	{
		DragData(ifs, i->rasterizeState);
		DragData(ifs, i->depthStencilState);
		DragData(ifs, i->blendState);
		uint vertIndex = 0, fragIndex = 0;
		DragData(ifs, vertIndex);
		DragData(ifs, fragIndex);
		i->vsShader = functions[vertIndex];
		i->psShader = functions[fragIndex];
	}
}

void ShaderIO::DecodeComputeShader(
	const std::string& fileName,
	std::vector<ComputeShaderVariable>& vars,
	std::vector<Microsoft::WRL::ComPtr<ID3DBlob>>& datas)
{
	vars.clear();
	datas.clear();
	uint varSize = 0;
	std::ifstream ifs(fileName, std::ios::binary);
	if (!ifs) return;
	DragData<uint>(ifs, varSize);
	vars.resize(varSize);
	for (auto i = vars.begin(); i != vars.end(); ++i)
	{
		DragData<std::string>(ifs, i->name);
		DragData<ComputeShaderVariable::Type>(ifs, i->type);
		DragData<uint>(ifs, i->tableSize);
		DragData<uint>(ifs, i->registerPos);
		DragData<uint>(ifs, i->space);
	}
	uint kernelSize = 0;
	DragData<uint>(ifs, kernelSize);
	datas.resize(kernelSize);
	for (auto i = datas.begin(); i != datas.end(); ++i)
	{
		uint64_t kernelSize = 0;
		DragData<uint64_t>(ifs, kernelSize);
		D3DCreateBlob(kernelSize, &*i);
		ifs.read((char*)(*i)->GetBufferPointer(), kernelSize);
	}
}
