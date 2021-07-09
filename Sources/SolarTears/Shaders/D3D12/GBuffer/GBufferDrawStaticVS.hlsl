#include "GBufferInclude.hlsli"

//================================================================

ConstantBuffer<PerFrameData> cbFrameData: register(b0, space0);

//================================================================

VertexOut main(VertexIn vin)
{
	float4 clipPos = float4(vin.LocalPosition, 1.0f);
	clipPos        = mul(clipPos, cbFrameData.ViewProjMatrix);

	VertexOut vout;
	vout.ClipPos  = clipPos;
	vout.Texcoord = vin.LocalTexcoord;

	return vout;
}