#version 450
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint quadResolution;
    float minHeight;
    float maxHeight;
} params;

layout(binding = 1, set = 0) uniform noise_t
{
  NoiseParams noise;
};

layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 oPos;
} vOut;

void main(void)
{
    vec3 pos = vec3(gl_VertexIndex / 4, gl_VertexIndex / 2 % 2 == 1, (gl_VertexIndex % 2) != 0);
    vOut.oPos = (pos - vec3(0.5, 0.5, 0.5)) * 2;

    vOut.oPos.x *= noise.boxSize.x;
    vOut.oPos.y *= noise.boxSize.y;
    vOut.oPos.z *= noise.boxSize.z;

    vOut.wPos = (params.mModel * vec4(vOut.oPos, 1.0)).xyz;
    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}