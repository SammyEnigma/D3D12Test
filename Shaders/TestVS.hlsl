struct FVSOut
{
	float4 Pos : SV_POSITION;
	float2 UVs : TEXCOORD0;
	float4 Color : COLOR;
};

#define USE_VIEW_UB	1
#if USE_VIEW_UB
cbuffer ViewUB : register(b0)
{
	float4x4 View;
	float4x4 Proj;
};
#endif

cbuffer ObjUB : register(b1)
{
	float4x4 Obj;
};

FVSOut Main(float3 Pos : POSITION, float2 UV : TEXCOORD0, uint VertexID : SV_VertexID)
{
	//float3 P[3] =
	//{
	//	float3(0, 0.4444, 0),
	//	float3(0.25, -0.4444, 0),
	//	float3(-0.25, -0.4444, 0),
	//};

	float3 P[3] =
	{
		float3(-0.5, -0.1, 1),
		float3(0.1, -0.1, 1),
		float3(0, 0.5, 1),
		//float3(-0.464, -0.173, 1),
		//float3(0.0928, -0.173, 1),
		//float3(0, 0.866, 1),
	};
	
	FVSOut Out = (FVSOut)0;
	//Out.Pos = float4(P[VertexID % 3], 1);
	Out.Pos = float4(Pos, 1);

	#if !USE_VIEW_UB
	#if 1
	float4x4 Proj =	
	{
		{0.928271, 0.0, 0.0, 0.0},
		{0.0, 1.73205090, 0.0, 0.0},
		{0.0, 0.0, 1.0001, 1},
		{0.0, 0.0, -0.10001, 0.0}
	};
	#else
	float4x4 Proj =	
	{
		{1, 0, 0 ,0},
		{0, 1, 0 ,0},
		{0, 0, 1, 0},
		{0.5, 0, 0, 1}
	};
	#endif
	#endif

	Out.Pos = mul(Out.Pos, Obj);
	
	#if USE_VIEW_UB
	Out.Pos = mul(View, Out.Pos);
	#else
	float4x4 View =	
	{
		{1, 0, 0 ,0},
		{0, 1, 0 ,0},
		{0, 0, 1, 0},
		{0, 0, 2.5, 1}
	};
	Out.Pos = mul(Out.Pos, View);
	#endif
	Out.Pos = mul(Proj, Out.Pos);
	Out.UVs = UV;
	
	return Out;
}
