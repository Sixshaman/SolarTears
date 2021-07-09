#include "GBufferInclude.hlsli"
#include "../CommonHeader.hlsli"

//================================================================

Texture2D gObjectTextures[]: register(t0);

SamplerState gSamplers[]: register(s0);

ConstantBuffer<MaterialData> cbMaterialData[]: register(b0, space0);

ConstantBuffer<MaterialIndexData> cbMaterialIndex: register(b0, space1);

//================================================================

float4 main(VertexOut pin): SV_TARGET
{
	MaterialData materialData = cbMaterialData[cbMaterialIndex.MaterialIndex];

	return gObjectTextures[materialData.TextureIndex].Sample(gSamplers[LinearSamplerIndex], pin.Texcoord);
}