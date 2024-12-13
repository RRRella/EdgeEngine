#include "Renderer.h"
#include "Device.h"
#include "Texture.h"

#include "../Application/Window.h"

#include "../Utils/Source/Log.h"
#include "../Utils/Source/utils.h"
#include "../../Libs/D3D12MemoryAllocator/src/Common.h"
#include "../../Libs/D3D12MemoryAllocator/include/D3D12MemAlloc.h"

#include <cassert>
#include <atomic>

#include <wrl.h>
#include <D3Dcompiler.h>
#pragma comment(lib, "D3DCompiler.lib")

using namespace Microsoft::WRL;
using namespace SystemInfo;

#ifdef _DEBUG
	#define ENABLE_DEBUG_LAYER      1
	#define ENABLE_VALIDATION_LAYER 1
#else
	#define ENABLE_DEBUG_LAYER      0
	#define ENABLE_VALIDATION_LAYER 0
#endif


// ---------------------------------------------------------------------------------------
// D3D12MA Integration 
// ---------------------------------------------------------------------------------------
#define D3D12MA_ENABLE_CPU_ALLOCATION_CALLBACKS       1
#define D3D12MA_ENABLE_CPU_ALLOCATION_CALLBACKS_PRINT 1
static void* const         CUSTOM_ALLOCATION_USER_DATA = (void*)(uintptr_t)0xDEADC0DE;
static std::atomic<size_t> g_CpuAllocationCount{ 0 };
static void* CustomAllocate(size_t Size, size_t Alignment, void* pUserData)
{
	assert(pUserData == CUSTOM_ALLOCATION_USER_DATA);
	void* memory = _aligned_malloc(Size, Alignment);
	if (D3D12MA_ENABLE_CPU_ALLOCATION_CALLBACKS_PRINT)
	{
		wprintf(L"Allocate Size=%llu Alignment=%llu -> %p\n", Size, Alignment, memory);
	}
	++g_CpuAllocationCount;
	return memory;
}
static void CustomFree(void* pMemory, void* pUserData)
{
	assert(pUserData == CUSTOM_ALLOCATION_USER_DATA);
	if (pMemory)
	{
		--g_CpuAllocationCount;
		if (D3D12MA_ENABLE_CPU_ALLOCATION_CALLBACKS_PRINT)
		{
			wprintf(L"Free %p\n", pMemory);
		}
		_aligned_free(pMemory);
	}
}
// ---------------------------------------------------------------------------------------

//
// PUBLIC
//
void Renderer::Initialize(const FRendererInitializeParameters& params)
{
	Device* pEdgeDevice = &mDevice;
	const FGraphicsSettings& Settings = params.Settings;
	const int NUM_SWAPCHAIN_BUFFERS   = Settings.bUseTripleBuffering ? 3 : 2;

	mDefaultHeapDesc.Alignment = 0;
	mDefaultHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
	mDefaultHeapDesc.SizeInBytes = Settings.HeapDefaultSize;
	mDefaultHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	mDefaultHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	mDefaultHeapDesc.Properties.CreationNodeMask = 0;
	mDefaultHeapDesc.Properties.VisibleNodeMask = 0;
	mDefaultHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;


	// Create the device
	FDeviceCreateDesc deviceDesc = {};
	deviceDesc.bEnableDebugLayer = ENABLE_DEBUG_LAYER;
	deviceDesc.bEnableValidationLayer = ENABLE_VALIDATION_LAYER;
	mDevice.Create(deviceDesc);
	ID3D12Device* pDevice = pEdgeDevice->GetDevicePtr();


	// Create Command Queues of different types
	mGFXQueue.Create(pEdgeDevice, CommandQueue::ECommandQueueType::GFX);
	mComputeQueue.Create(pEdgeDevice, CommandQueue::ECommandQueueType::COMPUTE);
	mCopyQueue.Create(pEdgeDevice, CommandQueue::ECommandQueueType::COPY);

	// Initialize memory
	InitializeD3D12MA();
	InitializeHeaps();

	Log::Info("[Renderer] Initialized.");
	// TODO: Log system info
}

void Renderer::Load()
{
	mHeapUpload.UploadToGPUAndWait(mGFXQueue.pQueue);
}

void Renderer::Unload()
{

}

