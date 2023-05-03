#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec3 gAlbedo;

layout (location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec2 texCoord;
} vOut;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    vec3 objColor;
} params;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

void main()
{
    gNormal = (Params.viewMat * vec4(vOut.wNorm, 0.0)).xyz;
    gPosition = (Params.viewMat * vec4(vOut.wPos, 1.0)).xyz;

    gAlbedo = params.objColor;
}