#pragma once

struct Image
{
    static Image LoadFromFile(const char* pFilePath);
    static Image CreateEmptyImage(size_t bytes);

    static Image CreateResizedImage(const Image& img, unsigned TargetWidth, unsigned TargetHeight);
    inline static Image CreateHalfResolutionFromImage(const Image& img) { return CreateResizedImage(img, img.Width >> 1, img.Height >> 1); }

    bool SaveToDisk(const char* pStrPath) const;
    void Destroy();  // Destroy must be called following a LoadFromFile() to prevent memory leak

    bool IsValid() const { return pData != nullptr && Width != 0 && Height != 0; }
    bool IsHDR() const { return BytesPerPixel > 4; }
    size_t GetSizeInBytes() const { return BytesPerPixel * Width * Height; }

    static unsigned short CalculateMipLevelCount(unsigned __int64 w, unsigned __int64 h);
    unsigned short CalculateMipLevelCount() const { return CalculateMipLevelCount(this->Width, this->Height); };

    int Width;
    int Height;
    int BytesPerPixel = 0;
    void* pData = nullptr;
    float MaxLuminance = 0.0f;
};