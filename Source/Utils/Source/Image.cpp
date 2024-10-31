#include "Image.h"
#include "Log.h"
#include "utils.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include "../Libs/stb/stb_image.h"
#include "../Libs/stb/stb_image_write.h"
#include "../Libs/stb/stb_image_resize.h"

#include <vector>
#include <set>
#include <cmath>
#include <cassert>

static const std::set<std::string> S_HDR_FORMATS = { "hdr", "exr" };
static bool IsHDRFileExtension(const std::string& ext) { return S_HDR_FORMATS.find(ext) != S_HDR_FORMATS.end(); }

static float CalculateMaxLuminance(const float* pData, int width, int height, int numComponents)
{
    float MaxLuminance = 0.0f;
    
    for (int h = 0; h < height; ++h)
    {
        for (int w = 0; w < width; ++w)
        {
            const int pxIndex = (width * h + w) * numComponents;
            const float& r = pData[pxIndex];
            const float& g = pData[pxIndex + 1];
            const float& b = pData[pxIndex + 2];

            const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            if (lum > MaxLuminance)
                MaxLuminance = lum;
        }
    }

    return MaxLuminance;
}

Image Image::LoadFromFile(const char* pFilePath)
{
    constexpr int reqComp = 0;

    const std::string Extension = DirectoryUtil::GetFileExtension(pFilePath);

    const bool bHDR = IsHDRFileExtension(Extension);

    Image img;

    int NumImageComponents = 0;
    img.pData = bHDR
        ? (void*)stbi_loadf(pFilePath, &img.Width, &img.Height, &NumImageComponents, 4)
        : (void*)stbi_load(pFilePath, &img.Width, &img.Height, &NumImageComponents, 4);

    if (img.pData == nullptr)
    {
        Log::Error("Error loading file: %s", pFilePath);
    }
    img.BytesPerPixel = bHDR 
        ? NumImageComponents * 4 // HDR=RGBA32F -> 16 Bytes/Pixel = 4 Bytes / component
        : NumImageComponents;    // SDR=RGBA8   -> 4  Bytes/Pixel = 1 Byte  / component

    if (img.pData && bHDR)
    {
        img.MaxLuminance = CalculateMaxLuminance(static_cast<const float*>(img.pData), img.Width, img.Height, 4);
    }

    return img;
}

Image Image::CreateEmptyImage(size_t bytes)
{
    Image img;
    img.pData = (void*)malloc(bytes);
    return img;
}

Image Image::CreateResizedImage(const Image& img, unsigned TargetWidth, unsigned TargetHeight)
{
    assert(TargetWidth > 0 && TargetHeight > 0);
    const int TargetResolution = TargetHeight * TargetWidth;
    const int TargetImageSizeInBytes = TargetResolution * (img.IsHDR() ? 16 : 4); // HDR is 16bytes/px (RGBA32F), SDR is 4bytes/px (RGBA8)
    assert(TargetImageSizeInBytes > 0);

    // create downsample image
    Image NewImage = CreateEmptyImage(TargetImageSizeInBytes);

    // 'resize' input image
    int rc = 0;
    if (img.IsHDR())
    {
        const int NUM_CHANNELS = 4; // RGBA
        const int STRIDE_BYTES_INPUT = 0;
        const int STRIDE_BYTES_OUTPUT = 0;
        rc = stbir_resize_float(reinterpret_cast<const float*>(     img.pData),   img.Width,   img.Height, STRIDE_BYTES_INPUT,
                                reinterpret_cast<      float*>(NewImage.pData), TargetWidth, TargetHeight, STRIDE_BYTES_OUTPUT,
                                NUM_CHANNELS
        );
    }
    else
    {
        rc = -1;
        assert(false);
    }

    if (rc <= 0)
    {
        Log::Error("Error resizing image ");
    }
    else
    {
        NewImage.Width = TargetWidth;
        NewImage.Height = TargetHeight;
        NewImage.MaxLuminance = img.MaxLuminance;
        NewImage.BytesPerPixel = img.BytesPerPixel;
    }

    return NewImage;
}

bool Image::SaveToDisk(const char* pStrPath) const
{
    if (this->GetSizeInBytes() == 0)
    {
        Log::Warning("Image::SaveToDisk() failed: ImageSize=0 : %s", pStrPath);
        return false;
    }

    const std::string Folder = DirectoryUtil::GetFolderPath(pStrPath);
    DirectoryUtil::CreateFolderIfItDoesntExist(Folder);

    const std::string Extension = DirectoryUtil::GetFileExtension(pStrPath);
    const bool bHDR = IsHDRFileExtension(Extension);

    int rc = 0;
    if (bHDR)
    {
        const int comp = 4; // HDR is 4 bytes / component (RGBA32F)
        rc = stbi_write_hdr(pStrPath, this->Width, this->Height, comp, reinterpret_cast<const float*>(this->pData));
    }
    else
    {
        assert(false);
        const int comp = this->BytesPerPixel;
        rc = stbi_write_png(pStrPath, this->Width, this->Height, comp, this->pData, 0); 
    }

    return rc != 0; // 0 is err?
}

void Image::Destroy()
{ 
	if (pData) 
	{ 
		stbi_image_free(pData); 
		pData = nullptr; 
	} 
}

unsigned short Image::CalculateMipLevelCount(unsigned __int64 w, unsigned __int64 h)
{
    int mips = 0;
    while (w >= 1 && h >= 1)
    {
        ++mips;
        w >>= 1;
        h >>= 1;
    }
    return mips;
}
