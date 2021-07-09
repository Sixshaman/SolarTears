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

layout(set = 1, binding = 0) uniform FrameConstants
{
	mat4 ViewProjMatrix;
};

//================================================================

void main()
{
	outTexCoord = inTexCoord;

	gl_Position = ViewProjMatrix * vec4(inPosition, 1.0f);
}