/*
struct FVSOut
{
	float4 Pos : SV_POSITION;
	float2 UVs : TEXCOORD0;
	float4 Color : COLOR;
}

Texture2D Texture;
SamplerState Sampler;
*/
float4 Main(/*FVSOut In*/) : SV_Target0
{
	return float4(1, 0.5, 0, 1);//Texture.Sample(Sampler, UVs);// * ColorIn;
}