void Renderer::Exit()
{
	mHeapUpload.Destroy();
	mHeapCBV_SRV_UAV.Destroy();
	mHeapDSV.Destroy();
	
	for (std::unordered_map<TextureID, Texture>::iterator it = mTextures.begin(); it != mTextures.end(); ++it)
	{
		it->second.Destroy();
	}

	mTextures.clear();
	
	mpAllocator->Release();

	for (auto* p : mpBuiltinRootSignatures)
	{
		if (p) p->Release();
	}
	for (ID3D12PipelineState* pPSO : mpBuiltinPSOs)
	{
		if (pPSO)
			pPSO->Release();
	}

	for (std::unordered_map<HWND, FWindowRenderContext>::iterator it = mRenderContextLookup.begin(); it != mRenderContextLookup.end(); ++it)
	{
		auto& ctx = it->second;
		for (ID3D12CommandAllocator* pCmdAlloc : ctx.mCommandAllocatorsGFX)     if (pCmdAlloc) pCmdAlloc->Release();
		for (ID3D12CommandAllocator* pCmdAlloc : ctx.mCommandAllocatorsCompute) if (pCmdAlloc) pCmdAlloc->Release();
		for (ID3D12CommandAllocator* pCmdAlloc : ctx.mCommandAllocatorsCopy)    if (pCmdAlloc) pCmdAlloc->Release();
		ctx.mDynamicHeap_ConstantBuffer.Destroy();
		ctx.SwapChain.Destroy(); // syncs GPU before releasing resources
		ctx.PresentQueue.Destroy();
		if (ctx.pCmdList_GFX)
			ctx.pCmdList_GFX->Release();
	}


	mGFXQueue.Destroy();
	mComputeQueue.Destroy();
	mCopyQueue.Destroy();

	mDevice.Destroy();
}

void Renderer::OnWindowSizeChanged(HWND hwnd, unsigned w, unsigned h)
{
	if (!CheckContext(hwnd)) return;
	FWindowRenderContext& ctx = mRenderContextLookup.at(hwnd);

	ctx.MainRTResolutionX = w;
	ctx.MainRTResolutionY = h;
}

SwapChain& Renderer::GetWindowSwapChain(HWND hwnd) { return mRenderContextLookup.at(hwnd).SwapChain; }

short Renderer::GetSwapChainBackBufferCount(Window* pWnd) const { return pWnd ? this->GetSwapChainBackBufferCount(pWnd->GetHWND()) : 0; }
short Renderer::GetSwapChainBackBufferCount(HWND hwnd) const
{
	if (!CheckContext(hwnd)) return 0;

	const FWindowRenderContext& ctx = mRenderContextLookup.at(hwnd);
	return ctx.SwapChain.GetNumBackBuffers();
}

void Renderer::InitializeRenderContext(FWindowRepresentation& WndDesc, int NumSwapchainBuffers)
{
	Device* pVQDevice = &mDevice;
	ID3D12Device* pDevice = pVQDevice->GetDevicePtr();
	FWindowRenderContext ctx = {};
	ctx.pDevice = pVQDevice;
	ctx.PresentQueue.Create(ctx.pDevice, CommandQueue::ECommandQueueType::GFX); // Create the GFX queue for presenting the SwapChain
	ctx.bVsync = WndDesc.bVSync; // we store bVSync in ctx instead of SwapChain and provide it through SwapChain.Present() param : either way should be fine
	// Create the SwapChain
	FSwapChainCreateDesc swapChainDesc = {};
	swapChainDesc.numBackBuffers = NumSwapchainBuffers;
	swapChainDesc.pDevice = ctx.pDevice->GetDevicePtr();
	swapChainDesc.pWindow = &WndDesc;
	swapChainDesc.pCmdQueue = &ctx.PresentQueue;
	swapChainDesc.bVSync = ctx.bVsync;
	swapChainDesc.bFullscreen = WndDesc.bFullscreen;
	ctx.SwapChain.Create(swapChainDesc);
	// Create command allocators
	ctx.mCommandAllocatorsGFX.resize(NumSwapchainBuffers);
	ctx.mCommandAllocatorsCompute.resize(NumSwapchainBuffers);
	ctx.mCommandAllocatorsCopy.resize(NumSwapchainBuffers);
	for (int f = 0; f < NumSwapchainBuffers; ++f)
	{
		ID3D12CommandAllocator* pCmdAlloc = {};
		pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx.mCommandAllocatorsGFX[f]));
		pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&ctx.mCommandAllocatorsCompute[f]));
		pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&ctx.mCommandAllocatorsCopy[f]));
		SetName(ctx.mCommandAllocatorsGFX[f], "RenderContext::CmdAllocGFX[%d]", f);
		SetName(ctx.mCommandAllocatorsCompute[f], "RenderContext::CmdAllocCompute[%d]", f);
		SetName(ctx.mCommandAllocatorsCopy[f], "RenderContext::CmdAllocCopy[%d]", f);
	}
	// Create command lists
	pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx.mCommandAllocatorsGFX[0], nullptr, IID_PPV_ARGS(&ctx.pCmdList_GFX));
	ctx.pCmdList_GFX->SetName(L"RenderContext::CmdListGFX");
	ctx.pCmdList_GFX->Close();
	// Create dynamic buffer heap
	ctx.mDynamicHeap_ConstantBuffer.Create(pDevice, NumSwapchainBuffers, 16 * MEGABYTE);
	// Save other context data
	ctx.MainRTResolutionX = WndDesc.width;
	ctx.MainRTResolutionY = WndDesc.height;
	// save the render context
	this->mRenderContextLookup.emplace(WndDesc.hwnd, std::move(ctx));
}


