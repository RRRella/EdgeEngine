Texture2D texSrc;
SamplerState Sampler;

struct PSInput
{
    float4 position : SV_POSITION;
	float2 uv       : TEXCOORD0;
};

PSInput VSMain(uint id : SV_VertexID)
{
    PSInput VSOut;

	// V0.pos = (-1, -1) | V0.uv = (0,  1)
	// V1.pos = ( 3, -1) | V1.uv = (2,  1)
	// V2.pos = (-1,  3) | V2.uv = (0, -1)
	VSOut.position.x = -1.0f + (float)((id & 1) << 2);
	VSOut.position.y = -1.0f + (float)((id & 2) << 1);
	VSOut.uv[0]       = 0.0f + (float)((id & 1) << 1);
	VSOut.uv[1]       = 1.0f - (float)((id & 2) << 0);
	
	VSOut.position.z = 0.0f;
	VSOut.position.w = 1.0f;
	return VSOut;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(texSrc.SampleLevel(Sampler, input.uv, 0).rgb, 1);
}
