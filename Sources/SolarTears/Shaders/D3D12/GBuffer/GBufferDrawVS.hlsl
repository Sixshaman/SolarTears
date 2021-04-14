#include "GBufferInclude.hlsli"

cbuffer cbPerObject: register(c0)
{
	float4x4 WorldMatrix;
}

cbuffer cbPerFrame: register(c1)
{
	float4x4 ViewProjMatrix;
}

VertexOut main(VertexIn vin)
{
	float4 clipPos = float4(vin.LocalPosition, 1.0f);
	clipPos        = mul(clipPos, WorldMatrix);
	clipPos        = mul(clipPos, ViewProjMatrix);

	VertexOut vout;
	vout.ClipPos  = clipPos;
	vout.Texcoord = vin.LocalTexcoord;

	return vout;
}