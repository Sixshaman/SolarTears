#include "GBufferInclude.hlsli"

//================================================================

ConstantBuffer<PerFrameData>  cbFrameData:    register(b0, space0);
ConstantBuffer<PerObjectData> cbObjectData[]: register(b1, space0);

ConstantBuffer<ObjectIndexData> cbObjectIndex: register(b0, space1);

//================================================================

VertexOut main(VertexIn vin)
{
	uint objectIndex = cbObjectIndex.ObjectIndex;

	float4 clipPos = float4(vin.LocalPosition, 1.0f);
	clipPos        = mul(clipPos, cbObjectData[objectIndex].WorldMatrix);
	clipPos        = mul(clipPos, cbFrameData.ViewProjMatrix);

	VertexOut vout;
	vout.ClipPos  = clipPos;
	vout.Texcoord = vin.LocalTexcoord;

	return vout;
}