
RWTexture2D<float4> InImage : register(u0);
RWTexture2D<float4> OutImage : register(u1);

[numthreads(8,8,1)]
void Main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID)
{
	int QuadIndexX = int(gl_GlobalInvocationID.x);
	int QuadIndexY = int(gl_GlobalInvocationID.y);
	if (
	//((gl_GlobalInvocationID.x % 4) == 0) ||
	((gl_GlobalInvocationID.y % 4) != 0)
	)
	{
		float4 Color0 = InImage[gl_GlobalInvocationID.xy + int2(-1, -1)] * 0.2;
		float4 Color1 = InImage[gl_GlobalInvocationID.xy + int2(-1, 1)] * 0.2;
		float4 Color2 = InImage[gl_GlobalInvocationID.xy + int2(0, 0)] * 0.2;
		float4 Color3 = InImage[gl_GlobalInvocationID.xy + int2(1, 1)] * 0.2;
		float4 Color4 = InImage[gl_GlobalInvocationID.xy + int2(1, -1)] * 0.2;
		float4 Color = Color0 + Color1 + Color2 + Color3 + Color4;
		OutImage[gl_GlobalInvocationID.xy] = Color;
	}
	else
	{
		float4 Color = 0;
		OutImage[gl_GlobalInvocationID.xy] = Color;
	}
}
