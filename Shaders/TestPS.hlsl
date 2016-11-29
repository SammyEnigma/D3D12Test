struct FVSOut
{
	float4 Pos : SV_POSITION;
	float2 UVs : TEXCOORD0;
	float4 Color : COLOR;
};

SamplerState Sampler : register(s0);

Texture2D Texture : register(t0);

float4 Main(FVSOut In) : SV_Target0
{
	return float4(In.UVs.xy, 0, 1) + Texture.Sample(Sampler, In.UVs);
}
