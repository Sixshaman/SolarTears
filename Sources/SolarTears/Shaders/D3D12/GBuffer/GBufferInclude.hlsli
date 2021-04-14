struct VertexIn
{
	float3 LocalPosition: POSITION;
	float3 LocalNormal:   NORMAL;
	float2 LocalTexcoord: TEXCOORD;
};

struct VertexOut
{
	float4 ClipPos:  SV_POSITION;
	float2 Texcoord: TEXCOORD;
};