#include "Texture.h"
#include "ResourceHeaps.h"
#include "ResourceViews.h"
#include "Libs/D3DX12/d3dx12.h"

#include "../../Libs/D3D12MemoryAllocator/include/D3D12MemAlloc.h"
#include "../Utils/Source/utils.h"
#include "../Utils/Source/Image.h"

#include <unordered_map>
#include <cassert>
 

//
// TEXTURE
//
void Texture::CreateFromFile(const TextureCreateDesc& tDesc, const std::string& FilePath)
{

    if (FilePath.empty())
    {
        Log::Error("Cannot create Texture from file: empty FilePath provided.");
        return;
    }

    // process file path
    const std::vector<std::string> FilePathTokens = StrUtil::split(FilePath, { '/', '\\' });
    assert(FilePathTokens.size() >= 1);

    const std::string& FileNameAndExtension = FilePathTokens.back();
    const std::vector<std::string> FileNameTokens = StrUtil::split(FileNameAndExtension, '.');
    assert(FileNameTokens.size() == 2);

    const std::string FileDirectory = FilePath.substr(0, FilePath.find(FileNameAndExtension));
    const std::string FileName = FileNameTokens.front();
    const std::string FileExtension = StrUtil::GetLowercased(FileNameTokens.back());

    const bool bHDR = FileExtension == "hdr";

    // load img
    Image image = Image::LoadFromFile(FilePath.c_str());
    assert(image.pData && image.BytesPerPixel > 0);

    TextureCreateDesc desc = tDesc;
    desc.Desc.Width  = image.Width;
    desc.Desc.Height = image.Height;
    desc.Desc.Format = bHDR ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
    //-------------------------------
    desc.Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Desc.Alignment = 0;
    desc.Desc.DepthOrArraySize = 1;
    desc.Desc.MipLevels = 1;
    desc.Desc.SampleDesc.Count = 1;
    desc.Desc.SampleDesc.Quality = 0;
    desc.Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    //-------------------------------
    Create(desc, image.pData);
    image.Destroy();
}