bool Renderer::CheckContext(HWND hwnd) const
{
	if (mRenderContextLookup.find(hwnd) == mRenderContextLookup.end())
	{
		Log::Warning("Render Context not found for <hwnd=0x%x>", hwnd);
		return false;
	}
	return true;
}

FWindowRenderContext& Renderer::GetWindowRenderContext(HWND hwnd)
{
	if (!CheckContext(hwnd))
	{
		Log::Error("Renderer::GetWindowRenderContext(): Context not found!");
		//return FWindowRenderContext{};
	}
	return mRenderContextLookup.at(hwnd);
}

// ================================================================================================================================================
// ================================================================================================================================================
// ================================================================================================================================================



//
// PRIVATE
//
void Renderer::InitializeD3D12MA()
{
	// Initialize D3D12MA
	const D3D12MA::ALLOCATOR_FLAGS FlagAlloc = D3D12MA::ALLOCATOR_FLAG_NONE;

	D3D12MA::ALLOCATOR_DESC desc = {};
	desc.Flags = FlagAlloc;
	desc.pDevice = mDevice.GetDevicePtr();
	desc.pAdapter = mDevice.GetAdapterPtr();

	D3D12MA::ALLOCATION_CALLBACKS allocationCallbacks = {};
	if (D3D12MA_ENABLE_CPU_ALLOCATION_CALLBACKS)
	{
		allocationCallbacks.pAllocate = &CustomAllocate;
		allocationCallbacks.pFree = &CustomFree;
		allocationCallbacks.pPrivateData = CUSTOM_ALLOCATION_USER_DATA;
		desc.pAllocationCallbacks = &allocationCallbacks;
	}

	CHECK_HR(D3D12MA::CreateAllocator(&desc, &mpAllocator));

	switch (mpAllocator->GetD3D12Options().ResourceHeapTier)
	{
	case D3D12_RESOURCE_HEAP_TIER_1:
		wprintf(L"ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_1\n");
		break;
	case D3D12_RESOURCE_HEAP_TIER_2:
		wprintf(L"ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_2\n");
		break;
	default:
		assert(0);
	}
}

void Renderer::InitializeHeaps()
{
	ID3D12Device* pDevice = mDevice.GetDevicePtr();

	const uint32 UPLOAD_HEAP_SIZE = 256 * MEGABYTE; // TODO: from RendererSettings.ini
	mHeapUpload.Create(pDevice, UPLOAD_HEAP_SIZE);

	constexpr uint32 NumDescsCBV = 10;
	constexpr uint32 NumDescsSRV = 10;
	constexpr uint32 NumDescsUAV = 10;
	constexpr bool   bCPUVisible = false;
	mHeapCBV_SRV_UAV.Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumDescsCBV + NumDescsSRV + NumDescsUAV, bCPUVisible);
	mHeapImguiSRV.Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100000, bCPUVisible);


	constexpr uint32 NumDescsDSV = 10;
	mHeapDSV.Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, NumDescsDSV);

	constexpr uint32 STATIC_GEOMETRY_MEMORY_SIZE = 32 * MEGABYTE;
	constexpr bool USE_GPU_MEMORY = true;
}

