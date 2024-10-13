#pragma once

#include "Common.h"

#include <vector>

namespace D3D12MA { class Allocation; class Allocator; }

class UploadHeap;
class CBV_SRV_UAV;
class DSV;
struct D3D12_SHADER_RESOURCE_VIEW_DESC;

struct TextureCreateDesc
{
	TextureCreateDesc(const std::string& name) : TexName(name) {}
	ID3D12Device*         pDevice   = nullptr;
	D3D12MA::Allocator*   pAllocator = nullptr;
	UploadHeap*           pUploadHeap = nullptr;
	D3D12_RESOURCE_DESC   Desc = {};
	const std::string&    TexName;
};


class Texture
{
public:

	Texture()  = default;
	~Texture() = default;

	void CreateFromFile(const TextureCreateDesc& desc, const std::string& FilePath);
	void Create(const TextureCreateDesc& desc, const void* pData = nullptr);

	void Destroy();

	void InitializeSRV(uint32 index, CBV_SRV_UAV* pRV, D3D12_SHADER_RESOURCE_VIEW_DESC* pSRVDesc = nullptr);
	void InitializeDSV(uint32 index, DSV* pRV, int ArraySlice = 1);

public:

private:
	D3D12MA::Allocation* mpAlloc = nullptr;
	ID3D12Resource*      mpTexture = nullptr;
};