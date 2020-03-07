#include "ShaderCompiler.h"
#include "../JobSystem/JobInclude.h"
#include "ShaderUniforms.h"
#include "../Common/StringUtility.h"
using namespace SCompile;
#define StructuredBuffer SCompile::StructuredBuffer
int ShaderCompiler::shaderIDCount = 0;

std::unordered_map<Shader*, int> ShaderCompiler::gbufferShaderIDs;
std::unordered_map<std::string, Shader*> ShaderCompiler::mShaders;
std::unordered_map<std::string, ComputeShader*> ShaderCompiler::mComputeShaders;

void LoadShader(const std::string& name, ID3D12Device* device, JobBucket* bucket, const std::string& path)
{
	Shader* sh = (Shader*)malloc(sizeof(Shader));
	if (bucket)
	{
		bucket->GetTask([=]()->void
			{
				new (sh)Shader(device, path);
			}, nullptr, 0);
	}
	else
		new (sh)Shader(device, path);
	ShaderCompiler::AddShader(name, sh);
}

void LoadComputeShader(const std::string& name, ID3D12Device* device, JobBucket* bucket, const std::string& path)
{
	ComputeShader* sh = (ComputeShader*)malloc(sizeof(ComputeShader));
	if (bucket)
	{
		bucket->GetTask([=]()->void
			{
				new (sh)ComputeShader(path, device);
			}, nullptr, 0);
	}
	else
		new (sh)ComputeShader(path, device);
	ShaderCompiler::AddComputeShader(name, sh);
}
void ShaderCompiler::AddShader(std::string str, Shader* shad)
{
	mShaders[str] = shad;
}

void ShaderCompiler::AddComputeShader(std::string str, ComputeShader* shad)
{
	mComputeShaders[str] = shad;
}

Shader* ShaderCompiler::GetShader(std::string name)
{
	return mShaders[name];
}

ComputeShader* ShaderCompiler::GetComputeShader(std::string name)
{
	return mComputeShaders[name];
}

D3D12_RASTERIZER_DESC GetRasterize(D3D12_CULL_MODE cullMode, bool enableConservativeRaster = false)
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

D3D12_BLEND_DESC GetAlphaBlend()
{
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

void GetTonemapLut3DShader(ID3D12Device* device, JobBucket* bucket)
{
	const UINT KERNEL_COUNT = 1;
	std::string kernelNames[KERNEL_COUNT] =
	{
		"KGenLut3D_AcesTonemap"
	};
	const uint SHADER_VAR_COUNT = 3;
	ComputeShaderVariable vars[SHADER_VAR_COUNT] =
	{
		RWTexture3D::GetComputeVar(1, 0, 0, "_MainTex"),
		Texture2D::GetComputeVar(1, 0, 0, "_Curves"),
		ConstantBuffer::GetComputeVar(0, 0, "Params")
	};
	ComputeShader* cs = new ComputeShader("Shaders\\Lut3DBaker.compute", kernelNames, KERNEL_COUNT, vars, SHADER_VAR_COUNT, device, bucket);
	ShaderCompiler::AddComputeShader("Lut3DBaker", cs);
}

void GetHIZMipGeneratorShader(ID3D12Device* device, JobBucket* bucket)
{
	const uint KERNEL_COUNT = 3;
	std::string kernelNames[KERNEL_COUNT] =
	{
		"GenerateMip",
		"GenerateMip1",
		"UpdateDepth"
	};
	const uint SHADER_VAR_COUNT = 2;
	ComputeShaderVariable vars[SHADER_VAR_COUNT] =
	{
		RWTexture2D::GetComputeVar(9, 0, 0, "_DepthTexture"),
		Texture2D::GetComputeVar(1, 0, 0, "_CameraDepthTexture")
	};

	ComputeShader* cs = new ComputeShader("Shaders/HizGenerator.compute", kernelNames, KERNEL_COUNT, vars, SHADER_VAR_COUNT, device, bucket);
	ShaderCompiler::AddComputeShader("HIZGenerator", cs);
}

void GetMipGeneratorShader(ID3D12Device* device, JobBucket* bucket)
{
	const uint KERNEL_COUNT = 1;
	std::string kernelNames[KERNEL_COUNT] =
	{
		"GenerateMip"
	};
	const uint SHADER_VAR_COUNT = 1;
	ComputeShaderVariable vars[SHADER_VAR_COUNT] =
	{
		RWTexture2D::GetComputeVar(9, 0, 0, "_DepthTexture"),
	};
	ComputeShader* cs = new ComputeShader("Shaders/MipGenerator.compute", kernelNames, KERNEL_COUNT, vars, SHADER_VAR_COUNT, device, bucket);
	ShaderCompiler::AddComputeShader("MipGenerator", cs);
}

void GetAutoExposure(ID3D12Device* device, JobBucket* bucket)
{
	const uint KERNEL_COUNT = 1;
	std::string kernelNames[KERNEL_COUNT] =
	{
		"MAIN"
	};
	const uint SHADER_VAR_COUNT = 4;
	ComputeShaderVariable vars[SHADER_VAR_COUNT] =
	{
		StructuredBuffer::GetComputeVar(0, 0, "_HistogramBuffer"),
		Texture2D::GetComputeVar(1,1,0, "_Source"),
		RWTexture2D::GetComputeVar(2,0,0,"_Destination"),
		ConstantBuffer::GetComputeVar(0, 0, "Params")
	};
	ComputeShader* cs = new ComputeShader("Shaders/AutoExposure.compute", kernelNames, KERNEL_COUNT, vars, SHADER_VAR_COUNT, device, bucket);
	ShaderCompiler::AddComputeShader("AutoExposure", cs);
}

void CompileShaders(ID3D12Device* device, JobBucket* bucket, const std::string& path)
{
	std::ifstream ifs(path);
	if (!ifs) return;
	std::vector<std::string> commandLines;
	StringUtil::ReadLines(ifs, commandLines);
	std::vector<std::string> separateCommands;
	for (auto i = commandLines.begin(); i != commandLines.end(); ++i)
	{
		StringUtil::Split(*i, ' ', separateCommands);
		if (separateCommands.size() != 3) continue;
		if (separateCommands[0] == "compute")
		{
			LoadComputeShader(separateCommands[1], device, bucket, separateCommands[2]);
		}
		else if (separateCommands[0] == "shader")
		{
			LoadShader(separateCommands[1], device, bucket, separateCommands[2]);
		}
	}
}

void ShaderCompiler::Init(ID3D12Device* device, JobSystem* jobSys)
{
#ifdef NDEBUG
#define SHADER_MULTICORE_COMPILE	
#endif
	JobBucket* bucket = nullptr;
#ifdef SHADER_MULTICORE_COMPILE
	bucket = jobSys->GetJobBucket();
#endif
	mShaders.reserve(50);
	mComputeShaders.reserve(50);
	CompileShaders(device, bucket, "ShaderCompileList.inf");
	GetTonemapLut3DShader(device, bucket);
	GetHIZMipGeneratorShader(device, bucket);
	GetMipGeneratorShader(device, bucket);
	GetAutoExposure(device, bucket);
#ifdef SHADER_MULTICORE_COMPILE
	jobSys->ExecuteBucket(bucket, 1);
#endif
}

void ShaderCompiler::Dispose()
{
	mComputeShaders.clear();
	mShaders.clear();
}