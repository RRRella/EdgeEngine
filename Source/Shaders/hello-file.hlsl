struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
	float2 uv : TEXCOORD0;
};

struct PerFrame
{
};
struct PerView
{
};
struct PerObject
{
};
cbuffer CBuffer : register(b0)
{
    float4x4 matModelViewProj;
}
Texture2D texColor;
SamplerState Sampler;

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD0)
{
    PSInput result;
    result.position = mul(matModelViewProj, float4(position,1.0f));
	result.uv = uv;
    return result;
}
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 ColorTex = texColor.SampleLevel(Sampler, input.uv, 0);
    
    return float4(ColorTex /* * input.color*/);
}