static std::wstring GetAssetFullPath(LPCWSTR assetName)
{
	std::wstring fullPath = L"Shaders/";
	return fullPath + assetName;
}

void Renderer::LoadPSOs()
{
	ID3D12Device* pDevice = mDevice.GetDevicePtr();

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	constexpr char* strShadingModelVS = "vs_5_0";
	constexpr char* strShadingModelPS = "ps_5_0";
	// ROOT SIGNATURES - hardcoded for now. TODO: http://simonstechblog.blogspot.com/2019/06/d3d12-root-signature-management.html
	{
		// Hello-World-Triangle Root Signature : [0]
		{
			CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			ID3D12RootSignature* pRS = nullptr;
			ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRS)));
			mpBuiltinRootSignatures.push_back(pRS);
		}

		// Fullscreen-Triangle Root Signature : [1]
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
			// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}


			CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 rootParameters[1];
			rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.MipLODBias = 0;
			sampler.MaxAnisotropy = 0;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.ShaderRegister = 0;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
			ID3D12RootSignature* pRS = nullptr;
			ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRS)));
			mpBuiltinRootSignatures.push_back(pRS);
		}
	
	}


	// Hello-World-Cube Root Signature : [2]
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ID3D12RootSignature* pRS = nullptr;
		ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRS)));
		mpBuiltinRootSignatures.push_back(pRS);
	}

	// Hello-World-File Root Signature : [3]
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ID3D12RootSignature* pRS = nullptr;
		ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRS)));
		mpBuiltinRootSignatures.push_back(pRS);
	}

	// HELLO WORLD TRIANGLE PSO
	{
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

	ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"hello-triangle.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"hello-triangle.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, 0 , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR"   , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT      , 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = mpBuiltinRootSignatures[0];
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mpBuiltinPSOs[EBuiltinPSOs::HELLO_WORLD_TRIANGLE_PSO])));
	SetName(mpBuiltinPSOs[EBuiltinPSOs::HELLO_WORLD_TRIANGLE_PSO], "PSO_HelloWorld");
	}

	// FULLSCREEN TIRANGLE PSO
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;


		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"FullscreenTriangle.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"FullscreenTriangle.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));


		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { };
		psoDesc.pRootSignature = mpBuiltinRootSignatures[1];
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mpBuiltinPSOs[EBuiltinPSOs::LOADING_SCREEN_PSO])));
		SetName(mpBuiltinPSOs[EBuiltinPSOs::HELLO_WORLD_TRIANGLE_PSO], "PSO_LoadingScreen");
	}

	// HELLO CUBE PSO
	{

		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;
		ComPtr<ID3DBlob> errBlob;


		HRESULT hr = D3DCompileFromFile(GetAssetFullPath(L"hello-cube.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errBlob);
		if (FAILED(hr))
		{
			if (errBlob)
			{
				OutputDebugStringA((char*)errBlob->GetBufferPointer());
				errBlob->Release();
			}
		}
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"hello-cube.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errBlob));
		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, 0 , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR"   , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT      , 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = mpBuiltinRootSignatures[2];
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mpBuiltinPSOs[EBuiltinPSOs::HELLO_WORLD_CUBE_PSO])));
		SetName(mpBuiltinPSOs[EBuiltinPSOs::HELLO_WORLD_CUBE_PSO], "PSO_HelloCube");
	}

	// HELLO FILE PSO
	{

		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;
		ComPtr<ID3DBlob> errBlob;


		HRESULT hr = D3DCompileFromFile(GetAssetFullPath(L"hello-file.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errBlob);
		if (FAILED(hr))
		{
			if (errBlob)
			{
				OutputDebugStringA((char*)errBlob->GetBufferPointer());
				errBlob->Release();
			}
		}
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"hello-file.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errBlob));
		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, 0 , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT      , 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = mpBuiltinRootSignatures[3];
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mpBuiltinPSOs[EBuiltinPSOs::HELLO_WORLD_FILE_PSO])));
		SetName(mpBuiltinPSOs[EBuiltinPSOs::HELLO_WORLD_FILE_PSO], "PSO_HelloFile");
	}
}

