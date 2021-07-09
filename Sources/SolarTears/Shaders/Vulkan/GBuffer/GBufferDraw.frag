#version 450

#extension GL_EXT_nonuniform_qualifier: require

struct MaterialData
{
	uint TextureIndex;
	uint NormalMapIndex;
};

//================================================================

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

//================================================================

layout(set = 0, binding = 0) uniform sampler2D ObjectTextures[];

layout(set = 0, binding = 1) readonly buffer MaterialConstants
{
	MaterialData Materials[];
};

layout(push_constant) uniform MaterialPushConstants
{
	uint MaterialIndex;
} PushConstants;

//================================================================

void main()
{
	MaterialData materialData = Materials[PushConstants.MaterialIndex];

	vec4 texColor = texture(ObjectTextures[materialData.TextureIndex], fragTexCoord);
	outColor      = texColor;
}