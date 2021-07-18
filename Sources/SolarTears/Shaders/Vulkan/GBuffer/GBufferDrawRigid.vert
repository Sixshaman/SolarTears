#version 450

struct ObjectData
{
	mat4 ModelMatrix;
};

//================================================================

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

//================================================================

layout(set = 2, binding = 0) uniform FrameConstants
{
	mat4 ViewProjMatrix;
};

layout(set = 2, binding = 1) readonly buffer ObjectConstants
{
	ObjectData ObjectDatas[];
};

layout(push_constant) uniform ObjectPushConstants
{
	uint ObjectIndex;
} PushConstants;

//================================================================

void main()
{
	outTexCoord = inTexCoord;

	mat4 modelMatrix = ObjectDatas[PushConstants.ObjectIndex].ModelMatrix;
	gl_Position = ViewProjMatrix * modelMatrix * vec4(inPosition, 1.0f);
}