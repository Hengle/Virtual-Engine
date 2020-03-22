#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
//#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "Common/GameTimer.h"
typedef DirectX::XMFLOAT2 float2;
typedef DirectX::XMFLOAT3 float3;
typedef DirectX::XMFLOAT4 float4;
typedef DirectX::XMUINT2 uint2;
typedef DirectX::XMUINT3 uint3;
typedef DirectX::XMUINT4 uint4;
typedef DirectX::XMINT2 int2;
typedef DirectX::XMINT3 int3;
typedef DirectX::XMINT4 int4;
typedef uint32_t uint;
typedef DirectX::XMFLOAT4X4 float4x4;
typedef DirectX::XMFLOAT3X3 float3x3;
typedef DirectX::XMFLOAT3X4 float3x4;
typedef DirectX::XMFLOAT4X3 float4x3;
class GameTimer;
class D3DAppDataPack;
struct D3DAppDataPack
{
	static constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr int SwapChainBufferCount = 2;
	static constexpr D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	HINSTANCE mhAppInst; // application instance handle
	bool      mAppPaused;  // is the application paused?
	bool      mMinimized;  // is the application minimized?
	bool      mMaximized;  // is the application maximized?
	bool      mResizing;   // are the resize bars being dragged?

	// Used to keep track of the Delta-time?and game time (?.4).
//	GameTimer mTimer;
	IDXGIFactory4* mdxgiFactory;
	IDXGISwapChain* mSwapChain;
	ID3D12Device* md3dDevice;
	ID3D12Fence* mFence;
	UINT64 mCurrentFence;
	ID3D12CommandQueue* mCommandQueue;
	int mCurrBackBuffer;
	ID3D12Resource* mSwapChainBuffer[SwapChainBufferCount];
	ID3D12DescriptorHeap* mRtvHeap;
	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize;
	UINT mDsvDescriptorSize;
	UINT mCbvSrvUavDescriptorSize;

	wchar_t* windowInfo;
	int mClientWidth;
	int mClientHeight;
	void(*flushCommandQueue)(D3DAppDataPack&);
	void(*disposeFunction)(D3DAppDataPack&);
	~D3DAppDataPack()
	{
		if (disposeFunction) disposeFunction(*this);
	}

};
struct IRendererBase
{
	virtual bool Initialize(D3DAppDataPack& pack) = 0;
	virtual void Dispose(D3DAppDataPack& pack) = 0;
	virtual void OnResize(D3DAppDataPack& pack) = 0;
	virtual bool Draw(D3DAppDataPack& pack, GameTimer&) = 0;
	virtual ~IRendererBase() {}
};