void Renderer::LoadDefaultResources()
{
	
	
}


ID3D12DescriptorHeap* Renderer::GetDescHeap(EResourceHeapType HeapType)
{
	ID3D12DescriptorHeap* pHeap = nullptr;
	switch (HeapType)
	{
	case RTV_HEAP:          pHeap = mHeapRTV.GetHeap(); break;
	case DSV_HEAP:          pHeap = mHeapDSV.GetHeap(); break;
	case CBV_SRV_UAV_HEAP:  pHeap = mHeapCBV_SRV_UAV.GetHeap(); break;
	case SAMPLER_HEAP:      pHeap = mHeapSampler.GetHeap(); break;
	}
	return pHeap;
}

ID3D12DescriptorHeap* Renderer::GetImguiHeap()
{
	return mHeapImguiSRV.GetHeap();
}


// ================================================================================================================================================

//
// STATIC
//
/*
@bEnumerateSoftwareAdapters : Basic Render Driver adapter.
*/
std::vector< FGPUInfo > Renderer::EnumerateDX12Adapters(bool bEnableDebugLayer, bool bEnumerateSoftwareAdapters /*= false*/)
{
	std::vector< FGPUInfo > GPUs;
	HRESULT hr = {};

	IDXGIAdapter1* pAdapter = nullptr; // adapters are the graphics card (this includes the embedded graphics on the motherboard)
	int iAdapter = 0;                  // we'll start looking for DX12  compatible graphics devices starting at index 0
	bool bAdapterFound = false;        // set this to true when a good one was found

	// Chuck Walbourn: For DIrect3D 12, you can assume CreateDXGIFactory2 and IDXGIFactory4 or later is supported. 
	IDXGIFactory6* pDxgiFactory; // DXGIFactory6 supports preferences when querying devices: DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
	UINT DXGIFlags = 0;
	if (bEnableDebugLayer)
	{
		DXGIFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	hr = CreateDXGIFactory2(DXGIFlags, IID_PPV_ARGS(&pDxgiFactory));

	auto fnAddAdapter = [&bAdapterFound, &GPUs](IDXGIAdapter1*& pAdapter, const DXGI_ADAPTER_DESC1& desc, D3D_FEATURE_LEVEL FEATURE_LEVEL)
	{
		bAdapterFound = true;

		FGPUInfo GPUInfo = {};
		GPUInfo.DedicatedGPUMemory = desc.DedicatedVideoMemory;
		GPUInfo.DeviceID = desc.DeviceId;
		GPUInfo.DeviceName = StrUtil::UnicodeToASCII<_countof(desc.Description)>(desc.Description);
		GPUInfo.VendorID = desc.VendorId;
		GPUInfo.MaxSupportedFeatureLevel = FEATURE_LEVEL;
		pAdapter->QueryInterface(IID_PPV_ARGS(&GPUInfo.pAdapter));
		GPUs.push_back(GPUInfo);
	};

	// Find GPU with highest perf
	while (pDxgiFactory->EnumAdapterByGpuPreference(iAdapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter)) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1(&desc);

		const bool bSoftwareAdapter = desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;
		if ((bEnumerateSoftwareAdapters && !bSoftwareAdapter) // We want software adapters, but we got a hardware adapter
			|| (!bEnumerateSoftwareAdapters && bSoftwareAdapter) // We want hardware adapters, but we got a software adapter
			)
		{
			++iAdapter;
			pAdapter->Release();
			continue;
		}

		hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr))
		{
			fnAddAdapter(pAdapter, desc, D3D_FEATURE_LEVEL_12_1);
		}
		else
		{
			const std::string AdapterDesc = StrUtil::UnicodeToASCII(desc.Description);
			Log::Warning("Device::Create(): D3D12CreateDevice() with Feature Level 12_1 failed with adapter=%s, retrying with Feature Level 12_0", AdapterDesc.c_str());
			hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(hr))
			{
				fnAddAdapter(pAdapter, desc, D3D_FEATURE_LEVEL_12_0);
			}
			else
			{
				Log::Error("Device::Create(): D3D12CreateDevice() with Feature Level 12_0 failed ith adapter=%s", AdapterDesc.c_str());
			}
		}

		pAdapter->Release();
		++iAdapter;
	}
	pDxgiFactory->Release();
	assert(bAdapterFound);

	return GPUs;
}