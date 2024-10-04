struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
	float2 uv : TEXCOORD0;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float2 uv : TEXCOORD0)
{
    PSInput result;

    result.position = position;
    result.color = color;
	result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
