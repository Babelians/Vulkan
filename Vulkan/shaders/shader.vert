#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform SceneData
{
	vec2 rectCenter;
}sceneData;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = vec4(sceneData.rectCenter + inPos, 0.0, 1.0);
	fragColor = inColor;
}