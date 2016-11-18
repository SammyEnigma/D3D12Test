struct FVSOut
{
	float4 Pos : SV_POSITION;
	float2 UVs : TEXCOORD0;
	float4 Color : COLOR;
};

FVSOut Main(float3 Pos : POSITION, uint VertexID : SV_VertexID)
{
	float3 P[3] =
	{
		float3(0, 0.4444, 0),
		float3(0.25, -0.4444, 0),
		float3(-0.25, -0.4444, 0),
	};
	
	FVSOut Out = (FVSOut)0;
	Out.Pos = float4(P[VertexID % 3], 1);
	return Out;
}
