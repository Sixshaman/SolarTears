#version 450

#extension GL_EXT_nonuniform_qualifier: require

//================================================================

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

//================================================================

layout(set = 3, binding = 0) uniform FrameConstants
{
	mat4 ViewProjMatrix;
} SceneFrameData;

layout(set = 3, binding = 1) uniform ObjectConstants
{
	mat4 ModelMatrix;
} SceneStaticObjectDatas[];

layout(push_constant) uniform ObjectPushConstants
{
	uint ObjectIndex;
} PushConstants;

//================================================================

void main()
{
	outTexCoord = inTexCoord;

	mat4 modelMatrix = SceneStaticObjectDatas[PushConstants.ObjectIndex].ModelMatrix;
	gl_Position = SceneFrameData.ViewProjMatrix * modelMatrix * vec4(inPosition, 1.0f);
}