#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

layout(set = 1, binding = 0) uniform UniformBufferObject
{
	mat4 ModelMatrix;
};

layout(set = 1, binding = 1) uniform UniformBufferFrame
{
	mat4 ViewProjMatrix;
};

void main()
{
	outTexCoord = inTexCoord;

	gl_Position = ViewProjMatrix * ModelMatrix * vec4(inPosition, 1.0f);
}