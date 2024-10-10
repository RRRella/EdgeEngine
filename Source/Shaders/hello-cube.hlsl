struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
	float2 uv : TEXCOORD0;
};
PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float2 uv : TEXCOORD0)
{
    PSInput result;
	float3 pos = position.xyz * 0.5f;
	result.position = float4(pos.x, pos.y, pos.z, 1.0f);
    result.color = color;
	result.uv = uv;
    return result;
}
float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(input.uv.x, input.uv.y, 0, 1);
}