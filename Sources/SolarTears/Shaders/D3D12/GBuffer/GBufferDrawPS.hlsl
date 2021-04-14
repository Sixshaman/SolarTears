#include "GBufferInclude.hlsli"

Texture2D gObjectTexture: register(t0);

SamplerState gTextureSampler: register(s0);

float4 main(VertexOut pin): SV_TARGET
{
	return gObjectTexture.Sample(gTextureSampler, pin.Texcoord);
}