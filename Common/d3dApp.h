//***************************************************************************************
// d3dApp.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once
#include "../IRendererBase.h"
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3DApp
{	
public:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	D3DAppDataPack dataPack;
	static constexpr DXGI_FORMAT BACK_BUFFER_FORMAT()
	{
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	static D3DApp* GetApp();

	int Run(IRendererBase*);

	bool Initialize();
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual ~D3DApp();
protected:
	void CreateRtvAndDsvDescriptorHeaps();
	void OnResize(D3DAppDataPack& pack);
	GameTimer timer;
	// Convenience overrides for handling mouse input.
	virtual void OnPressMinimizeKey(bool minimize) {};
protected:

	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:

	static D3DApp* mApp;

	
};

