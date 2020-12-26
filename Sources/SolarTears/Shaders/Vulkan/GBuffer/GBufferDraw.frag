#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D ObjectTexture;

void main()
{
	vec4 texColor = texture(ObjectTexture, fragTexCoord);
	outColor      = texColor;
}