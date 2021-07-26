#version 450

#extension GL_EXT_nonuniform_qualifier: require

struct MaterialData
{
	uint TextureIndex;
	uint NormalMapIndex;
};

const int LinearSamplerIndex      = 0;
const int AnisotropicSamplerIndex = 1;

//================================================================

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

//================================================================

layout(set = 0, binding = 0) uniform sampler Samplers[];

layout(set = 1, binding = 0) uniform MaterialConstants
{
	MaterialData Material;
} Materials[];

layout(set = 2, binding = 0) uniform texture2D ObjectTextures[];

layout(push_constant) uniform MaterialPushConstants
{
	uint MaterialIndex;
} PushConstants;

//================================================================

void main()
{
	MaterialData materialData = Materials[PushConstants.MaterialIndex].Material;

	vec4 texColor = texture(sampler2D(ObjectTextures[materialData.TextureIndex], Samplers[LinearSamplerIndex]), fragTexCoord);
	outColor      = texColor;
}