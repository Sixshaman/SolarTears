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

struct PerFrameData
{
	float4x4 ViewProjMatrix;
};

struct PerObjectData
{
	float4x4 WorldMatrix;
};

struct MaterialData
{
	uint TextureIndex;
	uint NormalMapIndex;
};

struct ObjectIndexData
{
	uint ObjectIndex;
};

struct MaterialIndexData
{
	uint MaterialIndex;
};