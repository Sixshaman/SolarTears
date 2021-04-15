#include "GBufferInclude.hlsli"
#include "../CommonHeader.hlsli"

Texture2D gObjectTexture: register(t0);

SamplerState gSamplers[]: register(s0);

float4 main(VertexOut pin): SV_TARGET
{
	return gObjectTexture.Sample(gSamplers[LinearSamplerIndex], pin.Texcoord);
}