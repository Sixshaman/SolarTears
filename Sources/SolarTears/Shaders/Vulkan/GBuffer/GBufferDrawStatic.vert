#version 450

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

//================================================================

void main()
{
	outTexCoord = inTexCoord;

	gl_Position = SceneFrameData.ViewProjMatrix * vec4(inPosition, 1.0f);

	//The --invert-y option doesn't actually flip the coordinate system, it only flips the viewport
	gl_Position.y = -gl_Position.y;
}