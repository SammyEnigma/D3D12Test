
RWTexture2D<float4> OutImage : register(u0);


[numthreads(8,8,1)]
void Main(uint3 gl_GlobalInvocationID : SV_DispatchThreadID)
{
	int QuadIndexX = int(gl_GlobalInvocationID.x);
	int QuadIndexY = int(gl_GlobalInvocationID.y);

	int NumSquares = 8;
	uint2 Block = gl_GlobalInvocationID.xy / uint2(NumSquares, NumSquares);
	uint BlockIndex = Block.y * NumSquares + Block.x;
	BlockIndex += Block.y & 1;
	float4 Color = float4(1.0, 1.0, 1.0, 1.0);
	if ((BlockIndex & 1) == 0)
	{
		Color = float4(0.0, 0.0, 0.0, 1.0);
	}
	//Color.xy = vec2(gl_GlobalInvocationID.xy) / vec2(imageSize(RWImage).xy);
	OutImage[gl_GlobalInvocationID.xy] = Color;
}