// TODO: clean up function
void Texture::Create(const TextureCreateDesc& desc, const void* pData /*= nullptr*/)
{
    HRESULT hr = {};

    ID3D12GraphicsCommandList* pCmd = desc.pUploadHeap->GetCommandList();

    const bool bDepthStencilTexture = desc.Desc.Format == DXGI_FORMAT_R32_TYPELESS;
    const bool bRenderTargetTexture = false;

    // determine resource state & optimal clear value
    D3D12_RESOURCE_STATES ResourceState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST;
    D3D12_CLEAR_VALUE* pClearValue = nullptr;
    if (bDepthStencilTexture)
    {
        D3D12_CLEAR_VALUE ClearValue = {};
        ResourceState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE;
        ClearValue.Format = (desc.Desc.Format == DXGI_FORMAT_R32_TYPELESS) ? DXGI_FORMAT_D32_FLOAT : desc.Desc.Format;
        ClearValue.DepthStencil.Depth = 1.0f;
        ClearValue.DepthStencil.Stencil = 0;
        pClearValue = &ClearValue;
    }
    if (bRenderTargetTexture)
    {
        D3D12_CLEAR_VALUE ClearValue = {};
        ResourceState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
        pClearValue = &ClearValue;
    }


    // Create resource
    D3D12MA::ALLOCATION_DESC textureAllocDesc = {};
    textureAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    hr = desc.pAllocator->CreateResource(
        &textureAllocDesc,
        &desc.Desc,
        ResourceState,
        pClearValue,
        &mpAlloc,
        IID_PPV_ARGS(&mpTexture));
    SetName(mpTexture, desc.TexName.c_str());
    if (FAILED(hr))
    {
        Log::Error("Couldn't create texture: ", desc.TexName.c_str());
        return;
    }

    // upload the data
    if (pData)
    {
        const UINT64 UploadBufferSize = GetRequiredIntermediateSize(mpTexture, 0, 1);

#if 1
        UINT8* pUploadBufferMem = desc.pUploadHeap->Suballocate(SIZE_T(UploadBufferSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (pUploadBufferMem == NULL)
        {

            // TODO:
            // We ran out of mem in the upload heap, flush it and try allocating mem from it again
#if 0
            desc.pUploadHeap->UploadToGPUAndWait();
            pUploadBufferMem = desc.pUploadHeap->Suballocate(SIZE_T(UploadBufferSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            assert(pUploadBufferMem);
#else
            assert(false);
#endif    
        }

        UINT64 UplHeapSize;
        uint32_t num_rows = {};
        UINT64 row_sizes_in_bytes = {};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTex2D = {};
        desc.pDevice->GetCopyableFootprints(&desc.Desc, 0, 1, 0, &placedTex2D, &num_rows, &row_sizes_in_bytes, &UplHeapSize);
        placedTex2D.Offset += UINT64(pUploadBufferMem - desc.pUploadHeap->BasePtr());

        // copy all the mip slices into the offsets specified by the footprint structure
        //

        for (uint32_t y = 0; y < num_rows; y++)
        {
            memcpy(pUploadBufferMem + y * placedTex2D.Footprint.RowPitch,
                (UINT8*)pData + y * row_sizes_in_bytes, row_sizes_in_bytes);
        }

        CD3DX12_TEXTURE_COPY_LOCATION Dst(mpTexture, 0);
        CD3DX12_TEXTURE_COPY_LOCATION Src(desc.pUploadHeap->GetResource(), placedTex2D);
        desc.pUploadHeap->GetCommandList()->CopyTextureRegion(&Dst, 0, 0, 0, &Src, NULL);

#else
        D3D12_SUBRESOURCE_DATA textureSubresourceData = {};
        textureSubresourceData.pData = pData;
        textureSubresourceData.RowPitch = imageBytesPerRow;
        textureSubresourceData.SlicePitch = imageBytesPerRow * desc.Desc.Height;

        UpdateSubresources(pCmd, mpTexture, textureUpload, 0, 0, 1, &textureSubresourceData);
#endif

        D3D12_RESOURCE_BARRIER textureBarrier = {};
        textureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        textureBarrier.Transition.pResource = mpTexture;
        textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        textureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        textureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        pCmd->ResourceBarrier(1, &textureBarrier);
    }
}


void Texture::Destroy()
{
    if (mpTexture)
    {
        mpTexture->Release(); 
        mpTexture = nullptr;
    }
    if (mpAlloc)
    {
        mpAlloc->Release();
        mpAlloc = nullptr;
    }
}




void Texture::InitializeSRV(uint32 index, CBV_SRV_UAV* pRV, D3D12_SHADER_RESOURCE_VIEW_DESC* pSRVDesc)
{
    ID3D12Device* pDevice;
    mpTexture->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

    pDevice->CreateShaderResourceView(mpTexture, pSRVDesc, pRV->GetCPUDescHandle(index));

    pDevice->Release();
}

void Texture::InitializeDSV(uint32 index, DSV* pRV, int ArraySlice /*= 1*/)
{
    ID3D12Device* pDevice;
    mpTexture->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
    D3D12_RESOURCE_DESC texDesc = mpTexture->GetDesc();

    D3D12_DEPTH_STENCIL_VIEW_DESC DSViewDesc = {};
    DSViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    if (texDesc.SampleDesc.Count == 1)
    {
        if (texDesc.DepthOrArraySize == 1)
        {
            DSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            DSViewDesc.Texture2D.MipSlice = 0;
        }
        else
        {
            DSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            DSViewDesc.Texture2DArray.MipSlice = 0;
            DSViewDesc.Texture2DArray.FirstArraySlice = ArraySlice;
            DSViewDesc.Texture2DArray.ArraySize = 1;// texDesc.DepthOrArraySize;
        }
    }
    else
    {
        DSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    }

    pDevice->CreateDepthStencilView(mpTexture, &DSViewDesc, pRV->GetCPUDescHandle(index));

    pDevice->Release();
}

#if 0
void Texture::CreateRTV(uint32_t index, RTV* pRV, D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc)
{
    ID3D12Device* pDevice;
    m_pResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

    pDevice->CreateRenderTargetView(m_pResource, pRtvDesc, pRV->GetCPUDescHandle(index));

    pDevice->Release();
}

void Texture::CreateUAV(uint32_t index, Texture* pCounterTex, CBV_SRV_UAV* pRV, D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc)
{
    ID3D12Device* pDevice;
    m_pResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

    pDevice->CreateUnorderedAccessView(m_pResource, pCounterTex ? pCounterTex->GetResource() : NULL, pUavDesc, pRV->GetCPUDescHandle(index));

    pDevice->Release();
}

void Texture::CreateRTV(uint32_t index, RTV* pRV, int mipLevel, int arraySize, int firstArraySlice)
{
    D3D12_RESOURCE_DESC texDesc = m_pResource->GetDesc();
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = texDesc.Format;
    if (texDesc.DepthOrArraySize == 1)
    {
        assert(arraySize == -1);
        assert(firstArraySlice == -1);
        if (texDesc.SampleDesc.Count == 1)
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = (mipLevel == -1) ? 0 : mipLevel;
        }
        else
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            assert(mipLevel == -1);
        }
    }
    else
    {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.ArraySize = arraySize;
        rtvDesc.Texture2DArray.FirstArraySlice = firstArraySlice;
        rtvDesc.Texture2DArray.MipSlice = (mipLevel == -1) ? 0 : mipLevel;
    }

    CreateRTV(index, pRV, &rtvDesc);
}
#endif

// ================================================================================================================================================

//
// STATIC
//

// from Microsoft's D3D12HelloTexture
std::vector<uint8> Texture::GenerateTexture_Checkerboard(uint Dimension)
{
    constexpr UINT TexturePixelSizeInBytes = 4; // byte/px
    const UINT TextureWidth = Dimension;
    const UINT TextureHeight = Dimension;

    const UINT rowPitch = TextureWidth * TexturePixelSizeInBytes;
    const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
    const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
    const UINT textureSize = rowPitch * TextureHeight;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += TexturePixelSizeInBytes)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n + 0] = 0x00;    // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n + 0] = 0xff;    // R
            pData[n + 1] = 0xff;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}


