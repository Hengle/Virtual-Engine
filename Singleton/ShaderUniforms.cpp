#include "ShaderUniforms.h"
#include <fstream>
#include "../Common/StringUtility.h"
using namespace std;
using namespace StringUtil;
namespace SCompile
{
	D3D12_BLEND_DESC GetBlendState(bool alphaBlend)
	{
		if (alphaBlend) {
			D3D12_BLEND_DESC blendDesc;
			blendDesc.IndependentBlendEnable = FALSE;
			blendDesc.AlphaToCoverageEnable = FALSE;


			D3D12_RENDER_TARGET_BLEND_DESC desc;
			desc.BlendEnable = TRUE;
			desc.LogicOpEnable = FALSE;
			desc.BlendOp = D3D12_BLEND_OP_ADD;
			desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.SrcBlendAlpha = D3D12_BLEND_ZERO;
			desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			desc.DestBlendAlpha = D3D12_BLEND_ONE;
			desc.LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;

			for (uint i = 0; i < 8; ++i)
			{
				blendDesc.RenderTarget[i] = desc;
			}
			return blendDesc;
		}
		else
		{
			return CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		}
	}

	D3D12_DEPTH_STENCIL_DESC GetDepthState(bool zwrite, D3D12_COMPARISON_FUNC compareFunc)
	{
		D3D12_DEPTH_STENCIL_DESC dsDesc;
		if (!zwrite && compareFunc == D3D12_COMPARISON_FUNC_ALWAYS)
		{
			dsDesc.DepthEnable = FALSE;
			dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		else
		{
			dsDesc.DepthEnable = TRUE;
			dsDesc.DepthWriteMask = zwrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		}

		dsDesc.DepthFunc = compareFunc;
		dsDesc.StencilEnable = FALSE;
		dsDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		dsDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		dsDesc.FrontFace = defaultStencilOp;
		dsDesc.BackFace = defaultStencilOp;
		return dsDesc;
	}

	D3D12_RASTERIZER_DESC GetCullState(D3D12_CULL_MODE cullMode, bool enableConservativeRaster = false)
	{
		D3D12_RASTERIZER_DESC desc;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = cullMode;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = TRUE;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = enableConservativeRaster ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		return desc;
	}

	void GetVarData(const string& s, bool useCount, char registerType, uint& count, uint& regis, uint& space, string& name)
	{
		string data;
		space = 0;
		regis = 0;
		count = 1;
		if (useCount)
		{
			GetDataFromAttribute(s, data);
			if (!data.empty())
				count = StringToInteger(data);
		}
		GetDataFromBrackets(s, data);
		int tIndices = GetFirstIndexOf(data, registerType);
		string regisStr;
		regisStr.reserve(3);
		if (tIndices >= 0)
		{
			for (int i = tIndices + 1; i < data.length() && data[i] != ',' && data[i] != ' '; ++i)
			{
				regisStr.push_back(data[i]);
			}
			if (!regisStr.empty())
				regis = StringToInteger(regisStr);
			else regis = 0;
		}
		regisStr.clear();
		tIndices = GetFirstIndexOf(data, "space");
		if (tIndices >= 0)
		{
			for (int i = tIndices + 5; i < data.length() && data[i] != ',' && data[i] != ' '; ++i)
			{
				regisStr.push_back(data[i]);
			}
			if (!regisStr.empty())
				space = StringToInteger(regisStr);
		}
		tIndices = GetFirstIndexOf(s, ' ');
		name.clear();
		if (tIndices >= 0)
		{
			name.reserve(10);
			for (uint i = tIndices + 1; i < s.length(); ++i)
			{
				if (s[i] == ' ' || s[i] == '[') break;
				name.push_back(s[i]);
			}
		}
	}

	void GetShaderRootSigData(const string& path, vector<ShaderVariable>& vars, vector<PassDescriptor>& passes)
	{
		vars.clear();
		vars.reserve(20);
		passes.clear();
		passes.reserve(20);
		ifstream ifs(path);
		if (!ifs) return;
		char c[256];
		string s;
		static string rwtex = "RWT";
		static string rwstr = "RWS";
		static string cbuffer = "cbuffer";
		static string tex = "Tex";
		static string str = "Str";
		static string pragma = "#pragma";
		static string endPragma = "#end";
		static string vertex = "vertex";
		static string fragment = "fragment";
		static string Cull = "cull";
		static string zWrite = "zwrite";
		static string zTest = "ztest";
		static string conservative = "conservative";
		static string blend = "blend";
		std::vector<string> commands;
		s.reserve(256);


		while (true)
		{

			ifs.getline(c, 255);
			if (strlen(c) == 0) return;
			s = c;
			ShaderVariable sv;
			sv.tableSize = 1;
			if (GetFirstIndexOf(s, rwtex) == 0)	//RWTexture
			{
				GetVarData(s, true, 'u', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ShaderVariableType_UAVDescriptorHeap;
				vars.push_back(sv);
			}
			/*	else if (GetFirstIndexOf(s, rwstr) == 0)//RWStructured
				{
					GetVarData(s, false, 'u', sv.tableSize, sv.registerPos, sv.space, sv.name);
					sv.type = ComputeShaderVariable::RWStructuredBuffer;
				}*/ //VS Shader do not support rwstructured
			else if (GetFirstIndexOf(s, cbuffer) == 0) //cbuffer
			{
				GetVarData(s, false, 'b', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ShaderVariableType_ConstantBuffer;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, tex) == 0) // texture
			{
				GetVarData(s, true, 't', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ShaderVariableType_SRVDescriptorHeap;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, str) == 0)//structured
			{
				GetVarData(s, false, 't', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ShaderVariableType_StructuredBuffer;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, pragma) == 0)
			{
				int start = GetFirstIndexOf(s, ' ');
				if (start >= 0)
				{
					PassDescriptor p;
					p.name.reserve(20);
					for (uint i = start + 1; i < s.length(); ++i)
						p.name += s[i];
					bool alpha = false;
					bool zwrite = true;
					auto ztest = D3D12_COMPARISON_FUNC_LESS_EQUAL;
					auto cullmode = D3D12_CULL_MODE_BACK;
					bool conservativeMode = false;
					while (true)
					{
						ifs.getline(c, 255);
						if (strlen(c) == 0) break;
						s = c;
						if (GetFirstIndexOf(s, endPragma) == 0)
						{
							p.blendState = GetBlendState(alpha);
							p.depthStencilState = GetDepthState(zwrite, ztest);
							p.rasterizeState = GetCullState(cullmode, conservativeMode);
							passes.push_back(p);
							break;
						}
						Split(s, ' ', commands);
						if (commands.size() >= 2)
						{
							if (commands[0] == zTest)
							{
								if (commands[1] == "less")
								{
									ztest = D3D12_COMPARISON_FUNC_LESS;
								}
								else if (commands[1] == "lequal")
								{
									ztest = D3D12_COMPARISON_FUNC_LESS_EQUAL;
								}
								else if (commands[1] == "greater")
								{
									ztest = D3D12_COMPARISON_FUNC_GREATER;
								}
								else if (commands[1] == "gequal")
								{
									ztest = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
								}
								else if (commands[1] == "equal")
								{
									ztest = D3D12_COMPARISON_FUNC_EQUAL;
								}
								else if (commands[1] == "nequal")
								{
									ztest = D3D12_COMPARISON_FUNC_NOT_EQUAL;
								}
								else if (commands[1] == "always")
								{
									ztest = D3D12_COMPARISON_FUNC_ALWAYS;
								}
								else if (commands[1] == "never")
								{
									ztest = D3D12_COMPARISON_FUNC_NEVER;
								}
							}
							else if (commands[0] == zWrite)
							{
								if (commands[1] == "on")
								{
									zwrite = true;
								}
								else if (commands[1] == "off")
								{
									zwrite = false;
								}
							}
							else if (commands[0] == Cull)
							{
								if (commands[1] == "back")
								{
									cullmode = D3D12_CULL_MODE_BACK;
								}
								else if (commands[1] == "front")
								{
									cullmode = D3D12_CULL_MODE_FRONT;
								}
								else if (commands[1] == "off")
								{
									cullmode = D3D12_CULL_MODE_NONE;
								}
							}
							else if (commands[0] == vertex)
							{
								p.vertex = commands[1];
							}
							else if (commands[0] == fragment)
							{
								p.fragment = commands[1];
							}
							else if (commands[0] == conservative)
							{
								if (commands[1] == "on")
									conservativeMode = true;
								else if (commands[1] == "off")
									conservativeMode = false;
							}
							else if (commands[0] == blend)
							{
								if (commands[1] == "on")
									alpha = true;
								else alpha = false;
							}
						}
					}
				}
			}
		}

	}
	void GetComputeShaderRootSigData(const string& path, vector<ComputeShaderVariable>& vars, vector<string>& passes)
	{
		vars.clear();
		vars.reserve(20);
		passes.clear();
		passes.reserve(20);
		ifstream ifs(path);
		if (!ifs) return;
		char c[256];
		string s;
		static string rwtex = "RWT";
		static string rwstr = "RWS";
		static string cbuffer = "cbuffer";
		static string tex = "Tex";
		static string str = "Str";
		static string pragma = "#pragma";
		string kernelName;
		//string endpragma = "#end";
		s.reserve(256);


		while (true)
		{

			ifs.getline(c, 255);
			if (strlen(c) == 0) return;
			s = c;
			ComputeShaderVariable sv;
			sv.tableSize = 1;
			if (GetFirstIndexOf(s, rwtex) == 0)	//RWTexture
			{
				GetVarData(s, true, 'u', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ComputeShaderVariable::UAVDescriptorHeap;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, rwstr) == 0)//RWStructured
			{
				GetVarData(s, false, 'u', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ComputeShaderVariable::RWStructuredBuffer;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, cbuffer) == 0) //cbuffer
			{
				GetVarData(s, false, 'b', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ComputeShaderVariable::ConstantBuffer;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, tex) == 0) // texture
			{
				GetVarData(s, true, 't', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ComputeShaderVariable::SRVDescriptorHeap;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, str) == 0)//structured
			{
				GetVarData(s, false, 't', sv.tableSize, sv.registerPos, sv.space, sv.name);
				sv.type = ComputeShaderVariable::StructuredBuffer;
				vars.push_back(sv);
			}
			else if (GetFirstIndexOf(s, pragma) == 0)//Pre
			{
				int start = GetFirstIndexOf(s, ' ');
				if (start >= 0)
				{
					kernelName.clear();
					kernelName.reserve(20);
					for (uint i = start + 1; i < s.length(); ++i)
					{
						kernelName += s[i];
					}
				}
				passes.push_back(kernelName);
			}
		}
	}
}